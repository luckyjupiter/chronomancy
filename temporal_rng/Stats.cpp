#include "Stats.hpp"
#include <math.h>


// Input z-score, returns cumulative normal distribution value
// Accuracy better than 1% to z=+/-7.5; .05% to z=+/-4.
double Stats::ZToP(double zScore)
{
	double retval;

    // calculation variables
	double w;
	double y;
	double t;
	double num;
	double denom;

	// check
	if ( zScore > 8. )
	{
		zScore = 8.;
	}
	else 
	{
		if ( zScore < -8. )
			zScore = -8.;
	}


	// calculation constants
	double c[8];
	c[1] = 2.506628275;    c[2] = 0.31938153;     c[3] = -0.356563782;    c[4] = 1.781477937;
	c[5] = -1.821255978;   c[6] = 1.330274429;    c[7] = 0.2316419;

	w = (zScore>=0)? 1 : -1;
	t = 1. + (c[7]*w*zScore);
	y = 1./t;

	num = c[2] + (c[6] + (c[5]*t) + (c[4]*t*t) + (c[3]*t*t*t)) / (t*t*t*t) ;
	denom = c[1] * exp( .5*zScore*zScore ) * t;

	retval = 0.5 + w * ( .5 - (num/denom) );

	return retval;
}


double Stats::PToZ(double pValue)
{
    // fit constants
    double p[] = {-0.322232431088, -1.0, -0.342242088547, -0.0204231210245, -0.0000453642210148};
    double q[] = {0.099348462606, 0.588581570495, 0.531103462366, 0.10353775285, 0.0038560700634};

    double pp;
    if (pValue  < 0.5)
        pp = pValue;
    else
        pp = 1.0 - pValue;

    if (pp <= 0.0) pp = 0.000001;
    if (pp >= 1.0) pp = 0.999999;

    double y = sqrt(log(1.0/(pp*pp)));
    double retVal = y + ((((y*p[4] + p[3])*y + p[2])*y + p[1])*y + p[0]) / ((((y*q[4] + q[3])*y + q[2])*y + q[1])*y + q[0]);
    if (pValue < 0.5)
        retVal = -retVal;

    return retVal;
}
