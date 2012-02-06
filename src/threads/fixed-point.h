#ifndef FIXED_POINT_H
#define FIXED_POINT_H

typedef struct 
{
	int value;
} fixedPoint; 

#define FRACTION_SIZE 2^14 //using 17.14 number format. i.e. f = 2^14

void add_int( fixedPoint*, int );
void add_fixed_point( fixedPoint*, fixedPoint* );
void subtract_int( fixedPoint*, int );
void subtract_fixed_point( fixedPoint*, fixedPoint* );
void multiply_int( fixedPoint*, int );
void multiply_fixed_point( fixedPoint*, fixedPoint* );
void divide_int( fixedPoint*, int );
void divide_fixed_point( fixedPoint*, fixedPoint* );
int convertToInt( fixedPoint* );
void convertToFixedPoint( int, fixedPoint* );

#endif





