/*
 * Interface for spliter and instance solver.
 * Both will be stored in the threadData as solver.
 *
 * Author: Franziska
 *
 */

#ifndef ISOLVER_H
#define ISOLVER_H

namespace Pcasso
{
class SolverInterface
{

  public:
    virtual ~SolverInterface()
    {
    }

    virtual void interrupt() = 0;
};
}

#endif //ISOLVER_H
