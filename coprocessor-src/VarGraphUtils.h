#ifndef VARGRAPHUTILS_H
#define VARGRAPHUTILS_H

#include "CoprocessorTypes.h"

// forward declaration
namespace Coprocessor {

class VarGraphUtils
{
  public:

  /* Partition the variables into _noParts_ disjoint sets of variables, such that the sets
   * have a pairwise "distance" of _bufferSize_ variables. */
  void partVars(int noParts, std::vector<std::vector<Var> >& variables, int bufferSize);

  void partClauses(std::vector<std::vector<Clause> >& variables, int bufferSize);

  void sortVars(std::vector<Var>& variables);

  protected:
  private:
};

inline void partVars(int noParts, std::vector<std::vector<Var> >& variables, int bufferSize)
{
  //TODO: find good starting points
  std::vector<std::vector<Var> > buffers;    // buffer vectors for breadth-first-search
  std::vector<std::vector<Var> > finalVars;  // vectors containing partitioned variables

  MarkArray locked;   // global array for locked variables
    locked.create( solver->nVars() );
    locked.nextStep();

  bool finished = false;
  while(!finished)
  {
  std::vector<Var> & buffer;
    for (int i = 0; i < noParts; ++i)
    {
      // do breadth-first-search-step for every starting point
      buffer = buffers[i];
      for (int j = 0; j < buffers[i].size(); ++j)
      {
        Var & active = buffer[j];
        // find all succesors for _active_ -> mark1()

        // chech local

        // check global

        // update buffer and var array and locks
      }
    }
    // test if all buffers are empty -> finished!
  }
}

inline void partClauses(std::vector<std::vector<Clause> >& variables, int bufferSize)
{

}

inline void sortVars(std::vector<Var>& variables)
{

}
}

#endif // VARGRAPHUTILS_H
