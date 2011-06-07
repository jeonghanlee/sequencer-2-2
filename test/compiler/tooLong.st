/*
 * Test proper rejection of variables whose type is too long
 * to allow faithfull representation in CA, i.e. 64 bit types.
 * On 64 bit target systems this should give error
 * messages for the l and ul variables.
 */
program tooLong

int i;
assign i to "";

unsigned int ui;
assign ui to "";

long l;
assign l to "";

unsigned long ul;
assign ul to "";

#include "simple.st"
