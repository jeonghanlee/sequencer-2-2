/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	DESCRIPTION: Generate tables for run-time sequencer.

	ENVIRONMENT: UNIX
	HISTORY:
28apr92,ajk	Implemented new event flag mode.
01mar94,ajk	Implemented new interface to sequencer (see seqCom.h).
01mar94,ajk	Implemented assignment of array elements to db channels.
17may94,ajk	removed old event flag (-e) option.
25may95,ajk	re-instated old event flag (-e) option.
20jul95,ajk	Added unsigned types.
22jul96,ajk	Added castS to action, event, delay, and exit functions.
08aug96,wfl	Added emission of code for syncQ queues.
11mar98,wfl	Corrected calculation of number of event words.
29apr99,wfl	Avoided compilation warnings.
29apr99,wfl	Removed unnecessary include files and unused vx_opt option.
06jul99,wfl	Supported "+m" (main) option; minor cosmetic changes.
07sep99,wfl	Set all event bits when array referenced in "when" test.
22sep99,grw     Supported entry and exit actions; supported state options;
		avoided warnings when no variables are mapped to channels.
18feb00,wfl     Partial support for local declarations (not yet complete).
31mar00,wfl	Supported entry handler.
***************************************************************************/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"seqCom.h"
#include	"analysis.h"
#include	"snc_main.h"
#include	"sym_table.h"

/* #define	DEBUG */

typedef struct event_mask_args {
	bitMask	*event_words;
	int	num_events;
} event_mask_args;

static void gen_channel_table(ChanList *chan_list, int num_events);
static void fill_channel_struct(Chan *cp, int elem_num, int num_events);
static void gen_state_table(Expr *ss_list, int num_events, int num_channels);
static void fill_state_struct(Expr *sp, char *ss_name);
static void gen_prog_params(char *param);
static void gen_prog_table(char *name, Options options);
static void encode_options(Options options);
static void encode_state_options(StateOptions options);
static void gen_ss_table(SymTable st, Expr *ss_list);
static void eval_state_event_mask(Expr *sp, int num_events,
	bitMask *event_words, int num_event_words);
static void eval_event_mask(Expr *ep, Expr *scope, void *parg);
static void eval_event_mask_subscr(Expr *ep, Expr *scope, void *parg);
static char *db_type_str(int type);

/* Generate all kinds of tables for a SNL program. */
void gen_tables(Program *p)
{
	printf("\n/************************ Tables ************************/\n");

	gen_channel_table(p->chan_list, p->num_events);

	gen_state_table(p->prog->prog_statesets, p->num_events, p->num_channels);

	gen_ss_table(p->sym_table, p->prog->prog_statesets);

	gen_prog_params(p->param);

	gen_prog_table(p->name, p->options);
}

/* Generate channel table with data for each defined channel */
static void gen_channel_table(ChanList *chan_list, int num_events)
{
	Chan *cp;

	if (chan_list->first)
	{
		printf("\n/* Channel Definitions */\n");
		printf("static struct seqChan seqChan[NUM_CHANNELS] = {\n");
		foreach (cp, chan_list->first)
		{
			int n;
#ifdef	DEBUG
			report("gen_channel_table: index=%d, num_elem=%d\n",
				cp->index, cp->num_elem);
#endif	/*DEBUG*/
			int num_elem = cp->num_elem ? cp->num_elem : 1;

			for (n = 0; n < num_elem; n++)
			{
				fill_channel_struct(cp, n, num_events);
			}
		}
		printf("};\n");
	}
	else
	{
		printf("\n/* No Channel Definitions, create 1 for ptr init. */\n");
		printf("static struct seqChan seqChan[1];\n");
	}
}

static void gen_var_name(Var *vp)
{
	if (vp->scope->type == D_PROG)
	{
		printf("%s", vp->name);
	}
	else if (vp->scope->type == D_SS)
	{
		printf("UserVar_ss_%s.%s", vp->scope->value, vp->name);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("UserVar_ss_%s.UserVar_state_%s.%s",
			vp->scope->extra.e_state->var_list->parent_scope->value,
			vp->scope->value, vp->name);
	}
}

