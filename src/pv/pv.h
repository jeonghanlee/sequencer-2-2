/* $Id: pv.h,v 1.1.1.1 2000-04-04 03:22:13 wlupton Exp $
 *
 * Definitions for EPICS sequencer message system-independent library (pv)
 * (NB, "pv" = "process variable").
 *
 * This is a simple layer which is specifically designed to provide the
 * facilities needed by the EPICS sequencer. Specific message systems are
 * expected to inherit from the pv classes.
 *
 * William Lupton, W. M. Keck Observatory
 */

#ifndef INCLpvh
#define INCLpvh

#include "osiThread.h"		/* for thread ids */
#include "osiSem.h"		/* for locks */
#include "tsStamp.h"		/* for time stamps */

#include "pvAlarm.h"		/* status and severity definitions */

/*
 * Standard FALSE and TRUE macros
 */
#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

/*
 * Magic number for validating structures / versions
 */
#define PV_MAGIC 0xfeddead	/* ...a sad tale of food poisoning? */

/*
 * Enum for data types (very restricted set of types)
 */
typedef enum {
    pvTypeERROR       = -1,
    pvTypeCHAR        = 0,
    pvTypeSHORT       = 1,
    pvTypeLONG        = 2,
    pvTypeFLOAT       = 3,
    pvTypeDOUBLE      = 4,
    pvTypeSTRING      = 5,
    pvTypeTIME_CHAR   = 6,
    pvTypeTIME_SHORT  = 7,
    pvTypeTIME_LONG   = 8,
    pvTypeTIME_FLOAT  = 9,
    pvTypeTIME_DOUBLE = 10,
    pvTypeTIME_STRING = 11
} pvType;

#define PV_SIMPLE(_type) ( (_type) <= pvTypeSTRING )

/*
 * Value-related types (c.f. db_access.h)
 */
typedef char   pvChar;
typedef short  pvShort;
typedef long   pvLong;
typedef float  pvFloat;
typedef double pvDouble;
typedef char   pvString[256]; /* use sizeof( pvString ) */

#define PV_TIME_XXX(_type) \
    typedef struct { \
	pvStat	  status; \
	pvSevr    severity; \
	TS_STAMP  stamp; \
	pv##_type value[1]; \
    } pvTime##_type

PV_TIME_XXX( Char   );
PV_TIME_XXX( Short  );
PV_TIME_XXX( Long   );
PV_TIME_XXX( Float  );
PV_TIME_XXX( Double );
PV_TIME_XXX( String );

typedef union {
    pvChar       charVal[1];
    pvShort      shortVal[1];
    pvLong       longVal[1];
    pvFloat      floatVal[1];
    pvDouble     doubleVal[1];
    pvString     stringVal[1];
    pvTimeChar   timeCharVal;
    pvTimeShort  timeShortVal;
    pvTimeLong   timeLongVal;
    pvTimeFloat  timeFloatVal;
    pvTimeDouble timeDoubleVal;
    pvTimeString timeStringVal;
} pvValue;

#define PV_VALPTR(_type,_value) \
    ( ( PV_SIMPLE(_type) ? ( void * ) ( _value ) : \
			   ( void * ) ( &_value->timeCharVal.value ) ) )

/*
 * Connect (connect/disconnect and event (get, put and monitor) functions
 */
typedef void (*pvConnFunc)( void *var, int connected );

typedef void (*pvEventFunc)( void *var, pvType type, int count,
			     pvValue *value, void *arg, pvStat status );

/*
 * Most of the rest is C++. The C interface is at the bottom.
 */
#ifdef __cplusplus

/*
 * Forward references
 */
class pvVariable;
class pvCallback;

////////////////////////////////////////////////////////////////////////////////
/*
 * System
 *
 * This is somewhat analogous to a cdevSystem object (CA has no equivalent)
 */

class pvSystem {

public:
    pvSystem( int debug = 0 );
    virtual ~pvSystem();

    inline pvSystem *getSystem() { return this; } 

    virtual pvStat attach() { return pvStatOK; }
    virtual pvStat flush() { return pvStatOK; }
    virtual pvStat pend( double seconds = 0.0, int wait = FALSE ) = 0;

    virtual pvVariable *newVariable( const char *name, pvConnFunc func = NULL,
				     void *priv = NULL, int debug = 0 ) = 0;

    void lock();
    void unlock();

    inline int getMagic() const { return magic_; }
    inline void setDebug( int debug ) { debug_ = debug; }
    inline int getDebug() const { return debug_; }

