/* Copyright 2010 Helmholtz-Zentrum Berlin f. Materialien und Energie GmbH
   (see file Copyright.HZB included in this distribution)
*/
#include "seq.h"

struct seqQueue {
    size_t          wr;
    size_t          rd;
    size_t          numElems;
    size_t          elemSize;
    boolean         overflow;
    epicsMutexId    mutex;
    char            *buffer;
};

boolean seqQueueInvariant(QUEUE q)
{
    return (q != NULL)
        && q->elemSize > 0
        && q->numElems > 0
        && q->numElems <= seqQueueMaxNumElems
        && q->rd < q->numElems
        && q->wr < q->numElems;
}

QUEUE seqQueueCreate(size_t numElems, size_t elemSize)
{
    QUEUE q = new(struct seqQueue);

    /* check arguments to establish invariants */
    if (numElems == 0) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: numElems must be positive\n");
        return 0;
    }
    if (elemSize == 0) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: elemSize must be positive\n");
        return 0;
    }
    if (numElems > seqQueueMaxNumElems) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: numElems too large\n");
        return 0;
    }
    if (!q) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: out of memory\n");
        return 0;
    }
    q->buffer = (char *)calloc(numElems, elemSize);
    if (!q->buffer) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: out of memory\n");
        free(q);
        return 0;
    }
    q->mutex = epicsMutexCreate();
    if (!q->mutex) {
        errlogSevPrintf(errlogFatal, "seqQueueCreate: out of memory\n");
        free(q->buffer);
        free(q);
        return 0;
    }
    q->elemSize = elemSize;
    q->numElems = numElems;
    q->overflow = FALSE;
    q->rd = q->wr = 0;
    return q;
}

void seqQueueDestroy(QUEUE q)
{
    epicsMutexDestroy(q->mutex);
    free(q->buffer);
    free(q);
}

boolean seqQueueGet(QUEUE q, void *value)
{
    return seqQueueGetF(q, memcpy, value);
}

boolean seqQueueGetF(QUEUE q, seqQueueFunc *get, void *arg)
{
    if (q->wr == q->rd) {
        if (!q->overflow) {
            return TRUE;
        }
        epicsMutexLock(q->mutex);
        get(arg, q->buffer + q->rd * q->elemSize, q->elemSize);
        /* check again, a put might have intervened */
        if (q->wr == q->rd && q->overflow)
            q->overflow = FALSE;
        else
            q->rd = (q->rd + 1) % q->numElems;
        epicsMutexUnlock(q->mutex);
    } else {
        get(arg, q->buffer + q->rd * q->elemSize, q->elemSize);
        q->rd = (q->rd + 1) % q->numElems;
    }
    return FALSE;
}

boolean seqQueuePut(QUEUE q, const void *value)
{
    return seqQueuePutF(q, memcpy, value);
}

boolean seqQueuePutF(QUEUE q, seqQueueFunc *put, const void *arg)
{
    boolean r = FALSE;

    if (q->overflow || (q->wr + 1) % q->numElems == q->rd) {
        epicsMutexLock(q->mutex);
        if ((q->wr + 1) % q->numElems == q->rd) {
            if (q->overflow) {
                r = TRUE;   /* we will overwrite the last element */
            }
            q->overflow = TRUE;
        } else if (q->overflow) {
            /* we had a get since the last put, so
               can now eliminate overflow flag and instead
               increment the write pointer */
            q->wr = (q->wr + 1) % q->numElems;
            if ((q->wr + 1) % q->numElems != q->rd) {
                q->overflow = FALSE;
            }
        }
        put(q->buffer + q->wr * q->elemSize, arg, q->elemSize);
        if (!q->overflow) {
            q->wr = (q->wr + 1) % q->numElems;
        }
        epicsMutexUnlock(q->mutex);
    } else {
        put(q->buffer + q->wr * q->elemSize, arg, q->elemSize);
        q->wr = (q->wr + 1) % q->numElems;
    }
    return r;
}

void seqQueueFlush(QUEUE q)
{
    epicsMutexLock(q->mutex);
    q->rd = q->wr;
    q->overflow = FALSE;
    epicsMutexUnlock(q->mutex);
}

static size_t used(const QUEUE q)
{
    return (q->numElems + q->wr - q->rd) % q->numElems + (q->overflow ? 1 : 0);
}

size_t seqQueueFree(const QUEUE q)
{
    return q->numElems - used(q);
}

size_t seqQueueUsed(const QUEUE q)
{
    return used(q);
}

boolean seqQueueIsEmpty(const QUEUE q)
{
    return q->wr == q->rd && !q->overflow;
}

boolean seqQueueIsFull(const QUEUE q)
{
    return (q->wr + 1) % q->numElems == q->rd && q->overflow;
}

size_t seqQueueNumElems(const QUEUE q)
{
    return q->numElems;
}

size_t seqQueueElemSize(const QUEUE q)
{
    return q->elemSize;
}

/* avoid nothing define but not used warnings */
pr_fun *queue_nothing_dummy = nothing;
