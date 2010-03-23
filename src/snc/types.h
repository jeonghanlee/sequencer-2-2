#ifndef INCLtypesh
#define INCLtypesh

#include <epicsVersion.h>

#ifndef	TRUE
#define	TRUE 1
#define	FALSE 0
#endif

typedef	struct sym_table	SymTable;
typedef struct options		Options;
typedef struct state_options	StateOptions;
typedef struct token		Token;
typedef struct when		When;
typedef struct state		State;
typedef struct state_set	StateSet;
typedef struct program		Program;
typedef struct channel		Chan;
typedef struct expression	Expr;
typedef struct variable		Var;
typedef struct chan_list	ChanList;
typedef struct var_list		VarList;
typedef struct expr_pair	ExprPair;

typedef enum var_type		VarType;
typedef enum var_class		VarClass;
typedef enum expr_type		ExprType;

struct sym_table
{
#if EPICS_REVISION<14 || (EPICS_REVISION==14 && EPICS_MODIFICATION <= 10)
	void *table;
#else
	struct gphPvt *table;
#endif
};

struct options				/* run-time options */
{
	unsigned async:1;		/* do pvGet() asynchronously */
	unsigned conn:1;		/* wait for all conns to complete */
	unsigned debug:1;		/* run-time debug */
	unsigned reent:1;		/* reentrant at run-time */
	unsigned newef:1;		/* new event flag mode */
	unsigned main:1;		/* main program */

					/* compile time options */
	unsigned init_reg:1;		/* register commands/programs */
	unsigned line:1;		/* line numbering */
	unsigned warn:1;		/* compiler warnings */
};

#define DEFAULT_OPTIONS (Options){0,1,0,0,1,0,1,1,1}

struct state_options			/* run-time state options */
{
	unsigned do_reset_timers:1;	/* reset timers on state entry from self */
	unsigned no_entry_from_self:1;	/* don't do entry actions if entering from same state */
	unsigned no_exit_to_self:1;	/* don't do exit actions if exiting to same state */
};

#define DEFAULT_STATE_OPTIONS (StateOptions){1,1,1}

struct token				/* for the lexer and parser */
{
	char	*str;
	char	*file;
	int	line;
};

struct when				/* extra data for when clauses */
{
	Expr		*next_state;	/* declaration of target state */
	VarList		*var_list;	/* list of local variables */
};

struct state				/* extra data for state clauses */
{
	int		index;		/* index in array of seqState structs */
	StateOptions	options;	/* state options */
	VarList		*var_list;	/* list of 'local' variables */
};

struct state_set			/* extra data for state set clauses */
{
	int	num_states;		/* number of states, useful for iteration */
	VarList	*var_list;		/* list of 'local' variables */
};

struct expression			/* generic syntax node */
{
	Expr	*next;			/* list node: next expression */
	Expr	*last;			/* list node: last expression */
	Expr	**children;		/* array of children [left,right,...] */
	int	type;			/* expression type (E_XXX) */
	char	*value;			/* operator or value string */
	int	line_num;		/* originating line number */
	char	*src_file;		/* originating source file */
	union				/* extra data, depends on type */
	{
		Var	*e_var;		/* variable definiton */
		Var	*e_decl;	/* variable definiton */
		int	e_delay;	/* delay id */
		int	e_option;	/* option value (1 or 0) */
		VarList	*e_prog;	/* top-level definitions */
		StateSet *e_ss;		/* state set data */
		State	*e_state;	/* state data */
		When	*e_when;	/* transition data */
		Expr	*e_change;	/* declaration of target state */
		VarList	*e_entry;	/* local declarations */
		VarList	*e_exit;	/* local declarations */
		VarList	*e_cmpnd;	/* block local definitions */
	}	extra;
};

struct variable				/* Variable or function definition */
{
	Var	*next;			/* link to next variable in list */
	char	*name;			/* variable name */
	Expr	*value;			/* initial value or NULL */
	int	type;			/* var type, see enum var_type */
	int	class;			/* var class, see enum var_class */
	int	length1;		/* 1st dim. array lth (default=1) */
	int	length2;		/* 2nd dim. array lth (default=1) */
	int	ef_num;			/* bit number if this is an event flag */
	Chan	*chan;			/* ptr to channel struct if assigned */
	int	queued;			/* whether queued via syncQ */
	int	maxqsize;		/* max syncQ queue size */
	int	qindex;			/* index in syncQ queue array */
	Expr	*decl;			/* declaration of this variable (or NULL) */
	Expr	*scope;			/* declaration of this variable (or NULL) */
};

struct channel				/* channel assignment info */
{
	Chan	*next;			/* link to next channel in list */
	char	**pv_names;		/* array of pv names */
	int	num_elem;		/* number of elements */
	Var	*var;			/* variable definition */
	int	count;			/* request count for pv access */
	int	*mon_flags;		/* array of monitor flags */
	Var	**ef_vars;		/* array of event flag variables */
	int	*ef_nums;		/* array of event flag numbers (for sync'ed vars) */
	int	index;			/* index in channel array (seqChan) */
};

