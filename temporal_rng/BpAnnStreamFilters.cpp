#include "BpAnnStreamFilters.hpp"
#include "Stats.hpp"

#include <algorithm>
#include <functional>
#include <memory.h>
#include <math.h>
#include <stdlib.h>


void BpAnnStreamFilters::ConvertZsToPs(std::vector<double>& streams)
{
    for (size_t i=0; i<streams.size(); ++i)
        streams[i] = Stats::ZToP(streams[i]);
}


void BpAnnStreamFilters::SNormalize(std::vector<double>& streams)
{
    double smean[] = {.125, .25, .375, .5, .625, .75, .875};
    double sdev[] = {0.1102397, 0.1443377, 0.1613745, 0.1666667, 0.1613745, 0.1443377, 0.1102397};

    for (size_t i=0; i<streams.size(); ++i)
        streams[i] = (streams[i]-smean[i])/sdev[i];
}


void BpAnnStreamFilters::Factorize(std::vector<double>& streams)
{
    double eigv[] = {3.99988, 1.33345, 0.666633, 0.399989, 0.266686, 0.190498, 0.142862};
    double initEigTab[] =
        {0.28867, 0.377961, 0.422581, 0.436442, 0.422581, 0.377961, 0.28867,
        -0.499982, -0.436442, -0.244001, 0.000011, 0.244001, 0.436442, 0.499982,
        0.56412, 0.123018, -0.275204, -0.426376, -0.275204, 0.123018, 0.56412,
        -0.476755, 0.312085, 0.418697, 0.000041, -0.418697, -0.312085, 0.476755,
        0.310013, -0.541326, 0.030244, 0.468922, 0.030244, -0.541326, 0.310013, 
        -0.150738, 0.460567, -0.514933, 0.000017, 0.514933, -0.460567, 0.150738, 
        0.04828, -0.22131, 0.494746, -0.63862, 0.494746, -0.22131, 0.04828};
    double eigtab[7][7];
    memcpy(eigtab, initEigTab, 7*7*sizeof(double));

    double* streamsCopy = new double[streams.size()];
    memcpy(streamsCopy, &streams[0], streams.size()*sizeof(double));
    for (size_t i=0; i<streams.size(); ++i)
    {
        streams[i] = 0;
        for (int j=0; j<7; ++j)
            streams[i] += streamsCopy[j] * eigtab[i][j];
        streams[i] /= sqrt(eigv[i]);
    }
    delete[] streamsCopy;
}


void BpAnnStreamFilters::SortAscending(std::vector<double>& streams)
{
    qsort((void*)&streams[0], (size_t)streams.size(), sizeof(double), BpAnnStreamFilters::SortComparator);
}


int BpAnnStreamFilters::SortComparator(const void* p1, const void* p2)
{
    double prob1 = *(double*)p1;
    double prob2 = *(double*)p2;
    if (prob1 == prob2)
        return 0;
    if (prob1 < prob2)
        return -1;
    else
        return 1;
}
