/*
 * netif.c
 *
 *  Created on: Apr 25, 2014
 *      Author: intern
 */

#include "netif.h"
#include "adapter.h"

struct netif *netif_list;

/*
 * struct netif * netif_add(): Add a network interface to the list of netifs.
 *
 * @param	netif is a pre-allocated netif structure.
 * @param	ipaddr is the IP address of the board.
 * @param	netmask is the netmask of the board.
 * @param	gw is the default gateway of the board.
 * @param	state contains opaque data passed to the new netif.
 * @param	init is the callback function that initializes the interface.
 *
 * @return	the added netif, or NULL if failed.
 *
 */

struct netif * netif_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
  ip_addr_t *gw, void *state, netif_init_fn init)
{
  static uint8_t netifnum = 0;

  /* reset new interface configuration state */
  ip_addr_set_zero(&netif->ipaddr);
  ip_addr_set_zero(&netif->netmask);
  ip_addr_set_zero(&netif->gw);
  netif->flags = 0;

  /* remember netif specific state information data */
  netif->state = state;
  netif->num = netifnum++;

  netif_set_addr(netif, ipaddr, netmask, gw);
  /* call user specified initialization function for netif */
  init(netif);

  /* add this netif to the list */
  netif->next = netif_list;
  netif_list = netif;

  return netif;
}

void netif_set_ipaddr(struct netif *netif, ip_addr_t *ipaddr){
	ip_addr_set(&(netif->ipaddr), ipaddr);
}
void netif_set_netmask(struct netif *netif, ip_addr_t *netmask){
	ip_addr_set(&(netif->netmask), netmask);
}
void netif_set_gw(struct netif *netif, ip_addr_t *gw){
  ip_addr_set(&(netif->gw), gw);
}

void netif_set_addr(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw){
  netif_set_ipaddr(netif, ipaddr);
  netif_set_netmask(netif, netmask);
  netif_set_gw(netif, gw);
}

