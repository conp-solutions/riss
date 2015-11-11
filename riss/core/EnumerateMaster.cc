/********************************************************************************[EnumerateMaster.h]
Copyright (c) 2015, All rights reserved, Norbert Manthey

 **************************************************************************************************/

#include "riss/core/EnumerateMaster.h"

// to be able to use the preprocessor
#include "coprocessor/Coprocessor.h"

using namespace std;

EnumerateMaster::EnumerateMaster(int _nVars)
    :
    enumerateParallel(false)
    , enumerateWithBacktracking(false)
    , coprocessor(nullptr)
    , nVars(_nVars)
    , useProjection(false)
    , maximalModels(1)
    , mType(2)
    , nextClientID(0)
    , successfulBloom(0)
    , shareBlockingClauses(true)
    , minimizeReceivedBlockingClauses(2)
    , checkForModelsEveryX(512)
{
}

EnumerateMaster::~EnumerateMaster()
{
    for (int i = 0 ; i < models.size(); ++ i) { delete models[i]; }
    for (int i = 0 ; i < blockingClauses.size(); ++ i) { delete blockingClauses[i].clause; }
    for (int i = 0 ; i < fullModels.size(); ++ i) { delete fullModels[i]; }

}


void EnumerateMaster::initEnumerateModels()
{
    if (projectionFileName != "") {
        // read in projection variables
        projectionVariables.clear();
        VarFileParser parse((string)projectionFileName);     // open file for parsing
        int max = parse.extract(projectionVariables);
        max = max >= nVars ? max : nVars;
        for (int i = 0 ; i < projectionVariables.size(); ++i) {
            assert(projectionVariables[i] > 0 && "should not parse literals, but only variables");
            if (coprocessor != nullptr) { coprocessor->freezeExtern(projectionVariables[i]); }
            projectionVariables [i] --;                    // convert into minisat representation!
        }
        useProjection = true;
    } else {
        useProjection = false;
    }

}

void EnumerateMaster::setModelMinimization(int minimizationType)
{
    mType = minimizationType;
}

void EnumerateMaster::setCheckEvery(uint64_t checkEvery)
{
    checkForModelsEveryX = checkEvery;
}

void EnumerateMaster::setMinimizeReceived(int mini)
{
    minimizeReceivedBlockingClauses = mini;
}


void EnumerateMaster::writeModelToStream(ostream& outputStream, const Riss::vec< Riss::lbool >& truthValues)
{
    stringstream valueStream;
    valueStream << "v ";
    if (usesProjection()) {
        for (int i = 0 ; i < projectionVariables.size(); ++ i) {  // use only projection variables
            valueStream << (truthValues[ projectionVariables[i] ] == l_False ? "-" : "") <<  projectionVariables[i] + 1 << " ";
        }
    } else {
        for (int i = 0 ; i < truthValues.size(); ++ i) {  // build model in memory first, print it in one line afterwards
            valueStream << (truthValues[i] == l_False ? "-" : "") <<  i + 1 << " ";
        }
    }
    valueStream << "0";

    outputStream << valueStream.str() << endl;
}

void EnumerateMaster::printSingleModel(ostream& outputStream, vec< lbool >& truthValues)
{
    if (coprocessor != nullptr && ! usesProjection()) { coprocessor->extendModel(truthValues); }
    writeModelToStream(outputStream, truthValues);
}


void EnumerateMaster::writeStreamToFile(string filename, bool toout)
{
    if (outputFileName == "" && filename == "" && !toout) { return; }
    lock();
    // get all models back to original size (in case variable elimination addition have been used)
    if (! printEagerly && coprocessor != nullptr && ! usesProjection()) {
        for (int i = 0 ; i < models.size(); ++ i) {
            vec<lbool>& model = (*models[i]);
            coprocessor->extendModel(model);
        }
    }
    if (outputFileName != "" || filename != "") {
        std::ofstream file(filename == "" ? outputFileName.c_str() : filename.c_str());
        for (int i = 0 ; i < models.size(); ++ i) {
            vec<lbool>& model = (*models[i]);
            writeModelToStream(file, model);
        }
        file.close();
    }

    if (toout && ! printEagerly) {  // print only, if not printed before already
        for (int i = 0 ; i < models.size(); ++ i) {
            vec<lbool>& model = (*models[i]);
            writeModelToStream(cout, model);
        }
    }
    unlock();
}

void EnumerateMaster::setDNFfile(string filename)
{
    DNFfileName = filename;
}

void EnumerateMaster::setModelFile(string filename)
{
    outputFileName = filename;
}

void EnumerateMaster::setFullModelFile(string filename)
{
    fullModelsFileName = filename;
}

void EnumerateMaster::setProjectionFile(string filename)
{
    projectionFileName = filename;
}

void EnumerateMaster::setPreprocessor(Coprocessor::Preprocessor* preprocessor)
{
    assert(coprocessor == nullptr && "should set preprocessor only once");
    coprocessor = preprocessor;
}

void EnumerateMaster::setPrintEagerly(bool p)
{
    printEagerly = p;
}

void EnumerateMaster::setReceiveModels(bool r)
{
    shareBlockingClauses = r;
}

void EnumerateMaster::activateNaiveBacktrackingEnumeration()
{
    enumerateWithBacktracking = true;
}

bool EnumerateMaster::usesBacktrackingEnumeration() const
{
    return enumerateWithBacktracking;
}



