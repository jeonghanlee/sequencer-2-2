#ifndef INCLtypesh
#define INCLtypesh

#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif

typedef struct options		Options;
typedef struct token		Token;
typedef struct scope		Scope;
typedef struct program		Program;
typedef struct channel		Chan;
typedef struct expression	Expr;
typedef struct variable		Var;
typedef struct chan_list	ChanList;
typedef struct var_list		VarList;

typedef enum var_type		VarType;
typedef enum var_class		VarClass;
typedef enum expr_type		ExprType;

struct options			/* compile & run-time options */
{
	int	async;		/* do pvGet() asynchronously */
	int	conn;		/* wait for all conns to complete */
	int	debug;		/* run-time debug */
	int	newef;		/* new event flag mode */
	int	init_reg;	/* register commands/programs */
	int	line;		/* line numbering */
	int	main;		/* main program */
	int	reent;		/* reentrant at run-time */
	int	warn;		/* compiler warnings */
};

struct token
{
	char	*str;
	char	*file;
	int	line;
};

#define tok(s)	(Token){s,0,0}

struct	scope
{
	VarList	*var_list;
	Scope	*next;
};

struct	expression			/* Expression block */
{
	Expr	*next;			/* link to next expression */
	Expr	*last;			/* link to last in list */
	Expr	*left;			/* ptr to left expression */
	Expr	*right;			/* ptr to right expression */
	int	type;			/* expression type (E_*) */
	char	*value;			/* operator or value string */
	int	line_num;		/* effective line number */
	char	*src_file;		/* effective source file */
	Scope	*scope;			/* local definitions */
};

struct	variable			/* Variable or function definition */
{
	struct	variable *next;		/* link to next item in list */
	char	*name;			/* variable name */
	char	*value;			/* initial value or NULL */
	int	type;			/* var type */
	int	class;			/* simple, array, or pointer */
	int	length1;		/* 1st dim. array lth (default=1) */
	int	length2;		/* 2nd dim. array lth (default=1) */
	int	ef_num;			/* bit number if this is an event flag */
	struct	channel *chan;		/* ptr to channel struct if assigned */
	int	queued;			/* whether queued via syncQ */
	int	maxQueueSize;		/* max syncQ queue size */
	int	queueIndex;		/* index in syncQ queue array */
	int	line_num;		/* line number */
};

struct	channel				/* DB channel assignment info */
{
	struct	channel *next;		/* link to next item in list */
	char	*db_name;		/* database name (assign all to 1 pv) */
	char	**db_name_list;		/* list of db names (assign each to a pv) */
	int	num_elem;		/* number of elements assigned in db_name_list */
	Var	*var;			/* ptr to variable definition */
	int	count;			/* count for db access */
	int	mon_flag;		/* TRUE if channel is "monitored" */
	int	*mon_flag_list;		/* ptr to list of monitor flags */
	Var	*ef_var;		/* ptr to event flag variable for sync */
	Var	**ef_var_list;		/* ptr to list of event flag variables */
	int	ef_num;			/* event flag number */
	int	*ef_num_list;		/* list of event flag numbers */
	int	index;			/* index in database channel array (seqChan) */
};
/* Note: Only one of db_name or db_name_list can have a non-zero value */

struct chan_list
{
	Chan	*first, *last;
};

struct var_list
{
	Var	*first, *last;
};

struct program				/* result of parsing an SNL program */
{
	/* parser generates these */
	char	*name;			/* ptr to program name (string) */
	char	*param;			/* parameter string for program stmt */
	Expr	*global_defn_list;	/* global definition list */
	Expr	*ss_list;		/* state set list */
	Expr	*entry_code_list;	/* entry code list */
	Expr	*exit_code_list;	/* exit code list */
	Expr	*global_c_list;		/* global C code following state program */

	/* these get added by later stages */
	Options *options;		/* program options, from source or command line */
	Scope	*global_scope;		/* global scope */
	ChanList *chan_list;		/* channel list */
	int	num_channels;		/* number of db channels */
	int	num_events;		/* number of event flags */
	int	num_queues;		/* number of syncQ queues */
	int	num_ss;			/* number of state sets */
};

/* Allocation 'functions' */
#define	allocExpr()		(Expr *)calloc(1, sizeof(Expr))
#define	allocVar()		(Var *)calloc(1, sizeof(Var))
#define	allocChan()		(Chan *)calloc(1, sizeof(Chan))
#define	allocVarList()		(VarList *)calloc(1, sizeof(VarList))
#define	allocChanList()		(ChanList *)calloc(1, sizeof(ChanList))
#define	allocScope()		(Scope *)calloc(1, sizeof(Scope))
#define	allocProgram()		(Program *)calloc(1, sizeof(Program))

/* Variable types */
enum var_type
{
	V_NONE,			/* not defined */
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
	VC_SIMPLE,		/* simple (un-dimensioned) variable */
	VC_ARRAY1,		/* single dim. array */
	VC_ARRAY2,		/* multiple dim. array */
	VC_POINTER,		/* pointer */
	VC_ARRAYP		/* array of pointers */
};

/* Expression types */
enum expr_type
{
	E_CONST,		/* numeric constant */
	E_STRING,		/* ptr to string constant */
	E_VAR,			/* variable */
	E_PAREN,		/* parenthesis around an expression */
	E_FUNC,			/* function */
	E_SUBSCR,		/* subscript: expr[expr] */
	E_POST,			/* unary postfix operator: expr OP */
	E_PRE,			/* unary prefix operator: OP expr */
	E_BINOP,		/* binary operator: expr OP expr */
	E_TERNOP,		/* ternary operator: expr OP expr OP expr */
	E_TEXT,			/* C code or other text to be inserted */
	E_STMT,			/* simple statement */
	E_CMPND,		/* begin compound statement: {...} */
	E_IF,			/* if statement */
	E_ELSE,			/* else statement */
	E_WHILE,		/* while statement */
	E_SS,			/* state set statement */
	E_STATE,		/* state statement */
	E_WHEN,			/* when statement */
	E_FOR,			/* for statement */
	E_X,			/* eXpansion (e.g. for(;;) */
	E_BREAK,		/* break stmt */
	E_DECL,			/* declaration statement */
	E_ENTRY,		/* entry statement */
	E_EXIT,			/* exit statement */
	E_OPTION,		/* state option statement */
	E_ASSIGN,		/* assign statement */
	E_MONITOR,		/* monitor statement */
	E_SYNC,			/* sync statement */
	E_SYNCQ			/* syncq statement */
};

#ifdef expr_type_GLOBAL
const char *expr_type_names[] =
{
	"E_CONST",
	"E_STRING",
	"E_VAR",
	"E_PAREN",
	"E_FUNC",
	"E_SUBSCR",
	"E_POST",
	"E_PRE",
	"E_BINOP",
	"E_TERNOP",
	"E_TEXT",
	"E_STMT",
	"E_CMPND",
	"E_IF",
	"E_ELSE",
	"E_WHILE",
	"E_SS",
	"E_STATE",
	"E_WHEN",
	"E_FOR",
	"E_X",
	"E_BREAK",
	"E_DECL",
	"E_ENTRY",
	"E_EXIT",
	"E_OPTION",
	"E_ASSIGN",
	"E_MONITOR",
	"E_SYNC",
	"E_SYNCQ"
};
#else
extern const char *expr_type_names[];
#endif

#endif	/*INCLtypesh*/
