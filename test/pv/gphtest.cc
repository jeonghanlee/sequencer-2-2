// gpHash test program

#include <stdio.h>

#include "gpHash.h"

static struct {
    const char *name;
    int  id;
} details[] = {
    {"a", 0},
    {"b", 1},
    {"c", 2},
    {"c", 3},
    {"d", 4}
};

#define NDETAILS ( sizeof( details ) / sizeof( details[0] ) )

int main() {
    gphPvt *handle;

    gphInitPvt( &handle, 256 );
    for ( size_t i = 0; i < NDETAILS; i++ )
	printf( "gphAdd( %s, %d ) = %p\n", details[i].name, details[i].id,
		gphAdd( handle, details[i].name, ( void * ) details[i].id ) );

    gphDump( handle );

    printf( "gphFind( a, 0 ) = %p\n", gphFind( handle, "a", ( void * ) 0 ) );
    printf( "gphFind( a, 1 ) = %p\n", gphFind( handle, "a", ( void * ) 1 ) );
    printf( "gphFind( c, 2 ) = %p\n", gphFind( handle, "c", ( void * ) 2 ) );
    printf( "gphFind( c, 3 ) = %p\n", gphFind( handle, "c", ( void * ) 3 ) );

    gphDelete( handle, "a", ( void * ) 0 );

    gphDump( handle );

    gphFreeMem( handle );

    gphDump( handle );

    return 0;
}
