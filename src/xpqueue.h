#ifndef __LWIP_PBUF_QUEUE_H_
#define __LWIP_PBUF_QUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define PQ_QUEUE_SIZE 128

/*
 * Describes a queue structure that the device driver
 * uses to store incoming data.
 */
typedef struct {
	void *data[PQ_QUEUE_SIZE];
	int head, tail, len;
} pq_queue_t;

pq_queue_t*	pq_create_queue();
int 		pq_enqueue(pq_queue_t *q, void *p);
void*		pq_dequeue(pq_queue_t *q);
int		pq_qlength(pq_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif
