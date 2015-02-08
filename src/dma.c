#include <stdio.h>
#include "adapter.h"
#include "xil_exception.h"
#include "xscugic_hw.h"
#include "xil_mmu.h"
#include "xil_cache.h"
#include "mem.h"

#define INTC_BASE_ADDR XPAR_SCUGIC_CPU_BASEADDR
#define INTC_DIST_BASE_ADDR	XPAR_SCUGIC_DIST_BASEADDR

#define XEMACPS_BD_TO_INDEX(ringptr, bdptr)	\
	(((u32)bdptr - (u32)(ringptr)->BaseBdAddr) / (ringptr)->Separation)
#define BD_ALIGNMENT (XEMACPS_DMABD_MINIMUM_ALIGNMENT*2)

static int tx_pbufs_storage[XLWIP_CONFIG_N_RX_DESC];
static int rx_pbufs_storage[XLWIP_CONFIG_N_RX_DESC];

static int EmacIntrNum;
extern u8 _end;

/*
 * void setup_rx_bds(): Sets up the buffer descriptors (BDs) for buffer in the receive handler.
 *
 * @param	rxring is the BD ring that contains information that the DMA uses.
 *
 * @return	None.
 *
 */
void setup_rx_bds(XEmacPs_BdRing *rxring){

	XEmacPs_Bd *rxbd;
	unsigned int FreeBds;
	unsigned int BdIndex;
	unsigned int *Temp;
	uint32_t *buffer;

	FreeBds = XEmacPs_BdRingGetFreeCnt (rxring);
	while (FreeBds > 0) {
		FreeBds--;
		XEmacPs_BdRingAlloc(rxring, 1, &rxbd);
		BdIndex = XEMACPS_BD_TO_INDEX(rxring, rxbd);
		Temp = (unsigned int *)rxbd;
		*Temp = 0;
		if (BdIndex == (XLWIP_CONFIG_N_RX_DESC - 1)) {
			*Temp = 0x00000002;
		}
		Temp++;
		*Temp = 0;

		buffer = buffer_alloc(XEMACPS_MAX_FRAME_SIZE);
		if (!buffer){
			XEmacPs_BdRingUnAlloc(rxring, 1, rxbd);
			dsb();
			return;
		}
		XEmacPs_BdSetAddressRx(rxbd, (u32)buffer);
		dsb();

		rx_pbufs_storage[BdIndex] = (int)buffer;
		XEmacPs_BdRingToHw(rxring, 1, rxbd);
	}
}

/*
 * void emacps_recv_handler(): Callback function that handles data received by the driver.
 *
 * @param	arg is the network interface containing the driver information that contains
 * 			the data received by the driver.
 *
 * @return	None.
 *
 */
void emacps_recv_handler(void *arg){

	XEmacPs_Bd *rxbdset, *CurBdPtr;
	struct xemac_s *xemac;
	xemacpsif_s *xemacpsif;
	XEmacPs_BdRing *rxring;
	volatile int bd_processed;
	int rx_bytes, k;			//rx_bytes used only if realloc is implemented. See comment below.
	unsigned int BdIndex;
	unsigned int regval;
	/* int i; */	//Used to loop through the received data and print it out. Uncomment when testing.
	uint32_t *buffer;		// Will contain the payload of the received data

	/*
	 * Casts arg to the xemac_s and xemacpsif_s structs. The same structs as
	 * in the initialization, but now xemacpsif->recv_q contains the received data.
	 */
	xemac = (struct xemac_s *)(arg);
	xemacpsif = (xemacpsif_s *)(xemac->state);
	rxring = &XEmacPs_GetRxRing(&xemacpsif->emacps);

	/*
	 * If Reception done interrupt is asserted, call RX call back function
	 * to handle the processed BDs and then raise the according flag.
	 */

	regval = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_RXSR_OFFSET);
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_RXSR_OFFSET, regval);

	/* infinite loop, so the stack is always ready to receive data */
	while(1) {

		bd_processed = XEmacPs_BdRingFromHwRx(rxring, XLWIP_CONFIG_N_RX_DESC, &rxbdset);

		if (bd_processed <= 0) {
			break;
		}

		for (k = 0, CurBdPtr=rxbdset; k < bd_processed; k++) {

			BdIndex = XEMACPS_BD_TO_INDEX(rxring, CurBdPtr);

			/* buffer now points to the reserved buffer space malloced in init_dma. */
			buffer = (void *)rx_pbufs_storage[BdIndex];

			/* If buffer_realloc is implemented, the function should realloc the buffer to rx_bytes. */
			rx_bytes = XEmacPs_BdGetLength(CurBdPtr);

			Xil_DCacheInvalidateRange((unsigned int)buffer, (unsigned)XEMACPS_MAX_FRAME_SIZE);

			/*
			 * store it in the receive queue,
		 	 * where it'll be processed by a different handler
		 	 */
			pq_enqueue(xemacpsif->recv_q, (void*)buffer);

			/* 	//Prints out the data. Comapre with Wireshark data to see if emacps_sgsend is functioning properly.

				xil_printf("%d ", BdIndex);
				uint32_t *var = (uint32_t *)buffer;
				for (i = 0; i < 100; i++){
					xil_printf("%x ", var[i]);
				}
				xil_printf("\n");
			*/

			CurBdPtr = XEmacPs_BdRingNext(rxring, CurBdPtr);
		}
		/* free up the BD's */
		XEmacPs_BdRingFree(rxring, bd_processed, rxbdset);
		setup_rx_bds(rxring);
	}
	return;
}