/* Fill in a "seqChan" struct */
static void fill_channel_struct(Chan *cp, int elem_num, int num_events)
{
	Var		*vp;
	char		*suffix, elem_str[20], *db_name;
	int		ef_num, mon_flag;

	vp = cp->var;

	/* Figure out text needed to handle subscripts */
	if (vp->class == VC_ARRAY1 || vp->class == VC_ARRAYP)
		sprintf(elem_str, "[%d]", elem_num);
	else if (vp->class == VC_ARRAY2)
		sprintf(elem_str, "[%d][0]", elem_num);
	else
		elem_str[0] = '\0';

	if (vp->type == V_STRING)
		suffix = "[0]";
	else
		suffix = "";

	/* Pick up other db info */

	if (cp->num_elem == 0)
	{
		db_name = cp->db_name;
		mon_flag = cp->mon_flag;
		ef_num = cp->ef_num;
	}
	else
	{
		db_name = cp->db_name_list[elem_num];
		mon_flag = cp->mon_flag_list[elem_num];
		ef_num = cp->ef_num_list[elem_num];
	}

	if (db_name == NULL)
		db_name = ""; /* not assigned */

	/* Now, fill in the dbCom structure */

	printf("  {\"%s\", ", db_name);/* unexpanded db channel name */

	/* Ptr or offset to user variable */
	printf("(void *)");

	printf("OFFSET(struct UserVar, ");
	gen_var_name(vp);
	printf("%s%s), ", elem_str, suffix);

 	/* variable name with optional elem num */
	printf("\"%s%s\", ", vp->name, elem_str);

 	/* variable type */
	printf("\n    \"%s\", ", db_type_str(vp->type));

	/* count for db requests */
	printf("%d, ", cp->count);

	/* event number */
	printf("%d, ", cp->index + elem_num + num_events + 1);

	/* event flag number (or 0) */
	printf("%d, ", ef_num);

	/* monitor flag */
	printf("%d, ", mon_flag);

	/* syncQ queue */
	if (!vp->queued)
		printf("0, 0, 0");
	else
		printf("%d, %d, %d", vp->queued, vp->maxqsize, vp->qindex);

	printf("}");
	printf(",\n\n");
}

/* Convert variable type to db type as a string */
static char *db_type_str(int type)
{
	switch (type)
	{
	case V_CHAR:	return "char";
	case V_SHORT:	return "short";
	case V_INT:	return "int";
	case V_LONG:	return "long";
	case V_UCHAR:	return "unsigned char";
	case V_USHORT:	return "unsigned short";
	case V_UINT:	return "unsigned int";
	case V_ULONG:	return "unsigned long";
	case V_FLOAT:	return "float";
	case V_DOUBLE:	return "double";
	case V_STRING:	return "string";
	default:	return "";
	}
}

/* Generate state table */
static void gen_state_table(Expr *ss_list, int num_events, int num_channels)
{
	Expr			*ssp;
	Expr			*sp;
	int			n;
	int			num_event_words;
	bitMask			*event_mask;

	/* Allocate an array for event mask bits (NB, bit zero is not used) */
	num_event_words = (num_events + num_channels + NBITS)/NBITS;
	event_mask = (bitMask *)calloc(num_event_words, sizeof (bitMask));

	/* for all state sets ... */
	foreach (ssp, ss_list)
	{
		/* Build event mask arrays for each state */
		printf("\n/* Event masks for state set %s */\n", ssp->value);
		foreach (sp, ssp->ss_states)
		{
			eval_state_event_mask(sp, num_events, event_mask, num_event_words);
			printf("\t/* Event mask for state %s: */\n", sp->value);
			printf("static bitMask\tEM_%s_%s[] = {\n",
				ssp->value, sp->value);
			for (n = 0; n < num_event_words; n++)
				printf("\t0x%08lx,\n", (unsigned long)event_mask[n]);
			printf("};\n");
		}

		/* Build state struct for each state */
		printf("\n/* State Table */\n");
		printf("\nstatic struct seqState state_%s[] = {\n", ssp->value);
		foreach (sp, ssp->ss_states)
		{
			fill_state_struct(sp, ssp->value);
		}
		printf("\n};\n");
	}

	free(event_mask);
}

/* Fill in a "seqState" struct */
static void fill_state_struct(Expr *sp, char *ss_name)
{
	/* Write the C code to initialize the state struct for this state */

	printf("\t/* State \"%s\" */ {\n", sp->value);

	printf("\t/* state name */       \"%s\",\n", sp->value);

	printf("\t/* action function */ (ACTION_FUNC) A_%s_%s,\n", ss_name, sp->value);

	printf("\t/* event function */  (EVENT_FUNC) E_%s_%s,\n", ss_name, sp->value);

	printf("\t/* delay function */   (DELAY_FUNC) D_%s_%s,\n", ss_name, sp->value);

	/* Check if there are any entry or exit "transitions" in this
	   state so that if so the state block will be initialized to include a
	   reference to the function which implements them, but otherwise just
	   include a null pointer in those members */

	printf("\t/* entry function */   (ENTRY_FUNC)");
	if (sp->state_entry)
		printf(" I_%s_%s,\n", ss_name, sp->value);
	else
		printf(" 0,\n");

	printf("\t/* exit function */   (EXIT_FUNC)");
	if (sp->state_exit)
		printf(" O_%s_%s,\n", ss_name, sp->value);
	else
		printf(" 0,\n");

	printf("\t/* event mask array */ EM_%s_%s,\n", ss_name, sp->value);

	printf("\t/* state options */   ");
	encode_state_options(sp->extra.e_state->options);
	printf("}");
	printf(",\n\n");
}

