/*
 * netif.h
 *
 *  Created on: Apr 24, 2014
 *      Author: intern
 */

#ifndef NETIF_H_
#define NETIF_H_

#include <stdint.h>
#include "constants.h"
#include "adapter.h"

#define NETIF_FLAG_BROADCAST 0x02U
#define NETIF_FLAG_ETHARP 0x20U
#define NETIF_FLAG_LINK_UP 0x10U

#define ip_addr_set_zero(ipaddr) ((ipaddr)->addr = 0)
#define ip_addr_set(dest, src) ((dest)->addr = ((src) == NULL ? 0 : (src)->addr))

struct netif;

typedef void (*netif_init_fn)(struct netif *netif);
typedef void (*netif_input_fn)(void *buffer, struct netif *inp);
typedef void (*netif_output_fn)(struct netif *netif, void *buffer, uint32_t *ipaddr);
typedef void (*netif_linkoutput_fn)(struct netif *netif);

/*
 * Describes a network interface, or a connection between two devices.
 */
struct netif{
	struct netif *next;
	ip_addr_t ipaddr;
	ip_addr_t netmask;
	ip_addr_t gw;

	netif_input_fn input;
	netif_output_fn output;
	netif_linkoutput_fn linkoutput;
	void *state;
	uint16_t mtu;
	uint8_t hwaddr_len;
	uint8_t hwaddr[NETIF_MAX_HWADDR_LEN];
	uint8_t flags;
	char name[2];
	uint8_t num;
};

struct netif * netif_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
  ip_addr_t *gw, void *state, netif_init_fn init);
void netif_set_ipaddr(struct netif *netif, ip_addr_t *ipaddr);
void netif_set_netmask(struct netif *netif, ip_addr_t *netmask);
void netif_set_gw(struct netif *netif, ip_addr_t *gw);
void netif_set_addr(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw);

#endif /* NETIF_H_ */
