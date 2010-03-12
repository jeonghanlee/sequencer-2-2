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
/*#define	DEBUG	1*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<assert.h>

#include	"seqCom.h"
#include	"analysis.h"
#include	"gen_code.h"
#include	"snc_main.h"

typedef struct eval_event_mask_args {
	bitMask	*event_words;
	int	num_events;
} eval_event_mask_args;

static void encode_state_options(Expr *sp);
static void gen_db_blocks(ChanList *chan_list, int num_events, int opt_reent);
static void fill_db_block(Chan *cp, int elem_num, int num_events, int opt_reent);
static void gen_state_blocks(Expr *ss_list, int num_events, int num_channels);
static void fill_state_block(Expr *sp, char *ss_name);
static void gen_prog_params(char *prog_param);
static void gen_prog_table(char *prog_name, Options *options);
static void encode_options(Options *options);
static void gen_ss_array(Expr *ss_list);
static void eval_state_event_mask(Expr *sp, int num_events,
	bitMask *event_words, int num_event_words);
static void eval_event_mask(Expr *ep, eval_event_mask_args *args);
static void eval_event_mask_subscr(Expr *ep, eval_event_mask_args *args);
static int find_error_state(Expr *ssp);
static char *db_type_str(int type);

/* Generate all kinds of tables for a SNL program. */
void gen_tables(Program *p)
{
	printf("\f/************************ Tables ***********************/\n");

	gen_db_blocks(p->chan_list, p->num_events, p->options->reent);

	gen_state_blocks(p->ss_list, p->num_events, p->num_channels);

	gen_ss_array(p->ss_list);

	gen_prog_params(p->param);

	gen_prog_table(p->name, p->options);
}

/* Generate database blocks with structure and data for each defined channel */
static void gen_db_blocks(ChanList *chan_list, int num_events, int opt_reent)
{
	Chan *cp;

	if (chan_list->first)
	{
		printf("\n/* Database Blocks */\n");
		printf("static struct seqChan seqChan[NUM_CHANNELS] = {\n");
		for (cp = chan_list->first; cp != NULL; cp = cp->next)
		{
			int n;
#ifdef	DEBUG
			fprintf(stderr, "gen_db_blocks: index=%d, num_elem=%d\n",
				cp->index, cp->num_elem);
#endif	/*DEBUG*/
			int num_elem = cp->num_elem ? cp->num_elem : 1;

			for (n = 0; n < num_elem; n++)
			{
				fill_db_block(cp, n, num_events, opt_reent);
			}
		}
		printf("};\n");
	}
	else
	{
		printf("\n/* No Database Blocks, create 1 for ptr init. */\n");
		printf("static struct seqChan seqChan[1];\n");
	}
}

/* Fill in a db block with data (all elements for "seqChan" struct) */
static void fill_db_block(Chan *cp, int elem_num, int num_events, int opt_reent)
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
	if (opt_reent)
		printf("OFFSET(struct UserVar, %s%s%s), ", vp->name, elem_str, suffix);
	else
		printf("&%s%s%s, ", vp->name, elem_str, suffix); /* variable ptr */

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
		printf("%d, %d, %d", vp->queued, vp->maxQueueSize, vp->queueIndex);

	printf("},\n\n");
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

/* Generate structure and data for state blocks */
static void gen_state_blocks(Expr *ss_list, int num_events, int num_channels)
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
	for (ssp = ss_list; ssp != NULL; ssp = ssp->next)
	{
		/* Build event mask arrays for each state */
		printf("\n/* Event masks for state set %s */\n", ssp->value);
		for (sp = ssp->left; sp != NULL; sp = sp->next)
		{
			eval_state_event_mask(sp, num_events, event_mask, num_event_words);
			printf("\t/* Event mask for state %s: */\n", sp->value);
			printf("static bitMask\tEM_%s_%s[] = {\n",
				ssp->value, sp->value);
			for (n = 0; n < num_event_words; n++)
				printf("\t0x%08lx,\n", (unsigned long)event_mask[n]);
			printf("};\n");
		}

		/* Build state block for each state in this state set */
		printf("\n/* State Blocks */\n");
		printf("\nstatic struct seqState state_%s[] = {\n", ssp->value);
		for (sp = ssp->left; sp != NULL; sp = sp->next)
		{
			fill_state_block(sp, ssp->value);
		}
		printf("\n};\n");
	}

	free(event_mask);
}

