/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		 Los Alamos National Laboratory

	Copyright, 2010, Helmholtz-Zentrum Berlin f. Materialien
		und Energie GmbH, Germany (HZB)
		(see file Copyright.HZB included in this distribution)
***************************************************************************
		Generate tables for runtime sequencer
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
#include	"parse.h"
#include	"gen_tables.h"

typedef struct event_mask_args {
	bitMask	*event_words;
	int	num_event_flags;
} event_mask_args;

static void gen_channel_table(ChanList *chan_list, uint num_event_flags, int opt_reent);
static void gen_channel(Chan *cp, uint num_event_flags, int opt_reent);
static void gen_state_table(Expr *ss_list, uint num_event_flags, uint num_channels);
static void fill_state_struct(Expr *sp, char *ss_name);
static void gen_prog_table(Program *p);
static void encode_options(Options options);
static void encode_state_options(StateOptions options);
static void gen_ss_table(SymTable st, Expr *ss_list);
static void gen_state_event_mask(Expr *sp, uint num_event_flags,
	bitMask *event_words, uint num_event_words);
static int iter_event_mask_scalar(Expr *ep, Expr *scope, void *parg);
static int iter_event_mask_array(Expr *ep, Expr *scope, void *parg);

/* Generate all kinds of tables for a SNL program. */
void gen_tables(Program *p)
{
	printf("\n/************************ Tables ************************/\n");
	gen_channel_table(p->chan_list, p->num_event_flags, p->options.reent);
	gen_state_table(p->prog->prog_statesets, p->num_event_flags,
		p->chan_list->num_elems);
	gen_ss_table(p->sym_table, p->prog->prog_statesets);
	gen_prog_table(p);
}

/* Generate channel table with data for each defined channel */
static void gen_channel_table(ChanList *chan_list, uint num_event_flags, int opt_reent)
{
	Chan *cp;

	if (chan_list->first)
	{
		printf("\n/* Channel Definitions */\n");
		printf("static struct seqChan seqChan[] = {\n");
		printf("  /* dbAsName, offset, pVarName, */\n");
		printf("  /* pVarType, count, eventNum, efId, monitored, queued, queueSize, queueIndex */\n");
		foreach (cp, chan_list->first)
		{
			gen_channel(cp, num_event_flags, opt_reent);
			printf("%s", cp->next ? ",\n" : "\n};\n");
		}
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
		printf("%s_%s.%s", VAR_PREFIX, vp->scope->value, vp->name);
	}
	else if (vp->scope->type == D_STATE)
	{
		printf("%s_%s.%s_%s.%s", VAR_PREFIX,
			vp->scope->extra.e_state->var_list->parent_scope->value,
			VAR_PREFIX, vp->scope->value, vp->name);
	}
}

/* Generate a seqChan structure */
static void gen_channel(Chan *cp, uint num_event_flags, int opt_reent)
{
	Var	*vp = cp->var;
	char	elem_str[20] = "";
	uint	ef_num = 0;

	if (vp->assign == M_MULTI)
		sprintf(elem_str, "[%d]", cp->index);

	if (cp->sync)
		ef_num = cp->sync->chan.evflag->index;
	else if (cp->syncq)
		ef_num = cp->syncq->ef_var->chan.evflag->index;

	if (cp->name == NULL)
		printf("  {NULL, ");
	else
		printf("  {\"%s\", ", cp->name);

	if (opt_reent)
	{
		printf("offsetof(struct %s, ", VAR_PREFIX);
		gen_var_name(vp);
		printf("%s), ", elem_str);
	}
	else
	{
		printf("(char*)&");
		gen_var_name(vp);
		printf("%s - (char*)0, ", elem_str);
	}

	/* variable name with optional elem num */
	printf("\"%s%s\",\n", vp->name, elem_str);
	/* variable type */
	printf("    \"%s\", ", type_name(type_base_type(vp->type)));
	/* count, for requests */
	printf("%d, ", cp->count);
	/* event number */
	printf("%d, ", num_event_flags + vp->index + cp->index + 1);
	/* event flag number (or 0) */
	printf("%d, ", ef_num);
	/* monitor flag */
	printf("%d, ", cp->monitor);
	/* syncQ queue */
	if (!cp->syncq)
		printf("0, 0, 0");
	else if (!cp->syncq->size)
		printf("1, MAX_QUEUE_SIZE, %d", cp->syncq->index);
	else
		printf("1, %d, %d", cp->syncq->size, cp->syncq->index);
	printf("}");
}

