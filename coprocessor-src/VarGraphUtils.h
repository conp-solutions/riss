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
  //TODO: use sets instead of vectors - adding only unique elements, find() method, ... ?
  std::vector<std::vector<Var> > buffers;    // buffer vectors for breadth-first-search
  std::vector<std::vector<Var> > finalVars;  // vectors containing partitioned variables

  MarkArray locked;   // global array for locked variables
    locked.create( solver->nVars() );
    locked.nextStep();

  bool finished = false;

  // ---------- initialize buffers --------------

  // --------- end initialize buffers -----------
  while(!finished)
  {
  std::vector<Var> buffer;
    for (int i = 0; i < noParts; ++i)
    {
      // do breadth-first-search-step for every starting point
      buffer = buffers[i];
      for (int j = 0; j < buffers[i].size(); ++j)
      {
        Var & active = buffer[j];
        // find all succesors for _active_ -> mark1()
        MarkArray array;
          array.create(solver->nVars());
          array.nextStep();
        CoprocessorData::mark1(active, array);  // all marked variables are connected to the active node
        int k;
        for (k = 0; k < array.size(); ++k)
        {
          if (array.isCurrentStep(k))   // _k_ is successor
          {
            // TODO: deal with _active_ being successor of itself
            if (locked.isCurrentStep(k) ) { // k is already locked
              // check if is owned by this search instance
              if ( std::find(buffer.begin(), buffer.end(), k) == buffer.end() ) {
                break;
              }
            } else {
              locked.setCurrentStep(k);   // lock successor variable for further actions
              buffer.push_back(k);  // push sucessore into the buffer
            }
          }
        }
        // if no successor clashed with locked variables, we can add the active var to tha final var list
        if (k == array.size())  // this means that the was no lock clash
          finalVars[i].push_back(active);
      }
    }
    // test if all buffers are empty -> finished!
    finished = true;
    for (std::vector<Var>& v : buffers)
    {
      if (!v.empty())
        finished = false;
    }
  }
  // after this while loop, the final vectors contain the partitions.
}

inline void partClauses(std::vector<std::vector<Clause> >& variables, int bufferSize)
{
  // TODO: implement partitioning for clauses
}

inline void sortVars(std::vector<Var>& variables)
{
  // TODO: sort a vector of variables according to partitions
  // TODO: write start and and position of each partition into a separate vector
  // TODO: pass another argument for number of partitions?
}
}

#endif // VARGRAPHUTILS_H
