#include <stdio.h>
#include <stdint.h>

#include "xparameters.h"
#include "structs.h"
#include "adapter.h"
#include "platform.h"

#define PLATFORM_EMAC_BASEADDR 0xE000B000

extern struct xemac_s *xemacs;
extern xemacpsif_s *xemacpsifs;
static int tx_pbufs_storage[XLWIP_CONFIG_N_RX_DESC];

#define XEMACPS_BD_TO_INDEX(ringptr, bdptr)	\
	(((u32)bdptr - (u32)(ringptr)->BaseBdAddr) / (ringptr)->Separation)

/* defined by each RAW mode application */
int start_application();
int transfer_data(){
	return 0;
}

/* missing declaration in lwIP */
void lwip_init();

void print_ip(char *msg, struct ip_addr *ip){

	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip_addr1(ip), ip_addr2(ip),
			ip_addr3(ip), ip_addr4(ip));
}

void print_ip_settings(struct ip_addr *ip, struct ip_addr *mask, struct ip_addr *gw){

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}

int main(){

	struct netif *netif, server_netif;
	struct ip_addr ipaddr, netmask, gw;

	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };
	netif = &server_netif;

	init_platform();

	/* initialize IP addresses to be used */
	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);

	print_ip_settings(&ipaddr, &netmask, &gw);

  	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}

	/*
	 	//Used to test the send function. Initializes an array of zeros to be sent and
	 	//calls the function pointer netif->linkoutput to initialize the drivers.

		int a[50];
		memset(a, 0, 50);
		struct pbuf *ar = (struct pbuf *)a;

		netif->linkoutput(netif);
	*/

	/*now enable interrupts*/
	platform_enable_interrupts();

	/* receive and process packets */
	while (1) {
		/* emacps_sgsend(ar, 50); */
		transfer_data();
	}

	/* never reached */
	cleanup_platform();

	return 0;
}
/*
 * void emacps_sgsend(): Sends the data onto the wire.
 *
 * @param	buffer is a buffer containing the data to be sent out.
 * @param	len is the length of the buffer.
 *
 * @return	None.
 *
 */
void emacps_sgsend(void *buffer, int len)
{
	uint8_t *q;
	int n_buffers;
	XEmacPs_Bd *txbdset, *txbd, *last_txbd = NULL;
	XEmacPs_Bd *temp_txbd;
	XStatus Status;
	XEmacPs_BdRing *txring;
	unsigned int BdIndex;
	unsigned int lev;

	lev = mfcpsr();
	mtcpsr(lev | 0x000000C0);

	uint8_t *payload;
	/* int i; */

	txring = &(XEmacPs_GetTxRing(&xemacpsifs->emacps));

	n_buffers = 1;

	/* allocates a buffer descriptor (BD) for the buffer */
	Status = XEmacPs_BdRingAlloc(txring, n_buffers, &txbdset);
	if (Status != XST_SUCCESS) {
		mtcpsr(lev);
	}

	/*casts the buffer into a printable format, uint8_t*/
	q = (uint8_t *)buffer;

	txbd = txbdset;
	BdIndex = XEMACPS_BD_TO_INDEX(txring, txbd);
	if (tx_pbufs_storage[BdIndex] != 0) {
		mtcpsr(lev);
	}

	payload = q;

	/* Send the data from the pbuf to the interface, one pbuf at a
	   time. The size of the data in each pbuf is kept in the ->len
	   variable. */
	Xil_DCacheFlushRange((unsigned int)payload, (unsigned)len);

	/*
		//Prints out the data. Comapre with Wireshark data to see if emacps_sgsend is functioning properly.

		for (i = 0; i < 50; i++){
			xil_printf("%x ", payload[i]);
		}
		xil_printf("\n");
	*/

	XEmacPs_BdSetAddressTx(txbd, (u32)payload);

	if (len > (XEMACPS_MAX_FRAME_SIZE - 18))
		XEmacPs_BdSetLength(txbd, (XEMACPS_MAX_FRAME_SIZE - 18) & 0x3FFF);
	else
		XEmacPs_BdSetLength(txbd, len & 0x3FFF);

	/*	Stores the buffer into tx_pbufs_storage to be freed from memory	*/
	tx_pbufs_storage[BdIndex] = (int)q;

	last_txbd = txbd;
	XEmacPs_BdClearLast(txbd);
	dsb();
 	txbd = XEmacPs_BdRingNext(txring, txbd);

	XEmacPs_BdSetLast(last_txbd);
	dsb();

	/* For fragmented packets, remember the 1st BD allocated for the 1st
	   packet fragment. The used bit for this BD should be cleared at the end
	   after clearing out used bits for other fragments. For packets without
	   just remember the allocated BD. */
	temp_txbd = txbdset;
	txbd = txbdset;
	txbd = XEmacPs_BdRingNext(txring, txbd);

	XEmacPs_BdClearTxUsed(temp_txbd);
	dsb();

	Status = XEmacPs_BdRingToHw(txring, n_buffers, txbdset);
	if (Status != XST_SUCCESS) {
		mtcpsr(lev);
	}
	dsb();

	/* Start transmit */
	XEmacPs_WriteReg((xemacpsifs->emacps).Config.BaseAddress,
	XEMACPS_NWCTRL_OFFSET,
	(XEmacPs_ReadReg((xemacpsifs->emacps).Config.BaseAddress,
	XEMACPS_NWCTRL_OFFSET) | XEMACPS_NWCTRL_STARTTX_MASK));
	dsb();
	mtcpsr(lev);
}
