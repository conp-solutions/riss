/*****************************************************************************************[Dense.h]
Copyright (c) 2013, Norbert Manthey, All rights reserved.
**************************************************************************************************/

#ifndef DENSE_H
#define DENSE_H

#include "coprocessor/CoprocessorTypes.h"
#include "coprocessor/Propagation.h"

#include "coprocessor/Technique.h"

// using namespace Riss;
// using namespace std;

namespace Coprocessor {

class Dense  : public Technique
{
  CoprocessorData& data;
  Propagation& propagation;
  
  struct Compression {
    // mapping from old variables to new ones
    int* mapping;
    uint32_t variables;	// number of variables before compression
    uint32_t postvariables;	// number of variables before compression
    std::vector<Lit> trail;	// already assigned literals
    
    Compression() : mapping(0), variables(0), postvariables(0) {};
    /// free the used resources again
    void destroy() {
      if(mapping!=0) delete[] mapping;
      mapping = 0;
    }
  };
  
  std::vector< Compression > map_stack;

  /// store to which new variable an old variable has been mapped
  std::vector< int > forward_mapping;
  
public:
  Dense(CP3Config &_config, ClauseAllocator& _ca, ThreadController& _controller, CoprocessorData& _data, Propagation& _propagation);

  
  /** compress the formula - if necessary, output a new whiteFile 
   * calls adoptUndoStack before it actually modifies the formula
   */
  void compress(const char *newWhiteFile = 0);

  /** decompress the most recent additions of the extend model std::vector, 
   *  does not touch lit_Undefs */
  void adoptUndoStack();
  
  /** undo variable mapping, so that model is a model for the original formula
   * adoptUndoStack should be called before this method!
   */
  void decompress(vec< lbool >& model);
  
  /** inherited from @see Technique */
  void printStatistics( std::ostream& stream );
  
  void destroy();
  
  /** write dense information to file, so that it can be loaded afterwards again */
  bool writeUndoInfo(const std::string& filename);
  
  /** read dense information from file */
  bool readUndoInfo(const std::string& filename);
  
  /** return the new variable for the old variable */
  Lit giveNewLit ( const Lit& l ) const ;
  
protected:

  unsigned globalDiff;

};


}

#endif // RESOLVING_H
