/*
 * $Id: seqCommands.c,v 1.2 2000-04-18 18:24:13 norume Exp $
 *
 * DESCRIPTION: EPICS sequencer commands
 *
 * Author:  Eric Norum
 *
 * Experimental Physics and Industrial Control System (EPICS)
 *
 * Initial development by:
 *    Canadian Light Source
 *    107 North Road
 *    University of Saskatchewan
 *    Saskatoon, Saskatchewan, CANADA
 *    cls.usask.ca
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <osiThread.h>
#include <cantProceed.h>

#include <seqCom.h>
#include <CommandInterpreter.h>

/*
 * Until the mechanism for registering sequencer programs and commands
 * has been finalized I've got a couple of different methods supported
 * in this source.  At the moment I'm using C++ constructors to generate
 * the calls.
 */
#define SEQ_PROG_REG

/*
 * Prototypes (these probably belong in seqCom.h)
 */
long seqShow (threadId);
long seqChanShow (threadId, char *);
long seqQueueShow (threadId tid);
long seqStop (threadId);

#ifdef SEQ_PROG_REG
struct sequencerProgram {
    struct seqProgram *prog;
    struct sequencerProgram *next;

};
static struct sequencerProgram *seqHead;

/*
 * This routine is called before multitasking has started, so there's
 * no race condition in creating the linked list.
 */
void
seqRegisterSequencerProgram (struct seqProgram *p)
{
    struct sequencerProgram *sp;

    sp = (struct sequencerProgram *)mallocMustSucceed (sizeof *sp, "seqRegisterSequencerProgram");
    sp->prog = p;
    sp->next = seqHead;
    seqHead = sp;
}
#endif

static long
seqWrapper (const char *table, char *macroDef, unsigned int stackSize)
{
#ifdef SEQ_PROG_REG
    struct sequencerProgram *sp;
#else
    extern struct seqProgram * const seqPrograms[];
    struct seqProgram * const *spp;
#endif

    if (*table == '&')
        table++;
#ifdef SEQ_PROG_REG
    for (sp = seqHead ; sp != NULL ; sp = sp->next) {
        if (!strcmp (table, sp->prog->pProgName)) {
            seq (sp->prog, macroDef, stackSize);
#else
    for (spp = seqPrograms ; *spp != NULL ; spp++) {
        if (!strcmp (table, (*spp)->pProgName)) {
            seq (*spp, macroDef, stackSize);
#endif
            return 0;
        }
    }
    printf ("Can't find sequencer `%s'.\n", table);
    return 1;
}

static threadId
findThread (const char *name)
{
    threadId id;
    char *term;

    id = (threadId)strtoul (name, &term, 16);
    if ((term != name) && (*term == '\0'))
        return id;
    id = threadGetId (name);
    if (id)
        return id;
    printf ("No such thread.\n");
    return NULL;
}

static long
seqShowWrapper (int argc, char **argv)
{
    threadId id;

    if (argc == 1)
        return seqShow (NULL);
    if ((id = findThread (argv[1])) != NULL)
        return seqShow (id);
    return 0;
}

static long
seqQueueShowWrapper (int argc, char **argv)
{
    threadId id;

    if ((id = findThread (argv[1])) != NULL)
        return seqQueueShow (id);
    return 0;
}

static long
seqStopWrapper (int argc, char **argv)
{
    threadId id;

    if ((id = findThread (argv[1])) != NULL)
        return seqStop (id);
    return 0;
}

static long
seqChanShowWrapper (int argc, char **argv)
{
    threadId id;
    char *chan;

    if (argc <= 2)
        chan = NULL;
    else
        chan = argv[2];
    if (argc == 1)
        return seqChanShow (NULL, chan);
    if ((id = findThread (argv[1])) != NULL)
        return seqChanShow (id, chan);
    return 0;
}

/*
 * Command table
 */
typedef long (*cmd)();
static const struct CommandTableEntry CommandTable[] = {
    { "seq",
      "Start sequencer",
      "**i",        (cmd)seqWrapper,             2, 4
    },

    { "seqShow",
      "Show sequencer info",
      NULL,            (cmd)seqShowWrapper,      1, 2
    },

    { "seqStop",
      "Stop sequencer",
      NULL,            (cmd)seqStopWrapper,      2, 2
    },

    { "seqQueueShow",
      "Show sequencer queue info",
      NULL,            (cmd)seqQueueShowWrapper, 2, 2
    },

    { "seqChanShow",
      "Show sequencer channel info",
      NULL,            (cmd)seqChanShowWrapper,  1, 3
    },

    { NULL,    NULL, NULL, NULL, 0, 0 },
};

/*
 * This routine is called before multitasking has started, so there's
 * no race condition in the test/set of firsTime.
 */
void
seqRegisterSequencerCommands (void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        CommandInterpreterRegisterCommands (CommandTable);
    }
};
