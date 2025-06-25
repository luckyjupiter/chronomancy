#include "BpZAnalogFlipMultiPrd.hpp"
#include "IRng.hpp"
#include "Buffer.hpp"
#include "BpAnnStreamFilters.hpp"
#include "Stats.hpp"
#include "MersenneTwister.hpp"
#include "LpSpeedFilter.hpp"
#include <stdio.h>
#include <direct.h>

#include <functional>
#include <algorithm>

double BPZAFP_CFACTOR = 0;
double BPZAFP_SD = 0;

BpZAnalogFlipMultiPrd::BpZAnalogFlipMultiPrd(IRng& rng, Buffer& inBuffer)
    : rng_(rng) 
    , inBuffer_(inBuffer)
    , inBlockLength_(0)
    , combinedCount_(0)
    , filterCoef_(0)
    , filterAmplitude_(0)
    , prdId_("BpZAnalogFlip")
    , prdMode_(1)
{
    lpFilters_.resize(7);
    ReadConfigFile();

    // calculate combinedCount_
//    if (rng_.GetId().find("PRNG") == 0)
//    {
//        combinedCount_ = (int)(((double)rng_.GetBitRate()/(7*1000))/(double)inBlockLength_);
//        inBuffer_.SetSize(1.*combinedCount_*inBlockLength_*238);
//    }
//    else
//    {
        combinedCount_ = (int)(((double)rng_.GetBitRate()/(7*1000))/(double)inBlockLength_);
        inBuffer_.SetSize(combinedCount_*inBlockLength_*238);
//    }

    BPZAFP_CFACTOR = 0.4111851;//0.4111851;//0.4074019;//0.4093;//0.4111851;
    BPZAFP_SD = 1.697586;
}

extern HMODULE hModule__;
void BpZAnalogFlipMultiPrd::ReadConfigFile()
{
    // open config file
    char configXml[10001];
    LoadStringA(hModule__, 0, configXml, 10000);
    XMLNode xMainNode = XMLNode::parseString(configXml, "PrdConfig");

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

    LpSpeedFilter::Init(filterCoef_);
}


#pragma warning(disable:4996)
CBackProp* BpZAnalogFlipMultiPrd::RestoreBpAnn(char const* annFileName)
{
    CBackProp* ann;

    char bpConfig[10001];
    LoadStringA(hModule__, atoi(annFileName), bpConfig, 10000);

    char* bpLine = strtok(bpConfig, ",");

    // create ann shell
    int layerCount = 0;
    sscanf_s(bpLine, "%i", &layerCount);
    int* structure = new int[layerCount];
    for (int l=0; l<layerCount; ++l)
    {
        bpLine = strtok(0, ",\n");
        sscanf_s(bpLine, "%i", &structure[l]);
    }
    ann = new CBackProp(layerCount, structure, 0, 0);
    delete[] structure;

    // restore ann
    ann->restore(annFileName);
    ann->alpha = 0;
    ann->beta = 0;


    // find median
    char* line;
    line = strtok(0, "T");
    line = strtok(0, "\n");
    annMedian_[ann] = atof(&line[1]);

    return ann;
}
#pragma warning(default:4996)