struct chan_list
{
	Chan	*first, *last;		/* first and last member of the list */
};

struct var_list
{
	Var	*first, *last;		/* first and last member of the list */
	Expr	*parent_scope;		/* next surrounding scope */
};

struct expr_pair
{
	Expr	*left, *right;
};

struct program
{
	/* result of parsing phase */
	Expr	*prog;			/* prog of syntax tree */

	/* these point into children of the prog node, for convenience */
	char	*name;			/* ptr to program name (string) */
	char	*param;			/* parameter string for program stmt */

	/* these are calculated by the analysis phase */
	Options options;		/* program options, from source or command line */
	SymTable sym_table;		/* symbol table */
	ChanList *chan_list;		/* channel list */
	int	num_channels;		/* number of channels */
	int	num_event_flags;	/* number of event flags */
	int	num_queues;		/* number of syncQ queues */
	int	num_ss;			/* number of state sets */
};

/* Structure allocation */
#define new(type)		(type *)calloc(1, sizeof(type))

/* Generic iteration on lists */
#define foreach(e,l)		for (e = l; e != 0; e = e->next)

/* #define bits(et)		(1<<et) */

/* Expression types that are scopes. By definition, a scope is an expression
   that allows variable declarations as (immediate) subexpressions. */
#define scope_mask		( (1<<D_PROG) | (1<<D_SS) | (1<<D_STATE)\
				| (1<<D_ENTRY) | (1<<D_EXIT) | (1<<D_WHEN) | (1<<S_CMPND) )
#define is_scope(e)		(((1<<((e)->type)) & scope_mask) != 0)
/* Expressions types that may have sub-scopes */
#define has_sub_scope_mask	( (1<<D_ENTRY) | (1<<D_EXIT) | (1<<D_PROG) | (1<<D_SS)\
				| (1<<D_STATE) | (1<<D_WHEN) | (1<<S_CMPND) | (1<<S_FOR)\
				| (1<<S_IF) | (1<<S_STMT) | (1<<S_WHILE) )
/* Expressions types that may have sub-expressions */
#define has_sub_expr_mask	( (1<<D_DECL) | (1<<D_ENTRY) | (1<<D_EXIT) | (1<<D_PROG)\
				| (1<<D_SS) | (1<<D_STATE) | (1<<D_SYNC) | (1<<D_SYNCQ)\
				| (1<<D_WHEN) | (1<<E_BINOP)\
				| (1<<E_DELAY) | (1<<E_FUNC) | (1<<E_PAREN) | (1<<E_POST)\
				| (1<<E_PRE) | (1<<E_SUBSCR) | (1<<E_TERNOP) | (1<<E_VAR)\
				| (1<<S_CHANGE) | (1<<S_CMPND) | (1<<S_FOR) | (1<<S_IF)\
				| (1<<S_STMT) | (1<<S_WHILE) )
/* Expressions types that may have sub-scopes */
#define	has_scope_mask		( scope_mask | (1<<S_FOR) | (1<<S_IF) | (1<<S_WHILE) )
/* Expression types that are actually expressions i.e. no definitions or statements.
   These are the ones that start with E_. */
#define	expr_mask		( (1<<E_BINOP) | (1<<E_CONST) | (1<<E_DELAY) | (1<<E_FUNC)\
				| (1<<E_PAREN) | (1<<E_POST) | (1<<E_PRE) | (1<<E_STRING)\
				| (1<<E_SUBSCR) | (1<<E_TERNOP) | (1<<E_VAR) | (1<<T_TEXT) )

#define expr_type_name(e)	expr_type_info[(e)->type].name

/* Variable types */
enum var_type
{
	V_NONE,			/* undeclared */
	V_CHAR,			/* char */
	V_SHORT,		/* short */
	V_INT,			/* int */
	V_LONG,			/* long */
	V_FLOAT,		/* float */
	V_DOUBLE,		/* double */
	V_STRING,		/* strings (array of char) */
	V_EVFLAG,		/* event flag */
	V_UCHAR,		/* unsigned char */
	V_USHORT,		/* unsigned short */
	V_UINT,			/* unsigned int */
	V_ULONG			/* unsigned long */
};

/* Variable classes */
enum var_class
{
	VC_SCALAR,		/* scalar variable */
	VC_ARRAY1,		/* 1-dimensional array */
	VC_ARRAY2,		/* 2-dimensional array */
	VC_POINTER,		/* pointer */
	VC_ARRAYP		/* array of pointers */
};