/*
 * void init_dma(): Sets up the buffer descriptor and interrupt functionality in the DMA.
 *
 * @param	xemac is the driver struct that holds driver information useful to the DMA.
 *
 * @return	None.
 */
void init_dma(struct xemac_s *xemac)
{
	XEmacPs_Bd BdTemplate;
	XEmacPs_BdRing *RxRingPtr, *TxRingPtr;
	XEmacPs_Bd *rxbd;
	int i;
	unsigned int BdIndex;
	char *endAdd = &_end;
	uint32_t *buffer;			// Will contain the payload of the received data

	/*
	 * Align the BD start address to 1 MB boundary.
	 */
	char *endAdd_aligned = (char *)(((int)endAdd + 0x100000) & (~0xFFFFF));
	xemacpsif_s *xemacpsif = (xemacpsif_s *)(xemac->state);
	struct xtopology_t *xtopologyp = &xtopology[xemac->topology_index];

	/*
	 * The BDs need to be allocated in uncached memory. Hence the 1 MB
	 * address range that starts at address 0xFF00000 is made uncached
	 * by setting appropriate attributes in the translation table.
	 */
	Xil_SetTlbAttributes((int)endAdd_aligned, 0xc02); // addr, attr

	/* Points to the respective BD ring for the receive and send mechanisms. */
	RxRingPtr = &XEmacPs_GetRxRing(&xemacpsif->emacps);
	TxRingPtr = &XEmacPs_GetTxRing(&xemacpsif->emacps);

	xemacpsif->rx_bdspace = (void *)endAdd_aligned;
	/*
	 * We allocate 65536 bytes for Rx BDs which can accomodate a
	 * maximum of 8192 BDs which is much more than any application
	 * will ever need.
	 */
	xemacpsif->tx_bdspace = (void *)(endAdd_aligned + 0x10000);

	if (!xemacpsif->rx_bdspace || !xemacpsif->tx_bdspace) {
		xil_printf("%s@%d: Error: Unable to allocate memory for TX/RX buffer descriptors",
				__FILE__, __LINE__);
	}

	/*
	 * Setup RxBD space.
	 *
	 * Setup a BD template for the Rx channel. This template will be copied to
	 * every RxBD. We will not have to explicitly set these again.
	 */
	XEmacPs_BdClear(&BdTemplate);

	/*
	 * Create the RxBD ring
	 */
	XEmacPs_BdRingCreate(RxRingPtr, (u32) xemacpsif->rx_bdspace, (u32) xemacpsif->rx_bdspace, BD_ALIGNMENT, XLWIP_CONFIG_N_RX_DESC);
	XEmacPs_BdRingClone(RxRingPtr, &BdTemplate, XEMACPS_RECV);
	XEmacPs_BdClear(&BdTemplate);
	XEmacPs_BdSetStatus(&BdTemplate, XEMACPS_TXBUF_USED_MASK);

	/*
	 * Create the TxBD ring
	 */
	XEmacPs_BdRingCreate(TxRingPtr, (u32) xemacpsif->tx_bdspace, (u32) xemacpsif->tx_bdspace, BD_ALIGNMENT, XLWIP_CONFIG_N_TX_DESC);

	/* We reuse the bd template, as the same one will work for both rx and tx. */
	XEmacPs_BdRingClone(TxRingPtr, &BdTemplate, XEMACPS_SEND);

	/*
	 * Allocate RX descriptors, 1 RxBD at a time.
	 */
	for (i = 0; i < XLWIP_CONFIG_N_RX_DESC; i++) {
		XEmacPs_BdRingAlloc(RxRingPtr, 1, &rxbd);

		/* Allocates storage space in memory for the received packet to be placed */
		buffer = buffer_alloc(XEMACPS_MAX_FRAME_SIZE);

		XEmacPs_BdSetAddressRx(rxbd, (u32)buffer);

		BdIndex = XEMACPS_BD_TO_INDEX(RxRingPtr, rxbd);

		/* Sets the global storage array to point to the malloced buffer */
		rx_pbufs_storage[BdIndex] = (int)buffer;
		/* Enqueue to HW */
		XEmacPs_BdRingToHw(RxRingPtr, 1, rxbd);
	}

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	XScuGic_RegisterHandler(INTC_BASE_ADDR, xtopologyp->scugic_emac_intr,
				(Xil_ExceptionHandler)XEmacPs_IntrHandler, (void *)&xemacpsif->emacps);

	/*
	 * Enable the interrupt for emacps.
	 */
	XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, (u32) xtopologyp->scugic_emac_intr);
	EmacIntrNum = (u32) xtopologyp->scugic_emac_intr;
}

