/*****************************************************************************************[Main.cc]
Copyright (c) 2013, Norbert Manthey, All rights reserved.

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "riss/ipasir.h"
#include "riss/librissc.h"
#include "coprocessor/libcoprocessorc.h"
#include "pfolio/libprissc.h"
#include "riss/NuSVM_SolverInterface.h"

#include <iostream>

void testIPASIR()
{

    std::cerr << "c solver: " << ipasir_signature()  << std::endl;

    void * riss = ipasir_init();

    ipasir_add(riss, 1);
    ipasir_add(riss, 2);
    ipasir_add(riss, 0);

    ipasir_solve(riss);

    ipasir_val(riss, 1);

    ipasir_assume(riss, 1);
    ipasir_failed(riss, 1);

//   ipasir_set_terminate (riss, void * state, int (*terminate)(void * state));

    ipasir_release(riss);
}

void testNuSVM()
{
    int clause [3];
    clause[0] = 1; clause[1] = 2; clause[2] = 3;

    // NuSVM Interface
    MiniSat_ptr riss = MiniSat_Create();
    MiniSat_Set_Random_Seed(riss, 0);
    MiniSat_Set_Preferred_Variable(riss, 1);

    MiniSat_Nof_Variables(riss);
    MiniSat_Nof_Clauses(riss);
    MiniSat_New_Variable(riss);
    MiniSat_New_Variable(riss);
    MiniSat_New_Variable(riss);
    MiniSat_Add_Clause(riss, clause, 3);
    MiniSat_Solve(riss);
    MiniSat_Solve_Assume(riss, 3, clause);
    MiniSat_simplifyDB(riss);
    MiniSat_Get_Value(riss, 1);

    int conflict [ MiniSat_Get_Nof_Conflict_Lits(riss) ];
    MiniSat_Get_Conflict_Lits(riss, conflict);

    MiniSat_Set_Polarity_Mode(riss, polarity_false);
    MiniSat_Get_Polarity_Mode(riss);

    MiniSat_Clear_Preferred_Variables(riss);

    MiniSat_Delete(riss);
}

int main(int argc, char** argv)
{
    std::cerr << "c check implemented solver interfaces ... " << std::endl;

    testIPASIR();
    testNuSVM();

    return 0;
}
