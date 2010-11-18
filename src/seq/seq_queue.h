/* Copyright 2010 Helmholtz-Zentrum Berlin f. Materialien und Energie GmbH
   (see file Copyright.HZB included in this distribution)
*/
/* This module implements generic fifo queues, similar to (and largely
   inspired by) epicsRingBytes, but with a fixed element size and such that
   a put overwrites the last element if the queue is full. Put and get
   operations always work on a single element per call.

   Instead of wasting one element (which is cheap for bytes and pointers,
   but not for large elements like arrays of strings), we just don't move
   the nextPut index if the queue gets full, which, incidentally,
   implements the desired "overwrite last element if full" semantics.

   Restricting the interface to always put and get one element per call
   greatly simplifies the implementation, compared to epicsRingBytes.

   Note that, contrary to epicsRingBytes, seqQueueFlush can be called w/o
   locking out writers, it behaves as if we had called get until the queue
   is empty (only faster).
*/
#ifndef INCLseq_queueh
#define INCLseq_queueh

typedef struct seqQueue *QUEUE;

/* Create a new queue with the given element size and
   number of elements and return it, if successful,
   otherwise (out of memory) return NULL. */
QUEUE seqQueueCreate(unsigned numElems, unsigned elemSize);

/* A common precondition of the following operations is
   that their QUEUE argument is valid; they do not check
   this.
   
   A QUEUE is valid if it has been received as a
   non-NULL result of calling seqQueueCreate, and if
   seqQueueDelete has not been called for it.

   Note that seqQueueGet returns FALSE on success and
   that seqQueuePut returns FALSE if no element was
   overwritten.
*/

/* Destroy the queue, freeing all memory. */
void seqQueueDestroy(QUEUE q);
/* Get an element from the queue if possible. Return
   whether the queue was empty and therefore no
   element could be copied. */
boolean seqQueueGet(QUEUE q, void *value);
/* Put an element into the queue. Return whether the
   queue was full and therefore its last element was
   overwritten. */
boolean seqQueuePut(QUEUE q, const void *value);
/* Remove all elements. Cheap. */
void seqQueueFlush(QUEUE q);
/* How many free elements are left. */
boolean seqQueueFree(const QUEUE q);
/* How many elements are used up. */
boolean seqQueueUsed(const QUEUE q);
/* Number of elements (fixed on construction). */
unsigned seqQueueNumElems(const QUEUE q);
/* Element size (fixed on construction). */
unsigned seqQueueElemSize(const QUEUE q);
/* Whether empty, same as seqQueueUsed(q)==0 */
boolean seqQueueIsEmpty(const QUEUE q);
/* Whether full, same as seqQueueFree(q)==0 */
boolean seqQueueIsFull(const QUEUE q);

#endif /* INCLseq_queueh */