    void setError( int status, pvSevr sevr, pvStat stat, const char *mess );
    inline int getStatus() const { return status_; }
    inline pvSevr getSevr() const { return sevr_; }
    inline pvStat getStat() const { return stat_; }
    inline void setStatus( int status ) { status_ = status; }
    inline void setStat( pvStat stat ) { stat_ = stat; }
    inline char *getMess() const { return mess_?mess_:(char *)""; }

private:
    int		magic_;		/* magic number (used for authentication) */
    int		debug_;		/* debugging level (inherited by pvs) */

    int		status_;	/* message system-specific status code */
    pvSevr	sevr_;		/* severity */
    pvStat	stat_;		/* status */
    char	*mess_;		/* error message */

    semMutexId	lock_;		/* prevents more than one thread in library */
};

////////////////////////////////////////////////////////////////////////////////
/*
 * Process variable
 *
 * This is somewhat analogous to a cdevDevice object (or a CA channel)
 */
class pvVariable {

public:
    // private data is constructor argument so that it is guaranteed set
    // before connection callback is invoked
    pvVariable( pvSystem *system, const char *name, pvConnFunc func = NULL,
		void *priv = NULL, int debug = 0 );
    virtual ~pvVariable();

    virtual pvStat get( pvType type, int count, pvValue *value ) = 0;
    virtual pvStat getNoBlock( pvType type, int count, pvValue *value ) = 0;
    virtual pvStat getCallback( pvType type, int count,
		pvEventFunc func, void *arg = NULL ) = 0;
    virtual pvStat put( pvType type, int count, pvValue *value ) = 0;
    virtual pvStat putNoBlock( pvType type, int count, pvValue *value ) = 0;
    virtual pvStat putCallback( pvType type, int count, pvValue *value,
		pvEventFunc func, void *arg = NULL ) = 0;
    virtual pvStat monitorOn( pvType type, int count,
		pvEventFunc func, void *arg = NULL,
		pvCallback **pCallback = NULL ) = 0;
    virtual pvStat monitorOff( pvCallback *callback = NULL ) = 0;

    virtual int getConnected() const = 0;
    virtual pvType getType() const = 0;
    virtual int getCount() const = 0;

    inline int getMagic() const { return magic_; }
    inline void setDebug( int debug ) { debug_ = debug; }
    inline int getDebug() const { return debug_; }
    inline pvConnFunc getFunc() const { return func_; }

    inline pvSystem *getSystem() const { return system_; }
    inline char *getName() const { return name_; }
    inline void setPrivate( void *priv ) { private_ = priv; }
    inline void *getPrivate() const { return private_; }

    void setError( int status, pvSevr sevr, pvStat stat, const char *mess );
    inline int getStatus() const { return status_; }
    inline pvSevr getSevr() const { return sevr_; }
    inline pvStat getStat() const { return stat_; }
    inline void setStatus( int status ) { status_ = status; }
    inline void setStat( pvStat stat ) { stat_ = stat; }
    inline char *getMess() const { return mess_?mess_:(char *)""; }

private:
    int		magic_;		/* magic number (used for authentication) */
    int		debug_;		/* debugging level (inherited from system) */
    pvConnFunc	func_;		/* connection state change function */

    pvSystem	*system_;	/* associated system */
    char	*name_;		/* variable name */
    void	*private_;	/* client's private data */

    int		status_;	/* message system-specific status code */
    pvSevr	sevr_;		/* severity */
    pvStat	stat_;		/* status */
    char	*mess_;		/* error message */
};

////////////////////////////////////////////////////////////////////////////////
/*
 * Callback
 *
 * This is somewhat analogous to a cdevCallback object
 */
class pvCallback {

public:
    pvCallback( pvVariable *variable, pvType type, int count,
		pvEventFunc func, void *arg, int debug = 0);
    ~pvCallback();

    inline int getMagic() { return magic_; }
    inline void setDebug( int debug ) { debug_ = debug; }
    inline int getDebug() { return debug_; }

    inline pvVariable *getVariable() { return variable_; }
    inline pvType getType() { return type_; }
    inline int getCount() { return count_; };
    inline pvEventFunc getFunc() { return func_; };
    inline void *getArg() { return arg_; };
    inline void setPrivate( void *priv ) { private_ = priv; }
    inline void *getPrivate() { return private_; }

private:
    int		magic_;		/* magic number (used for authentication) */
    int		debug_;		/* debugging level (inherited from variable) */

    pvVariable  *variable_;	/* associated variable */
    pvType	type_;		/* variable's native type */
    int		count_;		/* variable's element count */
    pvEventFunc func_;		/* user's event function */
    void	*arg_;		/* user's event function argument */
    void	*private_;	/* message system's private data */
};

////////////////////////////////////////////////////////////////////////////////
/*
 * End of C++.
 */
#endif	/* __cplusplus */

/*
 * C interface
 */
