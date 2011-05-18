program p

int x;
assign x;
syncq x 1; /* error: not monitored */

#include "simple.st"
