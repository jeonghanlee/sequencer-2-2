/* Copyright 2010 Helmholtz-Zentrum Berlin f. Materialien und Energie GmbH
   (see file Copyright.HZB included in this distribution)
*/
#include <stdlib.h>
#include <string.h>
#include "errlog.h"
#include "seq_queue.h"

struct seqQueue {
    int     nextPut;    /* count elements, not bytes */
    int     nextGet;    /* count elements, not bytes */
    int     numElems;
    int     elemSize;
    char    *buffer;
};

QUEUE seqQueueCreate(int numElems, int elemSize)
{
    QUEUE q = calloc(1,sizeof(struct seqQueue));
    if (!q) {
        errlogSevPrintf(errlogFatal, "queueCreate: out of memory\n");
        return 0;
    }
    q->elemSize = elemSize;
    q->numElems = numElems;
    q->buffer = calloc(numElems, elemSize);
    if (!q->buffer) {
        errlogSevPrintf(errlogFatal, "queueCreate: out of memory\n");
        return 0;
    }
    q->nextGet = q->nextPut = 0;
    return q;
}

void seqQueueDestroy(QUEUE q)
{
    free(q->buffer);
    free(q);
}

int seqQueueGet(QUEUE q, void *value)
{
    int nextGet = q->nextGet;
    int nextPut = q->nextPut;
    int numElems = q->numElems;
    int elemSize = q->elemSize;
    int empty = (nextGet == nextPut);

    if (!empty) {
        memcpy(value, q->buffer + nextGet * elemSize, elemSize);
        nextGet++;
        q->nextGet = (nextGet == numElems) ? 0 : nextGet;
    }
    return empty;
}

int seqQueuePut(QUEUE q, const void *value)
{
    int nextGet = q->nextGet;
    int nextPut = q->nextPut;
    int numElems = q->numElems;
    int elemSize = q->elemSize;
    int nextNextPut = (nextPut == numElems - 1) ? 0 : (nextPut + 1);
    int full = (nextNextPut == nextGet);

    memcpy(q->buffer + nextPut * elemSize, value, elemSize);
    if (!full) {
        q->nextPut = nextNextPut;
    }
    return full;
}

void seqQueueFlush(QUEUE q)
{
    q->nextGet = q->nextPut;
}

int seqQueueFree(const QUEUE q)
{
    int n;

    n = q->nextGet - q->nextPut - 1;
    if (n < 0)
        n += q->numElems;
    return n;
}

int seqQueueUsed(const QUEUE q)
{
    int n;

    n = q->nextPut - q->nextGet;
    if (n < 0)
        n += q->numElems;
    return n;
}

int seqQueueNumElems(const QUEUE q)
{
    return q->numElems;
}

int seqQueueElemSize(const QUEUE q)
{
    return q->elemSize;
}

int seqQueueIsEmpty(const QUEUE q)
{
    return (q->nextPut == q->nextGet);
}

int seqQueueIsFull(const QUEUE q)
{
    int n;

    n = (q->nextPut - q->nextGet) + 1;
    return (n == 0 || n == q->numElems);
}
