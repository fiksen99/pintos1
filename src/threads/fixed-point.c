#include "fixed-point.h"

fixedPoint* add( fixedPoint* x, int n ) {
	new fixedPoint result = { .value = x->value + n * FRACTION_SIZE };
	return &result;
}

fixedPoint* add( fixedPoint* x, fixedPoint* y ) {
	new fixedPoint result = { .value = x->value + y->value };
	return &result;
}

fixedPoint* subtract( fixedPoint* x, int n ) {
	new fixedPoint result = { .value = x->value - n * FRACTION_SIZE };
	return &result;
}

fixedPoint* subtract( fixedPoint* x, fixedPoint* y ) {
	new fixedPoint result = { .value = x->value - y->value };
	return &result;
}

fixedPoint* multiply( fixedPoint* x, int n ) {
	new fixedPoint result = { .value = x->value * n };
	return &result;
}

fixedPoint* multiply( fixedPoint* x, fixedPoint* y ) {
	new fixedPoint result = { .value = ( ( int64_t ) x->value ) * y->value / FRACTION_SIZE };
	return &result;
}

fixedPoint* divide( fixedPoint* x, int n ) {
	new fixedPoint result = { .value = x->value / n };
	return &result;
}

fixedPoint* divide( fixedPoint* x, fixedPoint* y ) {
	new fixedPoint result = { .value = ( ( int64_t ) x->value ) * FRACTION_SIZE / y->value };
	return &result;
}

int convertToInt( fixedPoint* x ) {
#ifdef ROUND_TO_NEAREST
	int result = x->value;
	if( result >= 0 ) {
		return result + FRACTION_SIZE / 2;
	} else {
		return result - FRACTION_SIZE / 2;
	}
#else
	return x->value / FRACTION_SIZE;
#endif
}

fixedPoint* convertToFixedPoint( int n ) {
	new fixedPoint result = { .value = n * FRACTION_SIZE };
	return &result;
}
