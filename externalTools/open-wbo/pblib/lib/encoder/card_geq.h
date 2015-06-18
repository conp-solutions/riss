#ifndef CARD_GEQ_H
#define CARD_GEQ_H

#include "../SimplePBConstraint.h"
#include "../IncSimplePBConstraint.h"
#include "../PBConfig.h"
#include "../clausedatabase.h"
#include "../auxvarmanager.h"
#include "../weightedlit.h"
#include "../IncrementalData.h"

// Cardinality Networks and their Applications (SAT 2009)
// Roberto As ́ın, Robert Nieuwenhuis, Albert Oliveras, Enric Rodr ́ıguez-Carbonell
class CardEncodingGeq
{  
private:

    PBConfig config;
    bool isBoth;
    bool isInc = false;
    
    vector<Lit> outlits;

    ClauseDatabase * formula;
    AuxVarManager * auxvars;
    
    vector<Lit> x;
    vector<Lit> units;
    Lit n;
    int64_t k;
    
    void hmerge(vector<Lit> a_s, vector<Lit> b_s, vector<Lit> c_s);
    void hsort(vector<Lit> a_s, vector<Lit> c_s);
    void smerge(vector<Lit> a_s, vector<Lit> b_s, vector<Lit> c_s);
    void card(vector< Lit > a_s, vector< Lit > c_s, int64_t k);

  
public:
    void encode(const SimplePBConstraint& pbconstraint, ClauseDatabase & formula, AuxVarManager & auxvars);
    
    CardEncodingGeq (PBConfig & config);
    virtual ~CardEncodingGeq();
};

#endif // CARDENCODING_H
