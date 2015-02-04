/*
 * adapter.h
 *
 *  Created on: Apr 24, 2014
 *      Author: intern
 */

#ifndef ADAPTER_H_
#define ADAPTER_H_

#include <stdlib.h>

#include "xemacps.h"
#include "structs.h"
#include "constants.h"
#include "xpqueue.h"
#include "netif.h"

#define ip_addr1(ipaddr) (((uint8_t*)(ipaddr))[0])
#define ip_addr2(ipaddr) (((uint8_t*)(ipaddr))[1])
#define ip_addr3(ipaddr) (((uint8_t*)(ipaddr))[2])
#define ip_addr4(ipaddr) (((uint8_t*)(ipaddr))[3])

#define IP4_ADDR(ipaddr, a,b,c,d) \
        (ipaddr)->addr = ((uint32_t)((d) & 0xff) << 24) | \
                         ((uint32_t)((c) & 0xff) << 16) | \
                         ((uint32_t)((b) & 0xff) << 8)  | \
                          (uint32_t)((a) & 0xff)

struct xtopology_t{
	unsigned emac_baseaddr;
	int emac_type;
	unsigned intc_baseaddr;
	unsigned intc_emac_intr;
	unsigned scugic_baseaddr;
	unsigned scugic_emac_intr;
};

struct pbuf{
	struct pbuf *next;
	void *payload;
	uint16_t tot_len;
	uint16_t len;
	uint8_t type;
	uint8_t flags;
	uint16_t ref;
};

struct xtopology_t xtopology[];

void setup_rx_bds(XEmacPs_BdRing *rxring);
void emacps_recv_handler (void *arg);
int is_tx_space_available(xemacpsif_s *xemacpsif);
void emacps_send_handler(void *arg);
void process_sent_bds(XEmacPs_BdRing *txring);
void SetUpSLCRDivisors(int mac_baseaddr, int speed);
unsigned Phy_Setup (XEmacPs *xemacpsp);
void init_dma(struct xemac_s *xemac);
int xtopology_find_index(unsigned base);
void init_emacps(xemacpsif_s *xemacps, struct netif *netif);
void setup_isr (struct xemac_s *xemac);
void xemacpsif_init(struct netif *netif);
struct netif * xemac_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
		ip_addr_t *gw, unsigned char *mac_ethernet_address, unsigned mac_baseaddr);

#endif /* ADAPTER_H_ */
