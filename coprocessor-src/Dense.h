/*****************************************************************************************[Dense.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef DENSE_H
#define DENSE_H

#include "coprocessor-src/CoprocessorTypes.h"
#include "coprocessor-src/Propagation.h"

#include "coprocessor-src/Technique.h"

using namespace Minisat;
using namespace std;

namespace Coprocessor {

class Dense  : public Technique
{
  CoprocessorData& data;
  Propagation& propagation;
  
  struct Compression {
    // mapping from old variables to new ones
    int* mapping;
    uint32_t variables;	// number of variables before compression
    vector<Lit> trail;	// already assigned literals
    
    Compression() : mapping(0), variables(0) {};
    /// free the used resources again
    void destroy() {
      if(mapping!=0) delete[] mapping;
      mapping = 0;
    }
  };
  
  vector< Compression > map_stack;

  
public:
  Dense(ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation);

  
  /** compress the formula - if necessary, output a new whiteFile */
  void compress(const char *newWhiteFile = 0);

  /** undo variable mapping, so that model is a model for the original formula*/
  void decompress(vec< lbool >& model);
  
  /** inherited from @see Technique */
  void printStatistics( ostream& stream );
  
  void destroy();
  
  
  /** write dense information to file, so that it can be loaded afterwards again */
  bool writeUndoInfo(const string& filename);
  
  /** read dense information from file */
  bool readUndoInfo(const string& filename);
  
protected:

  unsigned globalDiff;
};


}

#endif // RESOLVING_H
