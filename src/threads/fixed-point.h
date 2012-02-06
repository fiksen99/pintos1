#ifndef FIXED_POINT_H
#define FIXED_POINT_H

typedef struct {
	int value;
} fixedPoint; 

#define FRACTION_SIZE 2^14 //using 17.14 number format. i.e. f = 2^14

fixedPoint* add( fixedPoint*, int );
fixedPoint* add( fixedPoint*, fixedPoint* );
fixedPoint* subtract( fixedPoint*, int );
fixedPoint* subtract( fixedPoint*, fixedPoint* );
fixedPoint* multiply( fixedPoint*, int );
fixedPoint* multiply( fixedPoint*, fixedPoint* );
fixedPoint* divide( fixedPoint*, int );
fixedPoint* divide( fixedPoint*, fixedPoint* );
int convertToInt( fixedPoint* );
fixedPoint* convertToFixedPoint( int );

#endif
