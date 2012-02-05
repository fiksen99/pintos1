#ifndef FIXED_POINT_H
#define FIXED_POINT_H

typedef struct {
	public:
	int value;
} fixedPoint; 

fixedPoint* add( fixedPoint&, int );
fixedPoint* add( fixedPoint&, fixedPoint& );
fixedPoint* subtract( fixedPoint&, int );
fixedPoint* subtract( fixedPoint&, fixedPoint& );
fixedPoint* multiply( fixedPoint&, int );
fixedPoint* multiply( fixedPoint&, fixedPoint& );
fixedPoint* divide( fixedPoint&, int );
fixedPoint* divide( fixedPoint&, fixedPoint& );
int convertToInt( fixedPoint& );
fixedPoint* convertToFixedPoint( int );


#endif
