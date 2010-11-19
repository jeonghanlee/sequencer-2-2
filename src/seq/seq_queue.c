/* Copyright 2010 Helmholtz-Zentrum Berlin f. Materialien und Energie GmbH
   (see file Copyright.HZB included in this distribution)
*/
#include "seq.h"

struct seqQueue {
    int     nextPut;    /* count elements, not bytes */
    int     nextGet;    /* count elements, not bytes */
    int     numElems;
    int     elemSize;
    char    *buffer;
};

QUEUE seqQueueCreate(unsigned numElems, unsigned elemSize)
{
    QUEUE q = (QUEUE)calloc(1,sizeof(struct seqQueue));
    if (!q) {
        errlogSevPrintf(errlogFatal, "queueCreate: out of memory\n");
        return 0;
    }
    assert(elemSize <= INT_MAX);
    q->elemSize = (int)elemSize;
    assert(numElems <= INT_MAX);
    q->numElems = (int)numElems;
    q->buffer = (char *)calloc(numElems, elemSize);
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

boolean seqQueueGet(QUEUE q, void *value)
{
    int nextGet = q->nextGet;
    int nextPut = q->nextPut;
    int numElems = q->numElems;
    int elemSize = q->elemSize;
    int empty = (nextGet == nextPut);

    if (!empty) {
        memcpy(value, q->buffer + nextGet * elemSize, (unsigned)elemSize);
        nextGet++;
        q->nextGet = (nextGet == numElems) ? 0 : nextGet;
    }
    return empty;
}

boolean seqQueuePut(QUEUE q, const void *value)
{
    int nextGet = q->nextGet;
    int nextPut = q->nextPut;
    int numElems = q->numElems;
    int elemSize = q->elemSize;
    int nextNextPut = (nextPut == numElems - 1) ? 0 : (nextPut + 1);
    int full = (nextNextPut == nextGet);

    memcpy(q->buffer + nextPut * elemSize, value, (unsigned)elemSize);
    if (!full) {
        q->nextPut = nextNextPut;
    }
    return full;
}

void seqQueueFlush(QUEUE q)
{
    q->nextGet = q->nextPut;
}

boolean seqQueueFree(const QUEUE q)
{
    int n;

    n = q->nextGet - q->nextPut - 1;
    if (n < 0)
        n += q->numElems;
    return n;
}

boolean seqQueueUsed(const QUEUE q)
{
    int n;

    n = q->nextPut - q->nextGet;
    if (n < 0)
        n += q->numElems;
    return n;
}

unsigned seqQueueNumElems(const QUEUE q)
{
    assert(q->numElems >= 0);
    return (unsigned)q->numElems;
}

unsigned seqQueueElemSize(const QUEUE q)
{
    assert(q->elemSize >= 0);
    return (unsigned)q->elemSize;
}

boolean seqQueueIsEmpty(const QUEUE q)
{
    return (q->nextPut == q->nextGet);
}

boolean seqQueueIsFull(const QUEUE q)
{
    int n;

    n = (q->nextPut - q->nextGet) + 1;
    return (n == 0 || n == q->numElems);
}

/* avoid nothing define but not used warnings */
pr_fun *queue_nothing_dummy = nothing;
