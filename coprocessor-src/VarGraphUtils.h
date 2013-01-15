#ifndef VARGRAPHUTILS_H
#define VARGRAPHUTILS_H

#include "CoprocessorTypes.h"
#include <stdlib.h>
#include <time.h>

// forward declaration
namespace Coprocessor {

class VarGraphUtils
{
  public:

  /** Partition the variables into _noParts_ disjoint sets of variables.
   *
   * The partitions are build such that the sets have a pairwise "distance" of _bufferSize_ variables.
   *
   * @param data data object holding the variable information
   * @param noParts number of partitions
   * @param variables reference on the partitioned variable set
   * @param bufferSize distance between two partitions
   */
  void partVars(CoprocessorData& data, int noParts, std::vector<std::vector<Var> >& variables, int bufferSize);

  /** Partition the clauses with buffer space between them.
   *
   * The partitions are build such that the sets have a pairwise "distance" of _bufferSize_.
   *
   * @param data data object holding the variable information
   * @param clauses reference on the partitioned clause set
   * @param bufferSize distance between each two partitions
   */
  void partClauses(CoprocessorData& data, std::vector<std::vector<Clause> >& clauses, int bufferSize);

  /** Sort the set of variables such that the variables are sorted into partitions.
   *
   * The variables are sorted such that the variables that belong to one partition are right after each
   * other in the sorted vector.
   *
   */
  void sortVars(CoprocessorData& data, std::vector<Var>& variables);

  protected:
  private:
};

/**
 *
 */
inline void partVars(CoprocessorData& data, int noParts, std::vector<std::vector<Var> >& variables, int bufferSize)
{
  //TODO: find good starting points
  std::vector<std::vector<Var> > buffers;    // buffer vectors for breadth-first-search
  std::vector<std::vector<Var> > finalVars;  // vectors containing partitioned variables

  char *locked = (char*) malloc( sizeof(char) * data.nVars() );
  for (int i = 0; i < data.nVars(); ++i)
    locked[i] = 0;

  MarkArray tmp;
  tmp.create( data.nVars() );
  tmp.nextStep();

  bool finished = false;

  //TODO: adjacency list for graph search
  //TODO: use correct buffer size (lock »bufferSize« limits)

  // ---------- initialize buffers --------------
  MarkArray first, second, checked; //first -> distance 2 neighbors, second -> distance 4 neighbors + first, checked -> 
    first.create(data.nVars());
    first.nextStep();
    second.create(data.nVars());
    second.nextStep();
    checked.create(data.nVars());
    checked.nextStep();
  unsigned int count = 0;
  bool valid = false;

  srand( time(NULL) );
  while (count < noParts){
    int x = rand() % data.nVars();   // get a random variable

    first.nextStep();
    second.nextStep();
    tmp.nextStep();

    data.mark2(x, first, tmp);
    for (int i = 0; i < first.size(); ++i)
    {
      if (first.isCurrentStep(i)) {
        data.mark2(i, second, tmp);
      }
    }
    // check if there are intersections
    valid = true;
    for (int i = 0; i < first.size(); ++i)
    {
      if (second.isCurrentStep(i) && checked.isCurrentStep(i) ) {
        valid = false;
        break;
      } 
    }
    // if not valid remember to prevent from checking again! 
    if ( valid ) { 
        // add second to checked first
        for (int j = 0; j < second.size(); ++j){
          if (second.isCurrentStep(j))
            checked.setCurrentStep(j);
          }
           // check if the variable is valid
        buffers[count].push_back(x);
        count++;
    }
  }
  // --------- end initialize buffers -----------

  MarkArray array;              // mark array for finding successors
  array.create(data.nVars());
  array.nextStep();

  tmp.nextStep();

  while(!finished)
  {
  std::vector<Var> buffer;    // the working buffer
    for (int partition = 0; partition < noParts; ++partition)
    {
      // do breadth-first-search-step for every node
      buffer = buffers[partition];
      int start_length = buffer.size();
      for (int j = 0; j < start_length; ++j)
      {
        Var & active = buffer[j];
        // find all succesors for _active_ -> mark1()
        array.nextStep();

        data.mark1(active, array);  // all marked variables are connected to the active node
        int k;
        for (k = 0; k < array.size(); ++k)
        {
          if (array.isCurrentStep(k)) // k is successor of active
          {
            if ( locked[k] == 0 )   // variable k is free
            {
              locked[k] = -(partition+1);       // lock the variable ...
              buffer.push_back(k);    // ... and add it to the buffer
            }
            else if (!locked[k] == partition+1 || !locked[k] == -(partition+1))
              break;  // variable is occupied by another partition
          }
        }
        // if no successor clashed with locked variables, we can add the active var to tha final var list
        if (k == array.size())  // this means that the was no lock clash
        {
          finalVars[partition].push_back(active);
          locked[active] = partition+1;
        }
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

/**
 *
 */
inline void partClauses(CoprocessorData& data,std::vector<std::vector<Clause> >& clauses, int bufferSize)
{
  // TODO: implement partitioning for clauses
}

/**
 *
 */
inline void sortVars(CoprocessorData& data, std::vector<Var>& variables)
{
  // TODO: sort a vector of variables according to partitions
  // TODO: write start and and position of each partition into a separate vector
  // TODO: pass another argument for number of partitions?
}

}

#endif // VARGRAPHUTILS_H
