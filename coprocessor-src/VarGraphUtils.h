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
  void partVars(CoprocessorData& data, int noParts, std::vector<std::vector<Var> >& variables, int bufferSize);

  void partClauses(CoprocessorData& data, std::vector<std::vector<Clause> >& variables, int bufferSize);

  void sortVars(CoprocessorData& data, std::vector<Var>& variables);

  protected:
  private:
};

inline void partVars(CoprocessorData& data, int noParts, std::vector<std::vector<Var> >& variables, int bufferSize)
{
  //TODO: find good starting points
  //TODO: use sets instead of vectors - adding only unique elements, find() method, ... ?
  std::vector<std::vector<Var> > buffers;    // buffer vectors for breadth-first-search
  std::vector<std::vector<Var> > finalVars;  // vectors containing partitioned variables

  MarkArray locked;   // global array for locked variables
    locked.create( data.nVars() );
    locked.nextStep();

  MarkArray tmp;
  tmp.create( data.nVars() );
  tmp.nextStep();
    
  bool finished = false;

  // ---------- initialize buffers --------------
  MarkArray a, b, c, t;
    a.create(data.nVars());
    a.nextStep();
    b.create(data.nVars());
    b.nextStep();
    c.create(data.nVars());
    c.nextStep();
    t.create(data.nVars());
    t.nextStep();
  int conflict = -1;
  unsigned int count = 0;
  // Get a free variable
  for (int v = 0; v < data.nVars(); ++v)
  {
    data.mark2(v, a, t);
    for (int i = 0; i < a.size(); ++i)
    {
      if (a.isCurrentStep(i)) {
        data.mark2(i, b, tmp);
      }
    }
    // check if there are intersections
    for (int i = 0; i < a.size(); ++i)
    {
      if (b.isCurrentStep(i) && c.isCurrentStep(i) ) { // TODO Norbert: added "i" here, is this right?
        conflict = i;
        break;
      }
    }
    if (conflict != -1)
    {
        c = b;
        buffers[count].push_back(conflict);
        count++;
    }
  }
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
          array.create(data.nVars());
          array.nextStep();
        data.mark1(active, array);  // all marked variables are connected to the active node
        int k;
        for (k = 0; k < array.size(); ++k)
        {
          if (array.isCurrentStep(k))   // _k_ is successor
          {
            // TODO: deal with _active_ being successor of itself
            if (locked.isCurrentStep(k) ) { // k is already locked
              // check if is owned by this search instance
              bool foundItem = false;
	      for( int i = 0 ; i < buffer.size(); ++i ) 
		if( buffer[i] == k ) {foundItem = true; break; }
	      if( foundItem ) break;
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
    for ( int i = 0 ; i < buffers.size(); ++ i )
    {
      if (! buffers[i].empty())
        finished = false;
    }
  }
  // after this while loop, the final vectors contain the partitions.
}

inline void partClauses(CoprocessorData& data,std::vector<std::vector<Clause> >& variables, int bufferSize)  
{
  // TODO: implement partitioning for clauses
}

inline void sortVars(CoprocessorData& data, std::vector<Var>& variables)
{
  // TODO: sort a vector of variables according to partitions
  // TODO: write start and and position of each partition into a separate vector
  // TODO: pass another argument for number of partitions?
}
}

#endif // VARGRAPHUTILS_H