/* Write the state option bitmask into a state block */
static void encode_state_options(StateOptions options)
{
	printf("(0");
	if (!options.do_reset_timers)
		printf(" | OPT_NORESETTIMERS");
	if (!options.no_entry_from_self)
		printf(" | OPT_DOENTRYFROMSELF");
	if (!options.no_exit_to_self)
		printf(" | OPT_DOEXITTOSELF");
	printf(")");
} 

/* Generate the program parameter list */
static void gen_prog_params(char *param)
{
	printf("\n/* Program parameter list */\n");

	printf("static char prog_param[] = \"%s\";\n", param);
}

/* Generate a single program structure ("struct seqProgram") */
static void gen_prog_table(char *name, Options options)
{
	printf("\n/* State Program table (global) */\n");

	printf("struct seqProgram %s = {\n", name);

	printf("\t/* magic number */       %d,\n", MAGIC);	/* magic number */

	printf("\t/* *name */              \"%s\",\n", name);	/* program name */

	printf("\t/* *pChannels */         seqChan,\n");	/* table of db channels */

	printf("\t/* numChans */           NUM_CHANNELS,\n");	/* number of db channels */

	printf("\t/* *pSS */               seqSS,\n");		/* array of SS blocks */

	printf("\t/* numSS */              NUM_SS,\n");		/* number of state sets */

	printf("\t/* user variable size */ sizeof(struct UserVar),\n");

	printf("\t/* *pParams */           prog_param,\n");	/* program parameters */

	printf("\t/* numEvents */          NUM_EVENTS,\n");	/* number event flags */

	printf("\t/* encoded options */    ");
	encode_options(options);

	printf("\t/* entry handler */      (ENTRY_FUNC) entry_handler,\n");
	printf("\t/* exit handler */       (EXIT_FUNC) exit_handler,\n");
	printf("\t/* numQueues */          NUM_QUEUES\n");	/* number of syncQ queues */

	printf("};\n");
}

static void encode_options(Options options)
{
	printf("(0");
	if (options.async)
		printf(" | OPT_ASYNC");
	if (options.conn)
		printf(" | OPT_CONN");
	if (options.debug)
		printf(" | OPT_DEBUG");
	if (options.newef)
		printf(" | OPT_NEWEF");
	printf(" | OPT_REENT");
	if (options.main)
		printf(" | OPT_MAIN");
	printf("),\n");
}

/* Generate state set table, one entry for each state set */
static void gen_ss_table(SymTable st, Expr *ss_list)
{
	Expr	*ssp;
	int	num_ss;

	printf("\n/* State Set Table */\n");
	printf("static struct seqSS seqSS[NUM_SS] = {\n");
	num_ss = 0;
	foreach (ssp, ss_list)
	{
		Expr *err_sp;

		if (num_ss > 0)
			printf("\n\n");
		num_ss++;

		printf("\t/* State set \"%s\" */ {\n", ssp->value);

		printf("\t/* ss name */            \"%s\",\n", ssp->value);

		printf("\t/* ptr to state block */ state_%s,\n", ssp->value);

		printf("\t/* number of states */   %d,\n", ssp->extra.e_ss->num_states);

		err_sp = sym_table_lookup(st, "error", ssp);

		printf("\t/* error state */        %d},\n",
			err_sp ? err_sp->extra.e_state->index : -1);
	}
	printf("};\n");
}

/* Evaluate composite event mask for a single state */
static void eval_state_event_mask(Expr *sp, int num_events,
	bitMask *event_words, int num_event_words)
{
	int		n;
	Expr		*tp;

	/* Set appropriate bits based on transition expressions.
	 * Here we look at the when() statement for references to event flags
	 * and database variables.  Database variables might have a subscript,
	 * which could be a constant (set a single event bit) or an expression
	 * (set a group of bits for the possible range of the evaluated expression)
	 */

	for (n = 0; n < num_event_words; n++)
		event_words[n] = 0;

	foreach (tp, sp->state_whens)
	{
		event_mask_args em_args = { event_words, num_events };

		/* look for simple variables, e.g. "when(x > 0)" */
		traverse_expr_tree(tp->when_cond, 1<<E_VAR, 0, 0,
			eval_event_mask, &em_args);

		/* look for subscripted variables, e.g. "when(x[i] > 0)" */
		traverse_expr_tree(tp->when_cond, 1<<E_SUBSCR, 0, 0,
			eval_event_mask_subscr, &em_args);
	}
#ifdef	DEBUG
	report("event mask for state %s is", sp->value);
	for (n = 0; n < num_event_words; n++)
		report(" 0x%lx", event_words[n]);
	report("\n");
#endif	/*DEBUG*/
}

