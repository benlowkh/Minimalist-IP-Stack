/*
 * mem.h
 *
 *  Created on: May 14, 2014
 *      Author: intern
 */

#ifndef MEM_H_
#define MEM_H_

typedef enum{
	MEMP_POOL,
	MEMP_MAX
}memp_t;

struct memp{
	struct memp *next;
};

void * mem_malloc(memp_t type);
uint32_t * buffer_alloc(uint16_t length);

#endif /* MEM_H_ */
