#include "threads/fixed-point.h"
#include <stdint.h>

//result is stored in first fixedPoint parameter

void 
add_int (fixedPoint* x, int n)
{
	x->value += n * FRACTION_SIZE;
}

void
add_fixed_point (fixedPoint* x, fixedPoint* y) 
{
	x->value += y->value;
}

void
subtract_int (fixedPoint* x, int n) 
{
	x->value -= n * FRACTION_SIZE;
}

void
subtract_fixed_point (fixedPoint* x, fixedPoint* y) 
{
	x->value -= y->value;
}

void 
multiply_int (fixedPoint* x, int n) 
{
	x->value *= n;
}

void
multiply_fixed_point (fixedPoint* x, fixedPoint* y) 
{
	x->value = ( ( int64_t ) x->value ) * y->value / FRACTION_SIZE;
}

void
divide_int (fixedPoint* x, int n) 
{
	x->value /= n;
}

void
divide_fixed_point (fixedPoint* x, fixedPoint* y) 
{
	x->value = ( ( int64_t ) x->value ) * FRACTION_SIZE / y->value;
}

int
convert_to_int (fixedPoint* x) 
{
#ifdef ROUND_TO_NEAREST
	int result = x->value;
	if( result >= 0 ) 
	{
		return result + FRACTION_SIZE / 2;
	}
	else 
	{
		return result - FRACTION_SIZE / 2;
	}
#else
	return x->value / FRACTION_SIZE;
#endif
}


int convert_to_fixedPoint( int n )
{
	return n * FRACTION_SIZE;
}