double BpZAnalogFlipMultiPrd::Generate()
{
    double output = 0;
    int vote = 0;
    double allZCombined = 0;

    allZCombined = 0;
    double allCombinedCount = 0;
    double outZSum = 0;
    for (int cc=0; cc<combinedCount_; ++cc)
    {
        allZCombined = 0;
        allCombinedCount = 0;
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

        double uniIn = Stats::ZToP(allZCombined / (BPZAFP_SD*sqrt(allCombinedCount)));
        uniIn -= 0.5;

        output = 0.4999120227188166;
        output += 0.9401644273756995    * uniIn;
        output -= 0.06189892312579787   * pow(uniIn, 2);
        output += 0.5840547224293358  * pow(uniIn, 3);
        output -= 0.06279528329036001    * pow(uniIn, 4);
        output -= 41.30002136193432      * pow(uniIn, 5);
        output += 63.6934730022999     * pow(uniIn, 6);
        output += 4813.84460421426     * pow(uniIn, 7);
        output -= 7493.590850454374    * pow(uniIn, 8);
        output -= 340001.8565448058    * pow(uniIn, 9);
        output += 503336.4822699632   * pow(uniIn, 10);
        output += 1.566840088741423e7  * pow(uniIn, 11);
        output -= 2.1333176641644526e7   * pow(uniIn, 12);
        output -= 4.92937331874354e8   * pow(uniIn, 13);
        output += 6.076979605963644e8  * pow(uniIn, 14);
        output += 1.08581256560555e10 * pow(uniIn, 15);
        output -= 1.2002848790144238e10  * pow(uniIn, 16);
        output -= 1.688309368304776e11 * pow(uniIn, 17);
        output += 1.6559307639051575e11  * pow(uniIn, 18);
        output += 1.829205590182583e12  * pow(uniIn, 19);
        output -= 1.5648509913332349e12  * pow(uniIn, 20);
        output -= 1.3118835707742068e13  * pow(uniIn, 21);
        output += 9.4268947279356e12  * pow(uniIn, 22);
        output += 5.265125782576635e13  * pow(uniIn, 23);
        output -= 2.762011093410443e13 * pow(uniIn, 24);
        output -= 2.278189148932907e13 * pow(uniIn, 25);
        output -= 3.832176004541615e13  * pow(uniIn, 26);
        output -= 7.624043433390468e14  * pow(uniIn, 27);
        output += 5.1150012554234856e14  * pow(uniIn, 28);
        output += 1.6875893806964042e15  * pow(uniIn, 29);
        output -= 1.5152930594768678e14 * pow(uniIn, 30);
        output += 1.0794808700750578e16  * pow(uniIn, 31);
        output -= 7.743715011679011e15    * pow(uniIn, 32);
        output -= 2.8658317446488632e16 * pow(uniIn, 33);
        output += 2.4720899212964755e15 * pow(uniIn, 34);
        output -= 1.885159113232565e17  * pow(uniIn, 35);
        output += 1.2127851072533165e17  * pow(uniIn, 36);
        output += 3.0665388015258285e17 * pow(uniIn, 37);
        output += 6.522957787694365e16 * pow(uniIn, 38);
        output += 3.5025399272516137e18  * pow(uniIn, 39);
        output -= 1.84261076723662e18  * pow(uniIn, 40);
        output -= 1.4059624018684043e17 * pow(uniIn, 41);
        output -= 3.4388816728212977e18 * pow(uniIn, 42);
        output -= 5.978528790856636e19 * pow(uniIn, 43);
        output += 2.4978548332136088e19  * pow(uniIn, 44);
        output -= 9.214868556594325e19  * pow(uniIn, 45);
        output += 9.036733817269415e19 * pow(uniIn, 46);
        output += 9.130350829406218e20  * pow(uniIn, 47);
        output -= 3.065388981011795e20  * pow(uniIn, 48);
        output += 2.7712632892701886e21 * pow(uniIn, 49);
        output -= 1.8210797460227285e21 * pow(uniIn, 50);
        output -= 1.485724169818622e22 * pow(uniIn, 51);
        output += 4.747849059354342e21 * pow(uniIn, 52);
        output -= 5.146447262525216e22 * pow(uniIn, 53);
        output += 3.0669682466675424e22  * pow(uniIn, 54);
        output += 3.950249518060934e23  * pow(uniIn, 55);
        output -= 1.6435451711024604e23 * pow(uniIn, 56);
        output -= 8.239414755881368e23  * pow(uniIn, 57);
        output += 2.9590156586788576e23  * pow(uniIn, 58);
        output += 6.017834807224846e23 * pow(uniIn, 59);
        output -= 1.942327199752698e23 * pow(uniIn, 60);

        outZSum += Stats::PToZ(output);
    }

    output = outZSum /sqrt((double)combinedCount_);

    return output;
}


