#ifndef INCLUDE_HPP_BPANNSTREAMFILTERS_
#define INCLUDE_HPP_BPANNSTREAMFILTERS_


#include <vector>


class BpAnnStreamFilters
{
public:
    static void ConvertZsToPs(std::vector<double>& streams);
    static void SNormalize(std::vector<double>& streams);
    static void Factorize(std::vector<double>& streams);
    static void SortAscending(std::vector<double>& streams);

private:
    static int __cdecl SortComparator(const void* p1, const void* p2);
};


#endif // INCLUDE_HPP_BPANNSTREAMFILTERS_
