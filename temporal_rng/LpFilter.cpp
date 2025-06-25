#include "LpFilter.hpp"


void LpFilter::Init(double initVal)
{
    filteredVal_ = initVal;
}


double LpFilter::Feed(double newVal, double length)
{
	//                 1.                Length - 1.  
	// FilteredVal = ------ * NewVal  +  ----------- * FilteredVal
	//               Length                Length

	filteredVal_ += (newVal-filteredVal_)/length;

    return filteredVal_;
}


double LpFilter::GetValue()
{
    return filteredVal_;
}
