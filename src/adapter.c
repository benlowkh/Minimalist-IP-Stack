/*
 * adapter.c
 *
 *  Created on: Apr 24, 2014
 *      Author: intern
 */

#include "adapter.h"

XEmacPs_Config *mac_config;
struct netif *NetIf;
unsigned int link_speed = 100;
int xtopology_n_emacs = 1;

struct xemac_s *xemacs;
xemacpsif_s *xemacpsifs;

struct xtopology_t xtopology[] = {
	{
		0xE000B000,
		3,
		0x0,
		0x0,
		0xF8F00100,
		0x36
	},
};

int xtopology_find_index(unsigned base)
{
	int i;
	for (i = 0; i < xtopology_n_emacs; i++) {
		if (xtopology[i].emac_baseaddr == base)
			return i;
	}
	return -1;
}

/*
 * XEmacPs_Config *lookup_config(): Looks up the the device configuration of the board.
 *
 * @params 	mac_base is information about the device driver and hardware,
 * 		   	usually containing the MAC address.
 *
 * @return	Configuration of the device
 *
 */
XEmacPs_Config *lookup_config(unsigned mac_base){
	extern XEmacPs_Config XEmacPs_ConfigTable[];
	XEmacPs_Config *CfgPtr = NULL;
	int i;
	for (i = 0; i < XPAR_XEMACPS_NUM_INSTANCES; i++){
		if (XEmacPs_ConfigTable[i].BaseAddress == mac_base){
			CfgPtr = &XEmacPs_ConfigTable[i];
			break;
		}
	}
	return (CfgPtr);
}

/*
 * void init_emacps(): Initializes the emacps driver.
 *
 * @param	xemacps is the device driver struct that holds the driver instance, emacps.
 * @param	netif is the network interface on which the driver is operating.
 *
 * @return	None.
 *
 */

void init_emacps(xemacpsif_s *xemacps, struct netif *netif)
{
	unsigned mac_address = (unsigned)(netif->state);
	XEmacPs *xemacpsp;
	XEmacPs_Config *mac_config;
	int Status = XST_SUCCESS;

	/* obtain config of this emac */
	mac_config = lookup_config(mac_address);

	xemacpsp = &xemacps->emacps;

	/* set mac address */
	Status = XEmacPs_SetMacAddress(xemacpsp, (void*)(netif->hwaddr), 1);
	if (Status != XST_SUCCESS) {
		xil_printf("In %s:Emac Mac Address set failed...\r\n",__func__);
	}
	XEmacPs_SetMdioDivisor(xemacpsp, MDC_DIV_224);
	link_speed = Phy_Setup(xemacpsp);
	XEmacPs_SetOperatingSpeed(xemacpsp, link_speed);
	/* Setting the operating speed of the MAC needs a delay. */
	{
		volatile int wait;
		for (wait=0; wait < 20000; wait++);
	}
}

/*
 * void setup_isr(): Sets up the interrupt handlers of the stack, namely the send and receive handlers.
 *
 * @param	xemac is the struct holding information about the device driver.
 *
 * @return	None.
 */

void setup_isr (struct xemac_s *xemac)
{
	xemacpsif_s   *xemacpsif;
	xemacpsif = (xemacpsif_s *)(xemac->state);
	/*
	 * Setup callbacks
	 */

	XEmacPs_SetHandler(&xemacpsif->emacps, XEMACPS_HANDLER_DMASEND,
					(void *) emacps_send_handler,
					(void *) xemac);
	xil_printf("SETUP_ISR\n");
	XEmacPs_SetHandler(&xemacpsif->emacps, XEMACPS_HANDLER_DMARECV,
				    (void *) emacps_recv_handler,
				    (void *) xemac);
}

/*
 * static void low_level_output(): Initializes the xemac_s and xemacpsif_s global
 * 								   structs that the send function requires.
 *
 * @param	netif is the network interface on which the driver is operating.
 *
 * @return None.
 *
 */