/* Generate state event mask and table */
static void gen_state_table(Expr *ss_list, uint num_event_flags, uint num_channels)
{
	Expr	*ssp;
	Expr	*sp;
	uint	n;
	uint	num_event_words = (num_event_flags + num_channels + NBITS)/NBITS;
	bitMask	event_mask[num_event_words];

	/* NOTE: Bit zero of event mask is not used. Bit 1 to num_event_flags
	   are used for event flags, then come channels. */

	/* For each state set... */
	foreach (ssp, ss_list)
	{
		/* For each state, generate event mask array */
		printf("\n/* Event masks for state set %s */\n", ssp->value);
		foreach (sp, ssp->ss_states)
		{
			gen_state_event_mask(sp, num_event_flags, event_mask, num_event_words);
			printf("/* state %s: */\n", sp->value);
			printf("static bitMask\tEM_%s_%s[] = {\n",
				ssp->value, sp->value);
			for (n = 0; n < num_event_words; n++)
				printf("\t0x%08lx,\n", event_mask[n]);
			printf("};\n");
		}

		/* For each state, generate state structure */
		printf("\n/* State Table */\n");
		printf("static struct seqState state_%s[] = {\n", ssp->value);
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
	printf("\t/* num. channels */     %d,\n", p->chan_list->num_elems);
	printf("\t/* state sets */        seqSS,\n");
	printf("\t/* num. state sets */   %d,\n", p->num_ss);
	if (p->options.reent)
		printf("\t/* user var size */     sizeof(struct %s),\n", VAR_PREFIX);
	else
		printf("\t/* user var size */     0,\n");
	printf("\t/* param */             \"%s\",\n", p->param);
	printf("\t/* num. event flags */  %d,\n", p->num_event_flags);
	printf("\t/* encoded options */   "); encode_options(p->options);
	printf("\t/* init func */         global_prog_init,\n");
	printf("\t/* entry func */        %s,\n", p->prog->prog_entry?"global_prog_entry":"NULL");
	printf("\t/* exit func */         %s,\n", p->prog->prog_exit?"global_prog_exit":"NULL");
	printf("\t/* num. queues */       %d\n", p->syncq_list->num_elems);
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
	if (options.reent)
		printf(" | OPT_REENT");
	if (options.safe)
		printf(" | OPT_SAFE");
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
		if (num_ss > 0)
			printf("\n");
		num_ss++;
		printf("\t/* State set \"%s\" */ {\n", ssp->value);
		printf("\t/* ss name */           \"%s\",\n", ssp->value);
		printf("\t/* state struct */      state_%s,\n", ssp->value);
		printf("\t/* number of states */  %d,\n", ssp->extra.e_ss->num_states);
		printf("\t/* number of delays */  %d},\n", ssp->extra.e_ss->num_delays);
	}
	printf("};\n");
}

/* Generate event mask for a single state. The event mask has a bit set for each
   event flag and for each process variable (assigned var) used in one of the
   state's when() conditions. The bits from 1 to num_event_flags are for the
   event flags. The bits from num_event_flags+1 to num_event_flags+num_channels
   are for process variables. Bit zero is not used for whatever mysterious reason
   I cannot tell. */
static void gen_state_event_mask(Expr *sp, uint num_event_flags,
	bitMask *event_words, uint num_event_words)
{
	uint	n;
	Expr	*tp;

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

	/* this subroutine handles only the scalar variables and event flags */
#if 0
	if (vp->class != VC_SCALAR && vp->class != VC_EVFLAG)
#endif
	if (vp->type->tag < V_CHAR || vp->type->tag >= V_POINTER)
		return FALSE;		/* no children anyway */

	/* event flag? */
	if (vp->type->tag == V_EVFLAG)
	{
#ifdef DEBUG
		report("  iter_event_mask_scalar: evflag: %s, ef_num=%d\n",
			vp->name, vp->chan.evflag->index);
#endif
		bitSet(event_words, vp->chan.evflag->index);
		return FALSE;		/* no children anyway */
	}

	/* if not associated with channel, return */
	if (vp->assign == M_NONE)
		return FALSE;
	assert(vp->assign == M_SINGLE);		/* by L3 */
	cp = vp->chan.single;

	/* value queued via syncQ? */
	if (vp->syncq != M_NONE)
	{
		int ef_num;
		assert(vp->syncq == M_SINGLE);	/* by L1 */
		ef_num = cp->syncq->ef_var->chan.evflag->index;
#ifdef DEBUG
		report("  iter_event_mask_scalar: queued var: %s, ef_num=%d\n",
			vp->name, ef_num);
#endif
		bitSet(event_words, ef_num);
	}
	else
	{
#ifdef DEBUG
		report("  iter_event_mask_scalar: var: %s, event bit=%d\n",
			vp->name, vp->index + cp->index + num_event_flags + 1);
#endif
		bitSet(event_words, vp->index + cp->index + num_event_flags + 1);
	}
	return FALSE;		/* no children anyway */
}

/* Iteratee for array variables. */
static int iter_event_mask_array(Expr *ep, Expr *scope, void *parg)
{
	event_mask_args	*em_args = (event_mask_args *)parg;
	uint		num_event_flags = em_args->num_event_flags;
	bitMask		*event_words = em_args->event_words;

	Var		*vp=0;
	Expr		*e_var=0, *e_ix=0;

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
	if (vp->type->tag != V_ARRAY)
		return TRUE;

	if (vp->assign == M_NONE)
	{
		return FALSE;
	}
	else if (vp->assign == M_SINGLE)
	{
		uint ix = vp->chan.single->index;
#ifdef DEBUG
		report("  iter_event_mask_array: %s, event bit=%d\n",
			vp->name, vp->index + ix + num_event_flags + 1);
#endif
		bitSet(event_words, vp->index + ix + num_event_flags + 1);
		return TRUE;
	}
	else
	{
		uint length1 = type_array_length1(vp->type);

		assert(vp->assign == M_MULTI);
		/* an array variable subscripted with a constant */
		if (e_ix && e_ix->type == E_CONST)
		{
			uint ix;

			if (!strtoui(e_ix->value, length1, &ix))
			{
				error_at_expr(e_ix,
					"subscript in '%s[%s]' out of range\n",
					vp->name, e_ix->value);
				return FALSE;
			}
#ifdef DEBUG
			report("  iter_event_mask_array: %s, event bit=%d+%d+%d+1=%d\n",
				vp->name, vp->index, ix, num_event_flags,
                                vp->index + ix + num_event_flags + 1);
#endif
			bitSet(event_words, vp->index + ix + num_event_flags + 1);
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
			uint ix;
#ifdef DEBUG
			report("  iter_event_mask_array: %s, event bits=%d..%d\n",
				vp->name, vp->index + num_event_flags + 1,
				vp->index + num_event_flags + length1);
#endif
			for (ix = 0; ix < length1; ix++)
			{
				bitSet(event_words, vp->index + ix + num_event_flags + 1);
			}
			return FALSE;	/* no children anyway */
		}
	}
}