#ifdef __cplusplus
extern "C" {
pvSystem *newPvSystem( const char *name, int debug = 0 );
#endif

pvStat pvSysCreate( const char *name, int debug, void **pSys );
pvStat pvSysDestroy( void *sys );
pvStat pvSysFlush( void *sys );
pvStat pvSysPend( void *sys, double seconds, int wait );
pvStat pvSysLock( void *sys );
pvStat pvSysUnlock( void *sys );
pvStat pvSysAttach( void *sys );
int    pvSysGetMagic( void *sys );
void   pvSysSetDebug( void *sys, int debug );
int    pvSysGetDebug( void *sys );
int    pvSysGetStatus( void *sys );
pvSevr pvSysGetSevr( void *sys );
pvStat pvSysGetStat( void *sys );
char   *pvSysGetMess( void *sys );

pvStat pvVarCreate( void *sys, const char *name, pvConnFunc func, void *priv,
		    int debug, void **pVar );
pvStat pvVarDestroy( void *var );
pvStat pvVarGet( void *var, pvType type, int count, pvValue *value );
pvStat pvVarGetNoBlock( void *var, pvType type, int count, pvValue *value );
pvStat pvVarGetCallback( void *var, pvType type, int count,
		         pvEventFunc func, void *arg );
pvStat pvVarPut( void *var, pvType type, int count, pvValue *value );
pvStat pvVarPutNoBlock( void *var, pvType type, int count, pvValue *value );
pvStat pvVarPutCallback( void *var, pvType type, int count, pvValue *value,
		         pvEventFunc func, void *arg );
pvStat pvVarMonitorOn( void *var, pvType type, int count,
		       pvEventFunc func, void *arg, void **pId );
pvStat pvVarMonitorOff( void *var, void *id );
int    pvVarGetMagic( void *var );
void   pvVarSetDebug( void *var, int debug );
int    pvVarGetDebug( void *var );
int    pvVarGetConnected( void *var );
pvType pvVarGetType( void *var );
int    pvVarGetCount( void *var );
char   *pvVarGetName( void *var );
void   pvVarSetPrivate( void *var, void *priv );
void   *pvVarGetPrivate( void *var );
int    pvVarGetStatus( void *var );
pvSevr pvVarGetSevr( void *var );
pvStat pvVarGetStat( void *var );
char   *pvVarGetMess( void *var );

/*
 * Time utilities
 */
int    pvTimeGetCurrentDouble( double *pTime );

/*
 * Misc utilities
 */
char *Strdup( const char *s );
char *Strdcpy( char *dst, const char *src );

#ifdef __cplusplus
}
#endif

#endif /* INCLpvh */

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.18  2000/03/31 23:00:42  wlupton
 * added default attach and flush implementations; added setStatus
 *
 * Revision 1.17  2000/03/29 01:58:48  wlupton
 * split off pvAlarm.h; added pvVarGetName
 *
 * Revision 1.16  2000/03/18 04:00:25  wlupton
 * converted to use new configure scheme
 *
 * Revision 1.15  2000/03/16 02:10:24  wlupton
 * added newly-needed debug argument
 *
 * Revision 1.14  2000/03/07 09:27:39  wlupton
 * drastically reduced use of references
 *
 * Revision 1.13  2000/03/07 08:46:29  wlupton
 * created ktlKeyword class (works but a bit messy)
 *
 * Revision 1.12  2000/03/06 19:19:43  wlupton
 * misc type conversion and error reporting mods
 *
 * Revision 1.11  2000/03/01 02:07:14  wlupton
 * converted to use new OSI library
 *
 * Revision 1.10  2000/02/19 01:09:51  wlupton
 * added PV_SIMPLE() and prototypes for var-level error info
 *
 * Revision 1.9  2000/02/16 02:31:44  wlupton
 * merged in v1.9.5 changes
 *
 * Revision 1.8  1999/09/07 20:42:59  epics
 * removed unnecessary comment
 *
 * Revision 1.7  1999/07/07 18:50:33  wlupton
 * supported full mapping from EPICS status and severity to pvStat and pvSevr
 *
 * Revision 1.6  1999/07/01 20:50:18  wlupton
 * Working under VxWorks
 *
 * Revision 1.5  1999/06/10 00:35:03  wlupton
 * demo sequencer working with pvCa
 *
 * Revision 1.4  1999/06/08 19:21:43  wlupton
 * CA version working; about to use in sequencer
 *
 * Revision 1.3  1999/06/08 03:25:21  wlupton
 * nearly complete CA implementation
 *
 * Revision 1.2  1999/06/07 21:46:44  wlupton
 * working with simple pvtest program
 *
 * Revision 1.1  1999/06/04 20:48:27  wlupton
 * initial version of pv.h and pv.cc
 *
 */