/* Evaluate the event mask for a given transition (when() statement). 
 * Called from traverse_expr_tree() when ep->type==E_VAR.
 */
static void eval_event_mask(Expr *ep, Expr *scope, void *parg)
{
	event_mask_args	*em_args = (event_mask_args *)parg;
	Chan		*cp;
	Var		*vp;
	int		n;
	int		num_events = em_args->num_events;
	bitMask		*event_words = em_args->event_words;

	vp = ep->extra.e_var;
	assert(vp != 0);
	cp = vp->chan;

	/* event flag? */
	if (vp->type == V_EVFLAG)
	{
#ifdef	DEBUG
		report("  eval_event_mask: %s, ef_num=%d\n",
		 vp->name, vp->ef_num);
#endif	/*DEBUG*/
		bitSet(event_words, vp->ef_num);
		return;
	}

	/* if not associated with channel, return */
	if (cp == 0)
		return;

	/* value queued via syncQ? (pvgetQ() call; if array, all elements
	   are assumed to refer to the same event flag) */
	if (vp->queued)
	{
		int ef_num = cp->num_elem == 0 ?
					cp->ef_num : cp->ef_num_list[0];
#ifdef	DEBUG
		report("  eval_event_mask: %s, ef_num=%d\n",
		 vp->name, ef_num);
#endif	/*DEBUG*/
		bitSet(event_words, ef_num);
	}

	/* scalar database channel? */
	else if (cp->num_elem == 0)
	{
#ifdef	DEBUG
		report("  eval_event_mask: %s, db event bit=%d\n",
		 vp->name, cp->index + 1);
#endif	/*DEBUG*/
		bitSet(event_words, cp->index + num_events + 1);
	}

	/* array database channel? (set all bits) */
	else
	{
#ifdef	DEBUG
		report("  eval_event_mask: %s, db event bits=%d..%d\n",
			vp->name, cp->index + 1, cp->index + vp->length1);
#endif	/*DEBUG*/
		for (n = 0; n < vp->length1; n++)
		{
			bitSet(event_words, cp->index + n + num_events + 1);
		}
	}
}

/* Evaluate the event mask for a given transition (when() statement)
 * for subscripted database variables. 
 * Called from traverse_expr_tree() when ep->type==E_SUBSCR.
 */
static void eval_event_mask_subscr(Expr *ep, Expr *scope, void *parg)
{
	event_mask_args	*em_args = (event_mask_args *)parg;
	int		num_events = em_args->num_events;
	bitMask		*event_words = em_args->event_words;

	Chan		*cp;
	Var		*vp;
	Expr		*ep1, *ep2;
	int		subscr, n;

	assert(ep->type == E_SUBSCR);
	ep1 = ep->subscr_operand;
	assert(ep1 != 0);
	if (ep1->type != E_VAR)
		return;
	vp = ep1->extra.e_var;
	assert(vp != 0);

	cp = vp->chan;
	if (cp == NULL)
		return;

	/* Only do this if the array is assigned to multiple pv's */
	if (cp->num_elem == 0)
	{
#ifdef	DEBUG
		report("  eval_event_mask_subscr: %s, db event bit=%d\n",
		 vp->name, cp->index);
#endif	/*DEBUG*/
		bitSet(event_words, cp->index + num_events + 1);
		return;
	}

	/* Is this subscript a constant? */
	ep2 = ep->subscr_index;
	assert(ep2 != 0);
	if (ep2->type == E_CONST)
	{
		subscr = atoi(ep2->value);
		if (subscr < 0 || subscr >= cp->num_elem)
			return;
#ifdef	DEBUG
		report("  eval_event_mask_subscr: %s, db event bit=%d\n",
		 vp->name, cp->index + subscr + 1);
#endif	/*DEBUG*/
		bitSet(event_words, cp->index + subscr + num_events + 1);
		return;
	}

	/* subscript is an expression -- set all event bits for this variable */
#ifdef	DEBUG
	report("  eval_event_mask_subscr: %s, db event bits=%d..%d\n",
		vp->name, cp->index + 1, cp->index + vp->length1);
#endif	/*DEBUG*/
	for (n = 0; n < vp->length1; n++)
	{
		bitSet(event_words, cp->index + n + num_events + 1);
	}
}
