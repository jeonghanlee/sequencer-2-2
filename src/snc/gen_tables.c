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
#include	"gen_code.h"

/* #define DEBUG */

typedef struct event_mask_args {
	bitMask	*event_words;
	int	num_event_flags;
} event_mask_args;

static void gen_channel_table(ChanList *chan_list, int num_event_flags);
static void fill_channel_struct(Chan *cp, int elem_num, int num_event_flags);
static void gen_state_table(Expr *ss_list, int num_event_flags, int num_channels);
static void fill_state_struct(Expr *sp, char *ss_name);
static void gen_prog_table(Program *p);
static void encode_options(Options options);
static void encode_state_options(StateOptions options);
static void gen_ss_table(SymTable st, Expr *ss_list);
static void gen_state_event_mask(Expr *sp, int num_event_flags,
	bitMask *event_words, int num_event_words);
static int iter_event_mask_scalar(Expr *ep, Expr *scope, void *parg);
static int iter_event_mask_array(Expr *ep, Expr *scope, void *parg);
static char *pv_type_str(int type);

/* Generate all kinds of tables for a SNL program. */
void gen_tables(Program *p)
{
	printf("\n/************************ Tables ************************/\n");
	gen_channel_table(p->chan_list, p->num_event_flags);
	gen_state_table(p->prog->prog_statesets, p->num_event_flags, p->num_channels);
	gen_ss_table(p->sym_table, p->prog->prog_statesets);
	gen_prog_table(p);
}