/*
 * void process_sent_bds(): Process the buffer descriptors once the packet has been sent.
 *
 * @param	txring is the transmission BD ring used to send the packet in emacps_sgsend.
 *
 * @return	None.
 *
 */
void process_sent_bds(XEmacPs_BdRing *txring){
	XEmacPs_Bd *txbdset;
	XEmacPs_Bd *CurBdPntr;
	int n_bds;
	int n_pbufs_freed = 0;
	unsigned int BdIndex;
	unsigned int *Temp;
	uint32_t *buffer;

	while(1){
		n_bds = XEmacPs_BdRingFromHwTx(txring, XLWIP_CONFIG_N_TX_DESC, &txbdset);
		if (n_bds == 0){
			return;
		}
		n_pbufs_freed = n_bds;
		CurBdPntr = txbdset;
		while (n_pbufs_freed > 0){
			BdIndex = XEMACPS_BD_TO_INDEX(txring, CurBdPntr);
			Temp = (unsigned int *)CurBdPntr;
			*Temp = 0;
			Temp++;
			*Temp = 0x80000000;
			if (BdIndex == (XLWIP_CONFIG_N_TX_DESC - 1)){
				*Temp = 0xC0000000;
			}

			/* Frees whatever is contained inside the global storage array. */
			buffer = (uint32_t *)tx_pbufs_storage[BdIndex];
			free(buffer);

			tx_pbufs_storage[BdIndex] = 0;
			CurBdPntr = XEmacPs_BdRingNext(txring, CurBdPntr);
			n_pbufs_freed--;
			dsb();
		}
		XEmacPs_BdRingFree(txring, n_bds, txbdset);
	}
	return;
}

/*
 * void emacps_send_handler():	Callback function to handle the sending of the data.
 * 								Works in conjunction with emacps_sgsend.
 *
 * @param	arg is the network interface containing the driver information that contains
 * 			the data received by the driver.
 *
 * @return	None.
 *
 */
void emacps_send_handler(void *arg){
	struct xemac_s *xemac;
	xemacpsif_s *xemacpsif;
	XEmacPs_BdRing *TxRingPtr;
	unsigned int regval;

	xemac = (struct xemac_s *)(arg);
	xemacpsif = (xemacpsif_s *)(xemac->state);
	TxRingPtr = &(XEmacPs_GetTxRing(&xemacpsif->emacps));
	regval = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_TXSR_OFFSET);
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_TXSR_OFFSET, regval);

	/* After the data is sent, the BDs are processed and freed up. */
	process_sent_bds(TxRingPtr);
}