static void low_level_output(struct netif *netif){

	xemacs = (struct xemac_s *)(netif->state);
	xemacpsifs = (xemacpsif_s *)(xemacs->state);
}

/*
 * static void low_level_init():	The main initialization function. Kickstarts the
 * 									program once initialization is complete.
 *
 * @param	netif is the network interface on which the driver is operating.
 *
 * @return	None.
 *
 */

static void low_level_init(struct netif *netif)
{
	unsigned mac_address = (unsigned)(netif->state);
	struct xemac_s *xemac;
	xemacpsif_s *xemacpsif;
	u32 dmacrreg;

	int Status = XST_SUCCESS;

	NetIf = netif;

	/* Mallocs the device driver structs for future repeated use whenever receiving packets. */
	xemacpsif = malloc(sizeof *xemacpsif);
	xemac = malloc(sizeof *xemac);

	xemac->state = (void *)xemacpsif;
	xemac->topology_index = xtopology_find_index(mac_address);
	xemac->type = xemac_type_emacps;

	xemacpsif->send_q = NULL;
	/* Initializes the receive queue from which incoming packets are first received. */
	xemacpsif->recv_q = pq_create_queue();

	/* maximum transfer unit */
	netif->mtu = XEMACPS_MTU - XEMACPS_HDR_SIZE;

	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
											NETIF_FLAG_LINK_UP;


	/* obtain config of this emac */
	mac_config = (XEmacPs_Config *)lookup_config((unsigned)netif->state);

	Status = XEmacPs_CfgInitialize(&xemacpsif->emacps, mac_config, mac_config->BaseAddress);

	if (Status != XST_SUCCESS) {
		xil_printf("In %s:EmacPs Configuration Failed....\r\n", __func__);
	}

	/* initialize the mac */
	init_emacps(xemacpsif, netif);

	dmacrreg = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_DMACR_OFFSET);
	dmacrreg = dmacrreg | (0x00000010);
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_DMACR_OFFSET, dmacrreg);

	/* Sets up the send and receive handlers */
	setup_isr(xemac);

	/* Initializes the stack's Direct Memory Access (DMA) */
	init_dma(xemac);

	/* Starts the drivers. From here onwards, the stack is done with initialization
	   and can send/receive packets. */
	XEmacPs_Start(&xemacpsif->emacps);

	/*
	 * replace the state in netif (currently the emac baseaddress)
	 * with the mac instance pointer.
	 */
	netif->state = (void *)xemac;
}

/*
 * void xemacpsif_init(): Called at the beginning of the program to set up the network interface.
 *
 * @param	netif is the network interface on which the driver is operating.
 *
 */

void xemacpsif_init(struct netif *netif){

	/* Function that handles the send mechanism at the data link layer should be called here */

	/* Sets the netif linkoutput function pointer to point
	   to low_level_output, but does not call it. */
	netif->linkoutput = low_level_output;

	/* Does the actual setup and initialization of the hardware. */
	low_level_init(netif);
}

/*
 * struct netif * xemac_add(): 	Wrapper function for netif_add. Sets the MAC address in
 * 								the netif struct and passes it to netif_add.
 *
 * @param	netif is the network interface defined by the rest of the parameters.
 * @param	ipaddr is the IP address of the board.
 * @param	netmask is the subnet mask of the board.
 * @param	gw is the default gateway of the board.
 * @param	mac_ethernet_address is the MAC address of the board.
 * @param	mac_baseaddr is the base address of the driver.
 *
 * @return	the added netif, or NULL if failed.
 *
 */

struct netif * xemac_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
		ip_addr_t *gw, unsigned char *mac_ethernet_address, unsigned mac_baseaddr)
{
	int i;
	/* set mac address */
	netif->hwaddr_len = 6;
	for (i = 0; i < 6; i++)
		netif->hwaddr[i] = mac_ethernet_address[i];

	return netif_add(netif, ipaddr, netmask, gw, (void*)mac_baseaddr, xemacpsif_init); //deleted 7th parameter, input
}

