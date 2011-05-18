program p

int x;
assign x;
monitor x;
syncq x 0; /* error: queue size out of range */

#include "simple.st"