/* Fill in data for a state block (see seqState in seqCom.h) */
static void fill_state_block(Expr *sp, char *ss_name)
{
	Expr *ep;
	int isEntry = FALSE, isExit = FALSE;

	/* Check if there are any entry or exit "transitions" in this
	   state so that if so the state block will be initialized to include a
	   reference to the function which implements them, but otherwise just
	   include a null pointer in those members */

	for ( ep = sp->left; ep != NULL; ep = ep->next )
	{
		if ( ep->type == E_ENTRY ) 
			isEntry = TRUE;
		else if ( ep->type == E_EXIT )
			isExit = TRUE;
	} 

	/* Write the source code to initialize the state block for this state */

	printf("\t/* State \"%s\" */ {\n", sp->value);

	printf("\t/* state name */       \"%s\",\n", sp->value);

	printf("\t/* action function */ (ACTION_FUNC) A_%s_%s,\n", ss_name, sp->value);

	printf("\t/* event function */  (EVENT_FUNC) E_%s_%s,\n", ss_name, sp->value);

	printf("\t/* delay function */   (DELAY_FUNC) D_%s_%s,\n", ss_name, sp->value);

	printf("\t/* entry function */   (ENTRY_FUNC)");
	if ( isEntry ) 
		printf(" I_%s_%s,\n", ss_name, sp->value);
	else
		printf(" 0,\n");

	printf("\t/* exit function */   (EXIT_FUNC)");
	if ( isExit ) 
		printf(" O_%s_%s,\n", ss_name, sp->value);
	else
		printf(" 0,\n");

	printf("\t/* event mask array */ EM_%s_%s,\n", ss_name, sp->value);

	printf("\t/* state options */   ");
	encode_state_options(sp);
	printf("},\n\n");
}

/* Writes the state option bitmask into a state block. At present this f is
extremely simple since there is only one permitted option and so there are
no possible state option conflicts.  */
static void encode_state_options(Expr *sp)
{
	Expr	*ep;
	char	*pc;
	bitMask	options = 0,
		optionSpec = 0;
	int	duplicate = FALSE,
		contradictory = FALSE;  /* Currently there are no contradictions */

	printf("(0");
	/* For each option character, within each OPTION statement in this state,
	   check the option character is recognized and if so code it's bit mask */
	for (ep = sp->right; ep != NULL; ep = ep->next )
	{
		char *plusminus;
		int opt_minus;

		if (ep->type != E_OPTION) {
			continue;
		}

		plusminus = ep->left->value;
		opt_minus = plusminus[0] == '-';

		assert(ep->left->type == E_X);
		for (pc = ep->value; *pc != '\0'; pc++)
		{
			/* Option not to reset timers on state entry from self */
			if ( *pc == 't' )
			{
				if ( optionSpec & OPT_NORESETTIMERS )
					duplicate = TRUE;
				if (opt_minus)
				{
					printf(" | OPT_NORESETTIMERS" );
					options |= OPT_NORESETTIMERS;
				}
				optionSpec |=  OPT_NORESETTIMERS;
			}
			else if ( *pc == 'e' )
			{
				if ( optionSpec & OPT_DOENTRYFROMSELF )
					duplicate = TRUE;
				if (opt_minus)
				{
					printf(" | OPT_DOENTRYFROMSELF" );
					options |= OPT_DOENTRYFROMSELF;
				}
				optionSpec |= OPT_DOENTRYFROMSELF;
			}
			else if ( *pc == 'x' )
			{
				if ( optionSpec & OPT_DOEXITTOSELF )
					duplicate = TRUE;
				if (opt_minus)
				{
					printf(" | OPT_DOEXITTOSELF" );
					options |= OPT_DOEXITTOSELF;
				}
				optionSpec |= OPT_DOEXITTOSELF;
			}
			else
			{
				report_loc(ep->src_file, ep->line_num);
				report("unrecognized option in state %s: %s%c\n",
					sp->value, plusminus, *pc);
			}

			if ( duplicate )
			{
				report_loc(ep->src_file, ep->line_num);
				report("option already specified in state %s: %c\n",
					sp->value, *pc);
			}
			if ( contradictory )
			{
				report_loc(ep->src_file, ep->line_num);
				report("contradictory option or option out of "
					"order %s%c in state %s\n",
					plusminus, *pc, sp->value);
			}

		}
	}
	printf(")");
} 