/* Generate channel table with data for each defined channel */
static void gen_channel_table(ChanList *chan_list, int num_event_flags)
{
	Chan *cp;

	if (chan_list->first)
	{
		printf("\n/* Channel Definitions */\n");
		printf("static struct seqChan seqChan[] = {\n");
		foreach (cp, chan_list->first)
		{
			int n;
#ifdef DEBUG
			report("gen_channel_table: index=%d, num_elem=%d\n",
				cp->index, cp->num_elem);
#endif
			for (n = 0; n < cp->num_elem; n++)
			{
				fill_channel_struct(cp, n, num_event_flags);
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
		printf("UV_%s.%s", vp->scope->value, vp->name);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("UV_%s.UV_%s.%s",
			vp->scope->extra.e_state->var_list->parent_scope->value,
			vp->scope->value, vp->name);
	}
}

/* Generate a seqChan structure */
static void fill_channel_struct(Chan *cp, int elem_num, int num_event_flags)
{
	Var		*vp;
	char		*suffix, elem_str[20], *pv_name;
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

	pv_name = cp->pv_names[elem_num];
	mon_flag = cp->mon_flags[elem_num];
	ef_num = cp->ef_nums[elem_num];

	if (pv_name == NULL)
		pv_name = ""; /* not assigned */

	printf("  {\"%s\", ", pv_name);/* unexpanded channel name */

	/* Ptr or offset to user variable */
	printf("(void *)");

	printf("OFFSET(struct %s, ", SNL_PREFIX);
	gen_var_name(vp);
	printf("%s%s), ", elem_str, suffix);

 	/* variable name with optional elem num */
	printf("\"%s%s\", ", vp->name, elem_str);

 	/* variable type */
	printf("\n    \"%s\", ", pv_type_str(vp->type));

	/* count, for requests */
	printf("%d, ", cp->count);

	/* event number */
	printf("%d, ", cp->index + elem_num + num_event_flags + 1);

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

/* Convert variable type to pv type as a string */
static char *pv_type_str(int type)
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

/* Generate state event mask and table */
static void gen_state_table(Expr *ss_list, int num_event_flags, int num_channels)
{
	Expr	*ssp;
	Expr	*sp;
	int	n;
	int	num_event_words = (num_event_flags + num_channels + NBITS)/NBITS;
	bitMask	event_mask[num_event_words];

	/* NOTE: bit zero of event mask is not used */

	/* For each state set... */
	foreach (ssp, ss_list)
	{
		/* For each state, generate event mask array */
		printf("\n/* Event masks for state set %s */\n", ssp->value);
		foreach (sp, ssp->ss_states)
		{
			gen_state_event_mask(sp, num_event_flags, event_mask, num_event_words);
			printf("\t/* Event mask for state %s: */\n", sp->value);
			printf("static bitMask\tEM_%s_%s[] = {\n",
				ssp->value, sp->value);
			for (n = 0; n < num_event_words; n++)
				printf("\t0x%08lx,\n", event_mask[n]);
			printf("};\n");
		}

		/* For each state, generate state structure */
		printf("\n/* State Table */\n");
		printf("\nstatic struct seqState state_%s[] = {\n", ssp->value);
		foreach (sp, ssp->ss_states)
		{
			fill_state_struct(sp, ssp->value);
		}
		printf("};\n");
	}
}

/* Generate a state struct */
static void fill_state_struct(Expr *sp, char *ss_name)
{
	printf("\t/* State \"%s\" */ {\n", sp->value);
	printf("\t/* state name */        \"%s\",\n", sp->value);
	printf("\t/* action function */   A_%s_%s,\n", ss_name, sp->value);
	printf("\t/* event function */    E_%s_%s,\n", ss_name, sp->value);
	printf("\t/* delay function */    D_%s_%s,\n", ss_name, sp->value);
	printf("\t/* entry function */    ");
	if (sp->state_entry)
		printf("I_%s_%s,\n", ss_name, sp->value);
	else
		printf("0,\n");
	printf("\t/* exit function */     ");
	if (sp->state_exit)
		printf("O_%s_%s,\n", ss_name, sp->value);
	else
		printf("0,\n");
	printf("\t/* event mask array */  EM_%s_%s,\n", ss_name, sp->value);
	printf("\t/* state options */     ");
	encode_state_options(sp->extra.e_state->options);
	printf("}");
	printf(",\n\n");
}

/* Generate the state option bitmask */
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

/* Generate a single program structure ("struct seqProgram") */
static void gen_prog_table(Program *p)
{
	printf("\n/* State Program table (global) */\n");
	printf("struct seqProgram %s = {\n", p->name);
	printf("\t/* magic number */      %d,\n", MAGIC);
	printf("\t/* name */              \"%s\",\n", p->name);
	printf("\t/* channels */          seqChan,\n");
	printf("\t/* num. channels */     %d,\n", p->num_channels);
	printf("\t/* state sets */        seqSS,\n");
	printf("\t/* num. state sets */   %d,\n", p->num_ss);
	printf("\t/* user var size */     sizeof(struct %s),\n", SNL_PREFIX);
	printf("\t/* param */             \"%s\",\n", p->param);
	printf("\t/* num. event flags */  %d,\n", p->num_event_flags);
	printf("\t/* encoded options */   "); encode_options(p->options);
	printf("\t/* entry handler */     entry_handler,\n");
	printf("\t/* exit handler */      exit_handler,\n");
	printf("\t/* num. queues */       %d\n", p->num_queues);
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
	printf(" | OPT_REENT");					/* option is now always on */
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
	printf("static struct seqSS seqSS[] = {\n");
	num_ss = 0;
	foreach (ssp, ss_list)
	{
		Expr *err_sp;

		if (num_ss > 0)
			printf("\n\n");
		num_ss++;
		printf("\t/* State set \"%s\" */ {\n", ssp->value);
		printf("\t/* ss name */           \"%s\",\n", ssp->value);
		printf("\t/* state struct */      state_%s,\n", ssp->value);
		printf("\t/* number of states */  %d,\n", ssp->extra.e_ss->num_states);
		err_sp = sym_table_lookup(st, "error", ssp);
		printf("\t/* error state */       %d},\n\n",
			err_sp ? err_sp->extra.e_state->index : -1);
	}
	printf("};\n");
}

/* Generate event mask for a single state. The event mask has a bit set for each
   event flag and for each process variable (assigned var) used in one of the
   state's when() conditions. The bits from 1 to num_event_flags are for the
   event flags. The bits from num_event_flags+1 to num_event_flags+num_channels
   are for process variables. Bit zero is not used for whatever mysterious reason
   I cannot tell. */
static void gen_state_event_mask(Expr *sp, int num_event_flags,
	bitMask *event_words, int num_event_words)
{
	int		n;
	Expr		*tp;

	for (n = 0; n < num_event_words; n++)
		event_words[n] = 0;

	/* Look at the when() conditions for references to event flags
	 * and assigned variables.  Database variables might have a subscript,
	 * which could be a constant (set a single event bit) or an expression
	 * (set a group of bits for the possible range of the evaluated expression)
	 */
	foreach (tp, sp->state_whens)
	{
		event_mask_args em_args = { event_words, num_event_flags };

		/* look for scalar variables and event flags */
		traverse_expr_tree(tp->when_cond, 1<<E_VAR, 0, 0,
			iter_event_mask_scalar, &em_args);

		/* look for arrays and subscripted array elements */
		traverse_expr_tree(tp->when_cond, (1<<E_VAR)|(1<<E_SUBSCR), 0, 0,
			iter_event_mask_array, &em_args);
	}
#ifdef DEBUG
	report("event mask for state %s is", sp->value);
	for (n = 0; n < num_event_words; n++)
		report(" 0x%lx", event_words[n]);
	report("\n");
#endif
}

/* Iteratee for scalar variables (including event flags). */
static int iter_event_mask_scalar(Expr *ep, Expr *scope, void *parg)
{
	event_mask_args	*em_args = (event_mask_args *)parg;
	Chan		*cp;
	Var		*vp;
	int		num_event_flags = em_args->num_event_flags;
	bitMask		*event_words = em_args->event_words;

	assert(ep->type == E_VAR);
	vp = ep->extra.e_var;
	assert(vp != 0);

	/* this subroutine handles only the scalar variables */
	if (vp->class != VC_SCALAR)
		return FALSE;

	/* event flag? */
	if (vp->type == V_EVFLAG)
	{
#ifdef DEBUG
		report("  iter_event_mask_scalar: evflag: %s, ef_num=%d\n",
		 vp->name, vp->ef_num);
#endif
		bitSet(event_words, vp->ef_num);
		return FALSE;
	}

	cp = vp->chan;
	/* if not associated with channel, return */
	if (cp == 0)
		return FALSE;

	/* value queued via syncQ? */
	if (vp->queued)
	{
		int ef_num = cp->ef_nums[0];
#ifdef DEBUG
		report("  iter_event_mask_scalar: queued var: %s, ef_num=%d\n",
			vp->name, ef_num);
#endif
		bitSet(event_words, ef_num);
	}
	else
	{
		/* otherwise would not be class VC_SCALAR */
		assert(vp->length1 == 1);
#ifdef DEBUG
		report("  iter_event_mask_scalar: var: %s, event bit=%d\n",
			vp->name, cp->index + 1);
#endif
		bitSet(event_words, cp->index + num_event_flags + 1);
	}
	return FALSE;		/* no children anyway */
}

/* Iteratee for array variables. */
static int iter_event_mask_array(Expr *ep, Expr *scope, void *parg)
{
	event_mask_args	*em_args = (event_mask_args *)parg;
	int		num_event_flags = em_args->num_event_flags;
	bitMask		*event_words = em_args->event_words;

	Chan		*cp=0;
	Var		*vp=0;
	Expr		*e_var=0, *e_ix=0;
	int		ix, n;

	assert(ep->type == E_SUBSCR || ep->type == E_VAR);

	if (ep->type == E_SUBSCR)
	{
		e_var = ep->subscr_operand;
		e_ix = ep->subscr_index;
		assert(e_var != 0);
		assert(e_ix != 0);
		if (e_var->type != E_VAR)
			return TRUE;
	}
	if (ep->type == E_VAR)
	{
		e_var = ep;
		e_ix = 0;
	}

	vp = e_var->extra.e_var;
	assert(vp != 0);

	/* this subroutine handles only the array variables */
	if (vp->class != VC_ARRAY1 && vp->class != VC_ARRAY2 && vp->class != VC_ARRAYP)
		return TRUE;

	cp = vp->chan;
	if (cp == NULL)
		return TRUE;		/* not assigned to a pv */

	/* an array variable subscripted with a constant */
	if (e_ix && e_ix->type == E_CONST)
	{
		ix = atoi(e_ix->value);
		if (ix < 0 || ix >= cp->num_elem)
		{
			error_at_expr(e_ix, "subscript out of range\n");
			return FALSE;
		}
#ifdef DEBUG
		report("  iter_event_mask_array: %s, event bit=%d\n",
			vp->name, cp->index + ix + 1);
#endif
		bitSet(event_words, cp->index + ix + num_event_flags + 1);
		return FALSE;	/* important: do NOT descend further
				   otherwise will find the array var and
				   set all the bits (see below) */
	}
	else if (e_ix)	/* subscript is an expression */
	{
		/* must descend for the array variable (see below) and
		   possible array vars inside subscript expression */
		return TRUE;
	}
	else /* no subscript */
	{
		/* set all event bits for this variable */
#ifdef DEBUG
		report("  iter_event_mask_array: %s, event bits=%d..%d\n",
			vp->name, cp->index + 1, cp->index + vp->length1);
#endif
		for (n = 0; n < vp->length1; n++)
		{
			bitSet(event_words, cp->index + n + num_event_flags + 1);
		}
		return FALSE;	/* no children anyway */
	}
}
