#include <stdint.h>
#include "xemacps.h"
#include "xpqueue.h"

#ifndef STRUCTS_H_
#define STRUCTS_H_

enum xemac_types {
	xemac_type_unknown = -1,
	xemac_type_emacps
};

/*
 * Device driver struct that encapsulates the xemacpsif_s struct.
 */
struct xemac_s{
	enum xemac_types type;
	int topology_index;
	void *state;
};

/*
 * Device driver struct contained within xemac_s. Contains the driver
 * instance and the queue where the data is received by the driver.
 */
typedef struct{
	XEmacPs emacps;
	pq_queue_t *recv_q;
	pq_queue_t *send_q;
	void *rx_bdspace;
	void *tx_bdspace;
}xemacpsif_s;

/*
 * Struct used to represent an IP address.
 */
struct ip_addr{
	uint32_t addr;
};

typedef struct ip_addr ip_addr_t;

#endif /* STRUCTS_H_ */
