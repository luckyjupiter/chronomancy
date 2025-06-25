// © 2012 Psigenics
//
// PrdObject: Contains information for a PRD source instance - C-Style object
//
// 2012.10.01    pawilber        Creation: Refactored PrdObject out of PrdCore


#pragma once


#include "Buffer.hpp"
#include "PcqngCore.hpp"
#include "IRng.hpp"
#include "IPrdGen.hpp"
#include "ITargetGen.hpp"
#include "PrdComparator.hpp"


struct PrdObject
{
    Buffer*        pcqngCoreBuffer;
    Buffer*        rngBiasBuffer;
	Buffer*        rngAcBuffer;
    PcqngCore*     pcqngCore;
    IRng*          rng;
    IPrdGen*       prdGen;
	IPrdGen*       prdAcGen;
    ITargetGen*    targetGen;
    PrdComparator* prdComparator;
};


void InitPrdObject(PrdObject& prdObject, char* source);
void DeinitPrdObject(PrdObject& prdObject);