/* Generate the program parameter list */
static void gen_prog_params(char *prog_param)
{
	printf("\n/* Program parameter list */\n");

	printf("static char prog_param[] = \"%s\";\n", prog_param);
}

/* Generate the structure with data for a state program table (SPROG) */
static void gen_prog_table(char *prog_name, Options *options)
{
	printf("\n/* State Program table (global) */\n");

	printf("struct seqProgram %s = {\n", prog_name);

	printf("\t/* magic number */       %d,\n", MAGIC);	/* magic number */

	printf("\t/* *name */              \"%s\",\n", prog_name);/* program name */

	printf("\t/* *pChannels */         seqChan,\n");	/* table of db channels */

	printf("\t/* numChans */           NUM_CHANNELS,\n");	/* number of db channels */

	printf("\t/* *pSS */               seqSS,\n");		/* array of SS blocks */

	printf("\t/* numSS */              NUM_SS,\n");	/* number of state sets */

	if (options->reent)
		printf("\t/* user variable size */ sizeof(struct UserVar),\n");
	else
		printf("\t/* user variable size */ 0,\n");

	printf("\t/* *pParams */           prog_param,\n");	/* program parameters */

	printf("\t/* numEvents */          NUM_EVENTS,\n");	/* number event flags */

	printf("\t/* encoded options */    ");
	encode_options(options);

	printf("\t/* entry handler */      (ENTRY_FUNC) entry_handler,\n");
	printf("\t/* exit handler */       (EXIT_FUNC) exit_handler,\n");
	printf("\t/* numQueues */          NUM_QUEUES,\n");	/* number of syncQ queues */

	printf("};\n");
}

static void encode_options(Options *options)
{
	printf("(0");
	if (options->async)
		printf(" | OPT_ASYNC");
	if (options->conn)
		printf(" | OPT_CONN");
	if (options->debug)
		printf(" | OPT_DEBUG");
	if (options->newef)
		printf(" | OPT_NEWEF");
	if (options->reent)
		printf(" | OPT_REENT");
	if (options->main)
		printf(" | OPT_MAIN");
	printf("),\n");
}

/* Generate an array of state set blocks, one entry for each state set */
static void gen_ss_array(Expr *ss_list)
{
	Expr			*ssp;
	int			nss, num_states;

	printf("\n/* State Set Blocks */\n");
	printf("static struct seqSS seqSS[NUM_SS] = {\n");
	nss = 0;
	for (ssp = ss_list; ssp != NULL; ssp = ssp->next)
	{
		if (nss > 0)
			printf("\n\n");
		nss++;

		printf("\t/* State set \"%s\" */ {\n", ssp->value);

		printf("\t/* ss name */            \"%s\",\n", ssp->value);

		printf("\t/* ptr to state block */ state_%s,\n", ssp->value);

		num_states = expr_count(ssp->left);
		printf("\t/* number of states */   %d,\n", num_states);

		printf("\t/* error state */        %d},\n", find_error_state(ssp));

	}
	printf("};\n");
}

