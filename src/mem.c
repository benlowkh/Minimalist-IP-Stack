/*
 * mem.c
 *
 *  Created on: May 14, 2014
 *      Author: intern
 */
#include <stdint.h>
#include "mem.h"

#define MEMP_SIZE 0
#define MEM_ALIGNMENT 64
#define POOL_BUFSIZE 1700
#define LWIP_MEM_ALIGN(addr) ((void *)(((uint32_t)(addr) + MEM_ALIGNMENT - 1) & ~(uint32_t)(MEM_ALIGNMENT - 1)))
#define LWIP_MEM_ALIGN_SIZE(size) (((size) + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT - 1))
#define SIZEOF_BUFFER LWIP_MEM_ALIGN_SIZE(sizeof(uint32_t ))
#define POOL_BUFSIZE_ALIGNED LWIP_MEM_ALIGN_SIZE(POOL_BUFSIZE)
#define LWIP_MIN(x, y) (((x) < (y)) ? (x) : (y))

static struct memp *memp_tab[MEMP_MAX];

/* Custom malloc function. */
void * mem_malloc(memp_t type){
	struct memp *memp;
	memp = memp_tab[type];
	if (memp != 0){
		memp_tab[type] = memp->next;
		memp = (struct memp*)(void*)((uint8_t*)memp + MEMP_SIZE);
	}
	return memp;
}

/* Allocates a packet-sized buffer to memory */
uint32_t * buffer_alloc(uint16_t length){
	uint32_t *p, *q, *r;
	uint16_t offset;
	uint16_t plen, qlen;
	long rem_len;

	offset = 0;

	p = (uint32_t *)mem_malloc(MEMP_POOL);
	p = LWIP_MEM_ALIGN((void *)((uint8_t *)p + (SIZEOF_BUFFER + offset)));
	plen = LWIP_MIN(length, POOL_BUFSIZE_ALIGNED - LWIP_MEM_ALIGN_SIZE(offset));

	r = p;
	rem_len = length - plen;
	while (rem_len > 0){
		q = (uint32_t *)mem_malloc(MEMP_POOL);
		qlen = LWIP_MIN((uint16_t)rem_len, POOL_BUFSIZE_ALIGNED);
		q = (void *)((uint8_t *)q + SIZEOF_BUFFER);
		rem_len -= qlen;
		r = q;
	}
	return p;
}