/* Expression types */
enum expr_type			/* description [child expressions...] */
{
	D_ASSIGN,		/* assign statement [subscr,pvs] */
	D_DECL,			/* variable declaration [] */
	D_ENTRY,		/* entry statement [defns,stmts] */
	D_EXIT,			/* exit statement [defns,stmts] */
	D_MONITOR,		/* monitor statement [subscr] */
	D_OPTION,		/* option definition [] */
	D_PROG,			/* whole program [param,defns,entry,statesets,exit,ccode] */
	D_SS,			/* state set statement [defns,states] */
	D_STATE,		/* state statement [defns,entry,whens,exit] */
	D_SYNC,			/* sync statement [subscr,evflag] */
	D_SYNCQ,		/* syncq statement [subscr,evflag,maxqsize] */
	D_WHEN,			/* when statement [cond,defns,stmts] */

	E_BINOP,		/* binary operator [left,right] */
	E_CONST,		/* numeric (inkl. character) constant [] */
	E_DELAY,		/* delay function call [args] */
	E_FUNC,			/* function call [args] */
	E_PAREN,		/* parenthesis around an expression [expr] */
	E_POST,			/* unary postfix operator [operand] */
	E_PRE,			/* unary prefix operator [operand] */
	E_STRING,		/* string constant [] */
	E_SUBSCR,		/* subscripted expr [operand,index] */
	E_TERNOP,		/* ternary operator [cond,then,else] */
	E_VAR,			/* variable [] */

	S_CHANGE,		/* state change statement [] */
	S_CMPND,		/* compound statement [defns,stmts] */
	S_FOR,			/* for statement [init,cond,iter,stmt] */
	S_IF,			/* if statement [cond,then,else] */
	S_JUMP,			/* break or continue stmt [] */
	S_STMT,			/* simple statement, i.e. 'expr;'  [expr] */
	S_WHILE,		/* while statement [cond,stmt] */

	T_TEXT			/* C code or other text to be inserted [] */
};
/* CAUTION: must not be more than 32 expression types */

/* Accessors for child expressions. Would like to define structs for the
   various expression types with children, but then we could no longer
   uniformly iterate over all children... */
#define assign_subscr	children[0]
#define assign_pvs	children[1]
#define binop_left	children[0]
#define binop_right	children[1]
#define cmpnd_defns	children[0]
#define cmpnd_stmts	children[1]
#define delay_args	children[0]
#define entry_defns	children[0]
#define entry_stmts	children[1]
#define exit_defns	children[0]
#define exit_stmts	children[1]
#define for_init	children[0]
#define for_cond	children[1]
#define for_iter	children[2]
#define for_stmt	children[3]
#define func_args	children[0]
#define if_cond		children[0]
#define if_then		children[1]
#define if_else		children[2]
#define monitor_subscr	children[0]
#define paren_expr	children[0]
#define post_operand	children[0]
#define pre_operand	children[0]
#define prog_param	children[0]
#define prog_defns	children[1]
#define prog_entry	children[2]
#define prog_statesets	children[3]
#define prog_exit	children[4]
#define prog_ccode	children[5]
#define ss_defns	children[0]
#define ss_states	children[1]
#define state_defns	children[0]
#define state_entry	children[1]
#define state_whens	children[2]
#define state_exit	children[3]
#define stmt_expr	children[0]
#define subscr_operand	children[0]
#define subscr_index	children[1]
#define sync_subscr	children[0]
#define sync_evflag	children[1]
#define syncq_subscr	children[0]
#define syncq_evflag	children[1]
#define syncq_maxqsize	children[2]
#define ternop_cond	children[0]
#define ternop_then	children[1]
#define ternop_else	children[2]
#define when_cond	children[0]
#define when_defns	children[1]
#define when_stmts	children[2]
#define while_cond	children[0]
#define while_stmt	children[1]

#ifndef expr_type_GLOBAL
extern
#endif
struct expr_type_info
{
	const char *name;
	const int num_children;
}
expr_type_info[]
#ifdef expr_type_GLOBAL
= {
	{ "D_ASSIGN",	2 },
	{ "D_DECL",	0 },
	{ "D_ENTRY",	2 },
	{ "D_EXIT",	2 },
	{ "D_MONITOR",	1 },
	{ "D_OPTION",	0 },
	{ "D_PROG",	6 },
	{ "D_SS",	2 },
	{ "D_STATE",	4 },
	{ "D_SYNC",	2 },
	{ "D_SYNCQ",	3 },
	{ "D_WHEN",	3 },
	{ "E_BINOP",	2 },
	{ "E_CONST",	0 },
	{ "E_DELAY",	1 },
	{ "E_FUNC",	1 },
	{ "E_PAREN",	1 },
	{ "E_POST",	1 },
	{ "E_PRE",	1 },
	{ "E_STRING",	0 },
	{ "E_SUBSCR",	2 },
	{ "E_TERNOP",	3 },
	{ "E_VAR",	0 },
	{ "S_CHANGE",	0 },
	{ "S_CMPND",	2 },
	{ "S_FOR",	4 },
	{ "S_IF",	3 },
	{ "S_JUMP",	0 },
	{ "S_STMT",	1 },
	{ "S_WHILE",	2 },
	{ "T_TEXT",	0 }
};
#else
;
#endif

#endif	/*INCLtypesh*/
