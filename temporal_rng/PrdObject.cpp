// © 2012 Psigenics
//
// PrdObject: Contains information for a PRD source instance - C-Style object
//
// 2012.10.01    pawilber        Creation: Refactored PrdObject out of PrdCore


#include "PrdObject.h"
#include "FtdiRngArray.hpp"
#include "PcqngRng.hpp"
#include "BpZAnalogFlipMultiPrd.hpp"
#include "PcqngTargetGen.hpp"


void InitPrdObject(PrdObject& prdObject, char* source)
{
    // create and start PCQNG core
    prdObject.pcqngCoreBuffer = new Buffer(1000, false);
    prdObject.pcqngCore = new PcqngCore(*prdObject.pcqngCoreBuffer);

    // create Bias and AC buffers
    prdObject.rngBiasBuffer = new Buffer(12000, false);
    prdObject.rngAcBuffer = new Buffer(12000, false);

    // no source, then auto-select source and default to bias only generation
    if (source == NULL)
    {
        std::vector<std::string> sourceList;
        FtdiRngArray::ListSources(sourceList);
        if (sourceList.size() > 0)
	        prdObject.rng = new FtdiRngArray(*prdObject.rngBiasBuffer, *prdObject.rngAcBuffer, sourceList[0]);
        else
            prdObject.rng = new PcqngRng(*prdObject.pcqngCoreBuffer, *prdObject.rngBiasBuffer, *prdObject.rngAcBuffer);

        prdObject.prdAcGen = NULL;
    }

    // prd setup
    prdObject.prdGen = new BpZAnalogFlipMultiPrd(*prdObject.rng, *prdObject.rngBiasBuffer);
    prdObject.targetGen = new PcqngTargetGen(*prdObject.pcqngCoreBuffer, 1, *prdObject.pcqngCore);
    prdObject.pcqngCore->Start();
    prdObject.rng->Start();
    prdObject.prdComparator = new PrdComparator(*prdObject.rng, *prdObject.prdGen, *prdObject.targetGen, PrdComparator::PSIMODE_PSYCHOKINESIS);
}


void DeinitPrdObject(PrdObject& prdObject) {
    // shut down
    prdObject.rng->Stop();
    prdObject.pcqngCore->Stop();

    // clean up
    delete prdObject.prdComparator;
    delete prdObject.targetGen;
    delete prdObject.prdGen;
    delete prdObject.prdAcGen;
    delete prdObject.rng;
	if (prdObject.rngAcBuffer != NULL)
    {
		delete prdObject.rngAcBuffer;
		prdObject.rngAcBuffer = NULL;
	}
    delete prdObject.rngBiasBuffer;
    delete prdObject.pcqngCore;
    delete prdObject.pcqngCoreBuffer;
}