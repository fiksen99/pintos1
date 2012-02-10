#ifndef FIXED_POINT_H
#define FIXED_POINT_H

typedef struct 
{
	int value;
} fixed_point; 

#define FRACTION_SIZE (1<<14) //using 17.14 number format. i.e. f = 2^14

void add_int( fixed_point*, int );
void add_fixed_point( fixed_point*, fixed_point* );
void subtract_int( fixed_point*, int );
void subtract_fixed_point( fixed_point*, fixed_point* );
void multiply_int( fixed_point*, int );
void multiply_fixed_point( fixed_point*, fixed_point* );
void divide_int( fixed_point*, int );
void divide_fixed_point( fixed_point*, fixed_point* );
int convert_to_int( fixed_point* );
int convert_to_fixed_point( int );

#endif
