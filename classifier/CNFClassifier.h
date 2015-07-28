/*********************************************************************************[CNFClassifier.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef RISS_CNFCLASSIFIER_H
#define RISS_CNFCLASSIFIER_H

#include "riss/core/Solver.h"
#include "classifier/Graph.h"
#include "classifier/BipartiteGraph.h"

// using namespace Riss;
// using namespace std;

/**
 * Main class that performs the feature extraction and classification
 *
 * TODO: might be split into FeatureExtraction and CNFClassifier class
 */
class CNFClassifier
{
    Riss::ClauseAllocator& ca;      // clause storage
    Riss::vec<Riss::CRef>& clauses; // std::vector to indexes of clauses into storage
    int nVars;                      // number of variables
    std::vector<std::string> featuresNames;
    // parameters for features
    bool computeBinaryImplicationGraph; // create binary implication graph
    bool computeConstraints;            // create an ADDER, BlockedAdder and ExactlyOne graph - implies binary implication graph, necessary to find the other things
    bool computingClausesGraph, computingResolutionGraph, computingRWH, computingVarGraph;
    bool computeXor; // xor features
    bool dumpingPlots;
    const char* plotsFileName;
    const char* outputFileName;
    const char* attrFileName;
    int quantilesCount;
    int precision;
    int maxClauseSize;
    bool computingDerivative;
    std::vector<int> timeIndexes;
    double mycpuTime;
    int verb;

    uint64_t buildClausesAndVariablesGrapths(BipartiteGraph& clausesVariablesP,
            BipartiteGraph& clausesVariablesN,
            std::vector<double>& ret);
    uint64_t buildResolutionAndClausesGrapths(const BipartiteGraph& clausesVariablesP,
            const BipartiteGraph& clausesVariablesN,
            std::vector<double>& ret);
    void fband(std::vector<double>& ret);
    void symmetrycode(std::vector<double>& ret);

    /** calculate the recursive weight heuristic value for each literal for multiple iterations */
    void recursiveWeightHeuristic_code(const int maxClauseSize, std::vector< double >& ret);

    /** tries to extract xor gates, and lists them according as sequence (not sure how to really do this) */
    void extractXorFeatures(const std::vector<std::vector<Riss::CRef> >& litToClsMap, std::vector<double>& ret);

  public:
    CNFClassifier(Riss::ClauseAllocator& _ca, Riss::vec<Riss::CRef>& _clauses, int _nVars);

    ~CNFClassifier();

    /** return config for given file */
    //std::string getConfig( Riss::Solver& S, string dbName );
    std::string getConfig(Riss::Solver& S);
    /** return the names of the features */
    std::vector<std::string> featureNames();


    /** extract the features of the given formula */
    std::vector<double> extractFeatures(std::vector<double>& ret);

    std::vector<double> outputFeatures(const char* formulaName);

    bool isComputingClausesGraph() const
    {
        return computingClausesGraph;
    }

    void setComputingClausesGraph(bool computingClausesGraph)
    {
        this->computingClausesGraph = computingClausesGraph;
    }

    bool isComputingResolutionGraph() const
    {
        return computingResolutionGraph;
    }

    void setComputingResolutionGraph(bool computingResolutionGraph)
    {
        this->computingResolutionGraph = computingResolutionGraph;
    }

    int getQuantilesCount() const
    {
        return quantilesCount;
    }

    void setQuantilesCount(int quantilesCount)
    {
        this->quantilesCount = quantilesCount;
    }

    bool isDumpingPlots() const
    {
        return dumpingPlots;
    }

    void setDumpingPlots(bool dumpingPlots)
    {
        this->dumpingPlots = dumpingPlots;
    }

    const char* getPlotsFileName() const
    {
        return plotsFileName;
    }

    void setPlotsFileName(const char* plotsFileName)
    {
        dumpingPlots = (plotsFileName != nullptr);
        this->plotsFileName = plotsFileName;
    }

    bool isComputeBinaryImplicationGraph() const
    {
        return computeBinaryImplicationGraph;
    }

    void setComputeBinaryImplicationGraph(bool computeBinaryImplicationGraph)
    {
        this->computeBinaryImplicationGraph = computeBinaryImplicationGraph;
    }

    bool isComputeConstraints() const
    {
        return computeConstraints;
    }

    void setComputeConstraints(bool computeConstraints)
    {
        this->computeConstraints = computeConstraints;
    }

    bool isComputeXor() const
    {
        return computeXor;
    }

    void setComputeXor(bool computeXor)
    {
        this->computeXor = computeXor;
    }

    const char* getOutputFileName() const
    {
        return outputFileName;
    }

    void setOutputFileName(const char* outputFileName)
    {
        this->outputFileName = outputFileName;
    }

    const char* getAttrFileName() const
    {
        return attrFileName;
    }

    void setAttrFileName(const char* attrFileName)
    {
        this->attrFileName = attrFileName;
    }

    int getPrecision() const
    {
        return precision;
    }

    void setPrecision(int precision = 10)
    {
        this->precision = precision;
    }

    bool isComputingDerivative() const
    {
        return computingDerivative;
    }

    void setComputingDerivative(bool computingDerivative)
    {
        this->computingDerivative = computingDerivative;
    }

    const std::vector<int>& getTimeIndexes() const
    {
        return timeIndexes;
    }

    bool isComputingRwh() const
    {
        return computingRWH;
    }

    void setComputingRwh(bool computingRwh)
    {
        computingRWH = computingRwh;
    }

    const std::vector<std::string>& getFeaturesNames() const
    {
        return featuresNames;
    }

    double getCpuTime() const
    {
        return mycpuTime;
    }

    void setCpuTime(double cpuTime)
    {
        this->mycpuTime = cpuTime;
    }

    int getVerb() const
    {
        return verb;
    }

    void setVerb(int verb)
    {
        this->verb = verb;
    }

    bool isComputingVarGraph() const
    {
        return computingVarGraph;
    }

    void setComputingVarGraph(bool computingVarGraph)
    {
        this->computingVarGraph = computingVarGraph;
    }
};

#endif // CNFCLASSIFIER_H
