/**
 * @author Lucas Kahlert <lucas.kahlert@tu-dresden.de>
 */

#include "riss/utils/Compression.h"

using namespace std;
using namespace Riss;

int main()
{
    cout << "Start testing Compression map ... ";

    Compression compression = Riss::Compression();

    vector<Var> mapping(10);
    vector<lbool> trail(10);

    fill(trail.begin(), trail.end(), l_Undef);

    mapping[0] = 0;
    mapping[1] = 1;
    mapping[2] = Compression::UNIT;
    mapping[3] = 2;
    mapping[4] = 3;
    mapping[5] = Compression::UNIT;
    mapping[6] = 4;
    mapping[7] = 5;
    mapping[8] = 6;
    mapping[9] = 7;

    trail[2] = l_True;
    trail[5] = l_False;

    compression.update(mapping, trail);

    // test number of variables
    assert(compression.nvars() == 10);
    assert(compression.postvars() == 8);

    // test forward mapping
    assert(compression.exportVar(0) == 0);
    assert(compression.exportVar(1) == 1);
    assert(compression.exportVar(2) == 3);
    assert(compression.exportVar(3) == 4);
    assert(compression.exportVar(4) == 6);
    assert(compression.exportVar(5) == 7);
    assert(compression.exportVar(6) == 8);
    assert(compression.exportVar(7) == 9);

    // test mapping
    assert(compression.importVar(0) == 0);
    assert(compression.importVar(1) == 1);
    assert(compression.importVar(2) == var_Undef);
    assert(compression.importVar(3) == 2);
    assert(compression.importVar(4) == 3);
    assert(compression.importVar(5) == var_Undef);
    assert(compression.importVar(6) == 4);
    assert(compression.importVar(7) == 5);
    assert(compression.importVar(8) == 6);
    assert(compression.importVar(9) == 7);

    cout << "OK" << endl;

    return 0;
}
