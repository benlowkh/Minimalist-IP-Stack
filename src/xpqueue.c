#include <stdlib.h>

#include "xpqueue.h"

/* Creates a queue data structure. */
pq_queue_t * pq_create_queue(){

	pq_queue_t *q = (pq_queue_t *)malloc(sizeof(pq_queue_t));
	if (!q)
		return q;

	q->head = q->tail = q->len = 0;

	return q;
}

/* Enqueues a queue structure to a storage space. */
int pq_enqueue(pq_queue_t *q, void *p){

	if (q->len == PQ_QUEUE_SIZE)
		return -1;

	q->data[q->head] = p;
	q->head = (q->head + 1)%PQ_QUEUE_SIZE;
	q->len++;

	return 0;
}

/* Dequeues the data from a queue structure. */
void * pq_dequeue(pq_queue_t *q){

	int ptail;

	if (q->len == 0)
		return NULL;

	ptail = q->tail;
	q->tail = (q->tail + 1)%PQ_QUEUE_SIZE;
	q->len--;

	return q->data[ptail];
}

/* Returns the length of a queue. */
int pq_qlength(pq_queue_t *q){

	return q->len;
}
