#include "BpZAnalogFlipPrd.hpp"
#include "IRng.hpp"
#include "Buffer.hpp"
#include "BpAnnStreamFilters.hpp"
#include "Stats.hpp"

#include <functional>
#include <algorithm>


#define BPZAFP_CFACTOR 0.4283


BpZAnalogFlipPrd::BpZAnalogFlipPrd(IRng& rng)
    : rng_(rng) 
    , inBuffer_(rng.GetOutBuffer())
    , inBlockLength_(0)
    , combinedCount_(0)
    , filterCoef_(0)
    , filterAmplitude_(0)
    , prdId_("BpZAnalogFlip")
{
    lpFilters_.resize(7);
    ReadConfigFile();

    // calculate combinedCount_
    combinedCount_ = (int)(0.200*((double)rng_.GetBitRate()/7)/(double)inBlockLength_ + 0.5);
    if ((combinedCount_%2) == 0)
        --combinedCount_;
    inBuffer_.SetSize(combinedCount_*inBlockLength_);
}


void BpZAnalogFlipPrd::ReadConfigFile()
{
    // open config file
    XMLNode xMainNode = XMLNode::openFileHelper("PsigPrdConfig.xml", "PrdConfig");

    // find the desired processing section
    XMLNode xNode = xMainNode.getChildNode("BpAnnMajFilt17");
    // find blocklength
    inBlockLength_ = atoi(xNode.getAttribute("blocklength"));
    // get filter coefficient
    filterCoef_ = atof(xNode.getAttribute("filtercoef"));
    // get filter amplitude
    filterAmplitude_ = atof(xNode.getAttribute("filteramp"));

    // get number of ann's in this section
    int annCount = xNode.nChildNode("BpAnn");
    int nodeIterator = 0;
    for (int i=0; i<annCount; ++i)
    {
        XMLNode annXNode = xNode.getChildNode("BpAnn", &nodeIterator);

        char const* annFileName = annXNode.getAttribute("file");
        CBackProp* ann = RestoreBpAnn(annFileName);
        if (strcmp(annXNode.getAttribute("type"), "TRawNorm") == 0)
            tRawNormAnns_.push_back(ann);
        if (strcmp(annXNode.getAttribute("type"), "TRawUni") == 0)
            tRawUniAnns_.push_back(ann);
        if (strcmp(annXNode.getAttribute("type"), "TFactNorm") == 0)
            tFactNormAnns_.push_back(ann);
        if (strcmp(annXNode.getAttribute("type"), "TFactUni") == 0)
            tFactUniAnns_.push_back(ann);
        if (strcmp(annXNode.getAttribute("type"), "HFactUni") == 0)
            hFactUniAnns_.push_back(ann);

        // get ann fit SD
        annFitSd_[ann] = atof((char*)annXNode.getAttribute("fitsd"));

        // get ann fit coefs
        std::vector<double> coefs;
        char* coefString = (char*)annXNode.getAttribute("fitcoefs");
        char* nextToken;
        char* coefToken = strtok_s(coefString, ",", &nextToken);
        while (coefToken != 0)
        {
            coefs.push_back(atof(coefToken));
            coefToken = strtok_s(0, ",", &nextToken);
        }
        annFitCoefs_[ann]= coefs;
    }
}


CBackProp* BpZAnalogFlipPrd::RestoreBpAnn(char const* annFileName)
{
    CBackProp* ann;

    // open ann file
    FILE* annFile = 0;
    fopen_s(&annFile, annFileName, "r");

    // create ann shell
    int layerCount = 0;
    fscanf_s(annFile, "%i", &layerCount);
    int* structure = new int[layerCount];
    for (int l=0; l<layerCount; ++l)
        fscanf_s(annFile, ", %i", &structure[l]);
    ann = new CBackProp(layerCount, structure, 0, 0);
    delete[] structure;

    // restore ann
    ann->restore(annFileName);
    ann->alpha = 0;
    ann->beta = 0;

    // find median
    while (!feof(annFile))
    {
        int filePos = ftell(annFile);
        char line[41];
        fgets(line, 40, annFile);
        if (strchr(line, 'T') == line)
        {
            annMedian_[ann] = atof(&line[1]);
            break;
        }
        filePos = 0;
    }

    return ann;
}


