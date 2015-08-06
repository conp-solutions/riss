//
// Created by lucas on 05.08.15.
//

#include "riss/utils/Compression.h"

using namespace std;
using namespace Riss;

int main()
{
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
    compression.serialize("debug.map", true);

    assert(compression.deserialize("debug.map"));

    // validate mapping
    for (Var var = 0; var < mapping.size(); ++var) {
        assert(compression.importVar(var) == mapping[var]);
    }

    // validate forward mapping
    assert(compression.exportVar(0) == 0);
    assert(compression.exportVar(1) == 1);
    assert(compression.exportVar(2) == 3);
    assert(compression.exportVar(3) == 4);
    assert(compression.exportVar(4) == 6);
    assert(compression.exportVar(5) == 7);
    assert(compression.exportVar(6) == 8);
    assert(compression.exportVar(7) == 9);

    return 0;
}