void BpZAnalogFlipMultiPrd::ClearBuffers()
{
    rng_.ClearBuffer();
}


std::string BpZAnalogFlipMultiPrd::GetId()
{
    return prdId_;
}


void BpZAnalogFlipMultiPrd::SetPrdMode(int prdMode)
{
    switch (prdMode)
    {
    case 1:    // calculate combinedCount_
        if (rng_.GetId().find("PCQNG") == 0)
        {
            combinedCount_ = (int)(((double)rng_.GetBitRate()/(7*1000))/(double)inBlockLength_);
            inBuffer_.SetSize(combinedCount_*inBlockLength_*238);
        }
        else
        {
            combinedCount_ = (int)(((double)rng_.GetBitRate()/(7*1000))/(double)inBlockLength_);
            inBuffer_.SetSize(combinedCount_*inBlockLength_*238);
        }
        prdMode_ = 1;
        break;
    case 2:
        if (rng_.GetId().find("PCQNG") == 0)
            combinedCount_ = (int)(((double)rng_.GetBitRate()/(7*1000))/(double)inBlockLength_);
        else
            combinedCount_ = (int)(((double)rng_.GetBitRate()/(7*1000))/(double)inBlockLength_);
        inBuffer_.SetSize(combinedCount_*inBlockLength_*238);
        prdMode_ = 2;
        break;
    case 3:
        combinedCount_ = 1;
        inBuffer_.SetSize(combinedCount_*inBlockLength_*238);
        prdMode_ = 3;
        break;
    }
}

int BpZAnalogFlipMultiPrd::GetPrdMode()
{
    return prdMode_;
}

unsigned long words__[7];
void BpZAnalogFlipMultiPrd::GenerateSevenTuple(std::vector<double>& sevenTuple)
{
    std::for_each(lpFilters_.begin(), lpFilters_.end(), std::bind2nd(std::mem_fun1_ref(&LpFilter::Init), 0));

    std::vector<uint8_t> randWords;
    inBuffer_.Read(randWords, inBlockLength_);
    std::for_each(randWords.begin(), randWords.end(),
        std::bind1st(std::mem_fun1(&BpZAnalogFlipMultiPrd::LfsrFeed), this));

    for (int i=0; i<7; ++i)
        sevenTuple.push_back(LpSpeedFilter::LookUp[words__[i]&0x1FFFF]/filterAmplitude_);
}


void BpZAnalogFlipMultiPrd::LfsrFeed(uint8_t rngWord)
{

    for (int n=0; n<7; ++n)
    {
        words__[n] <<= 1;
        words__[n] |= (rngWord>>(6-n))&0x1;
    }
}


void BpZAnalogFlipMultiPrd::FeedAnns(std::vector<CBackProp*>& anns, std::vector<double>& sevenTuple, std::vector<double>& annResults)
{
    for (size_t i=0; i<anns.size(); ++i)
    {
        anns[i]->ffwd(&sevenTuple[0]);
        double fitt = CurveFit(anns[i]);
        annResults.push_back(fitt);
    }
}


double BpZAnalogFlipMultiPrd::ZCombineAnns(std::vector<double>& annResults)
{
    double combinedZ = 0;

    for (size_t i=0; i<annResults.size(); ++i)
        combinedZ += Stats::PToZ(annResults[i]);

    return combinedZ;
}


double BpZAnalogFlipMultiPrd::CurveFit(CBackProp* ann)
{
    double fitResult = 0;
    double fitArg    = Stats::ZToP((ann->Out(0)-annMedian_[ann]) / annFitSd_[ann]) - 0.5;

    for (size_t i=0; i<annFitCoefs_[ann].size(); ++i)
        fitResult += annFitCoefs_[ann][i] * pow(fitArg, (int)i);

    return fitResult;
}