/* Find the state named "error" in a state set */
static int find_error_state(Expr *ssp)
{
	Expr		*sp;
	int		error_state;
	for (sp = ssp->left, error_state = 0; sp != 0; sp = sp->next, error_state++)
	{
		if (strcmp(sp->value, "error") == 0)
			return error_state;
	}
	return -1; /* no state named "error" in this state set */
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

	for (tp = sp->left; tp != 0; tp = tp->next)
	{
		eval_event_mask_args args = { event_words, num_events };

		/* ignore local declarations */
		if (tp->type == E_TEXT)
			continue;

		/* look for simple variables, e.g. "when(x > 0)" */
		traverse_expr_tree(tp->left, E_VAR, 0, (expr_fun*)eval_event_mask, &args);

		/* look for subscripted variables, e.g. "when(x[i] > 0)" */
		traverse_expr_tree(tp->left, E_SUBSCR, 0, (expr_fun*)eval_event_mask_subscr, &args);
	}
#ifdef	DEBUG
	fprintf(stderr, "Event mask for state %s is", sp->value);
	for (n = 0; n < num_event_words; n++)
		fprintf(stderr, " 0x%lx", event_words[n]);
	fprintf(stderr, "\n");
#endif	/*DEBUG*/
}

/* Evaluate the event mask for a given transition (when() statement). 
 * Called from traverse_expr_tree() when ep->type==E_VAR.
 */
static void eval_event_mask(Expr *ep, eval_event_mask_args *args)
{
	Chan		*cp;
	Var		*vp;
	int		n;
	int		num_events = args->num_events;
	bitMask		*event_words = args->event_words;

	vp = (Var *)ep->left;
	if (vp == 0)
		return; /* this shouldn't happen */
	cp = vp->chan;

	/* event flag? */
	if (vp->type == V_EVFLAG)
	{
#ifdef	DEBUG
		fprintf(stderr, " eval_event_mask: %s, ef_num=%d\n",
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
		fprintf(stderr, " eval_event_mask: %s, ef_num=%d\n",
		 vp->name, ef_num);
#endif	/*DEBUG*/
		bitSet(event_words, ef_num);
	}

	/* scalar database channel? */
	else if (cp->num_elem == 0)
	{
#ifdef	DEBUG
		fprintf(stderr, " eval_event_mask: %s, db event bit=%d\n",
		 vp->name, cp->index + 1);
#endif	/*DEBUG*/
		bitSet(event_words, cp->index + num_events + 1);
	}

	/* array database channel? (set all bits) */
	else
	{
#ifdef	DEBUG
		fprintf(stderr, "  eval_event_mask: %s, db event bits=%d..%d\n",
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
static void eval_event_mask_subscr(Expr *ep, eval_event_mask_args *args)
{
	int		num_events = args->num_events;
	bitMask		*event_words = args->event_words;

	Chan		*cp;
	Var		*vp;
	Expr		*ep1, *ep2;
	int		subscr, n;

	ep1 = ep->left;
	if (ep1 == 0 || ep1->type != E_VAR)
		return;
	vp = (Var *)ep1->left;
	if (vp == 0)
		return; /* this shouldn't happen */

	cp = vp->chan;
	if (cp == NULL)
		return;

	/* Only do this if the array is assigned to multiple pv's */
	if (cp->num_elem == 0)
	{
#ifdef	DEBUG
		fprintf(stderr, "  eval_event_mask_subscr: %s, db event bit=%d\n",
		 vp->name, cp->index);
#endif	/*DEBUG*/
		bitSet(event_words, cp->index + num_events + 1);
		return;
	}

	/* Is this subscript a constant? */
	ep2 = ep->right;
	if (ep2 == 0)
		return;
	if (ep2->type == E_CONST)
	{
		subscr = atoi(ep2->value);
		if (subscr < 0 || subscr >= cp->num_elem)
			return;
#ifdef	DEBUG
		fprintf(stderr, "  eval_event_mask_subscr: %s, db event bit=%d\n",
		 vp->name, cp->index + subscr + 1);
#endif	/*DEBUG*/
		bitSet(event_words, cp->index + subscr + num_events + 1);
		return;
	}

	/* subscript is an expression -- set all event bits for this variable */
#ifdef	DEBUG
	fprintf(stderr, "  eval_event_mask_subscr: %s, db event bits=%d..%d\n",
	 vp->name, cp->index + 1, cp->index + vp->length1);
#endif	/*DEBUG*/
	for (n = 0; n < vp->length1; n++)
	{
		bitSet(event_words, cp->index + n + num_events + 1);
	}
}