double BpZAnalogFlipPrd::Generate()
{
    double allZCombined = 0;
    double allCombinedCount = 0;
    for (int cc=0; cc<combinedCount_; ++cc)
    {
        // raw normal streams (create and use)
        std::vector<double> sevenTuple;
        GenerateSevenTuple(sevenTuple);
        std::vector<double> tAnnResults;
        FeedAnns(tRawNormAnns_, sevenTuple, tAnnResults);

        // raw uniform streams (create and use)
        BpAnnStreamFilters::ConvertZsToPs(sevenTuple);
        FeedAnns(tRawUniAnns_, sevenTuple, tAnnResults);

        // factorized normal streams (create and use)
        BpAnnStreamFilters::SortAscending(sevenTuple);
        BpAnnStreamFilters::SNormalize(sevenTuple);
        BpAnnStreamFilters::Factorize(sevenTuple);
        FeedAnns(tFactNormAnns_, sevenTuple,  tAnnResults);

        // factorized uniform streams (create and use)
        BpAnnStreamFilters::ConvertZsToPs(sevenTuple);
        FeedAnns(tFactUniAnns_, sevenTuple,  tAnnResults);
        std::vector<double> hAnnResults;
        FeedAnns(hFactUniAnns_, sevenTuple,  hAnnResults);

        double tZCombined = ZCombineAnns(tAnnResults);
        double hZCombined = ZCombineAnns(hAnnResults);

        tZCombined += hZCombined * BPZAFP_CFACTOR;
        if (hZCombined < 0)
            tZCombined = -tZCombined;

        allZCombined += tZCombined;
        allCombinedCount += tAnnResults.size() + (double)hAnnResults.size()*pow(BPZAFP_CFACTOR, 2);

        static double sumsqr = 0;
        sumsqr += tZCombined*tZCombined;
        static double sqn = 0;
        static int sqc = 0;
        static double fff = 0;
        ++fff;
        sqn += tAnnResults.size() + (double)hAnnResults.size()*pow(BPZAFP_CFACTOR, 2);
        ++sqc;
        if (sqc == 100000)
        {
            sqc = 0;
//            printf("%f  %f\n", fff, sqrt(sumsqr/sqn));
        }
    }

    allZCombined /= sqrt(allCombinedCount);

    return allZCombined;
}


void BpZAnalogFlipPrd::ClearBuffers()
{
    rng_.ClearBuffer();
}


std::string BpZAnalogFlipPrd::GetId()
{
    return prdId_;
}


void BpZAnalogFlipPrd::GenerateSevenTuple(std::vector<double>& sevenTuple)
{
    std::for_each(lpFilters_.begin(), lpFilters_.end(), std::bind2nd(std::mem_fun1_ref(&LpFilter::Init), 0));

    std::vector<uint8_t> randWords;
    inBuffer_.Read(randWords, inBlockLength_);
    std::for_each(randWords.begin(), randWords.end(),
        std::bind1st(std::mem_fun1(&BpZAnalogFlipPrd::LfsrFeed), this));

    for (int i=0; i<7; ++i)
        sevenTuple.push_back(lpFilters_[i].GetValue()/filterAmplitude_);
}


void BpZAnalogFlipPrd::LfsrFeed(uint8_t rngWord)
{
    for (int n=0; n<7; ++n)
        lpFilters_[n].Feed(2*((rngWord>>(6-n))&0x1) - 1, filterCoef_);
}


void BpZAnalogFlipPrd::FeedAnns(std::vector<CBackProp*>& anns, std::vector<double>& sevenTuple, std::vector<double>& annResults)
{
    for (size_t i=0; i<anns.size(); ++i)
    {
        anns[i]->ffwd(&sevenTuple[0]);
        annResults.push_back(CurveFit(anns[i]));
    }
}


double BpZAnalogFlipPrd::ZCombineAnns(std::vector<double>& annResults)
{
    double combinedZ = 0;

    for (size_t i=0; i<annResults.size(); ++i)
        combinedZ += Stats::PToZ(annResults[i]);

    return combinedZ;
}


double BpZAnalogFlipPrd::CurveFit(CBackProp* ann)
{
    double fitResult = 0;
    double fitArg    = Stats::ZToP((ann->Out(0)-annMedian_[ann]) / annFitSd_[ann]) - 0.5;

    for (size_t i=0; i<annFitCoefs_[ann].size(); ++i)
        fitResult += annFitCoefs_[ann][i] * pow(fitArg, (int)i);

    return fitResult;
}
