/**********************************************************************************[Compression.cc]
Copyright (c) 2015, Norbert Manthey, Lucas Kahlert

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <fstream>
#include <sstream>

#include "Compression.h"

using namespace std;


namespace Riss
{

static void skipComment(ifstream& file, string& line, uint& lineno)
{
    getline(file, line);
    lineno++;

    // skip comments
    while (line.length() > 0 && line[0] == 'c') {
        getline(file, line);
        lineno++;
    }
}

bool Compression::serialize(const string& filename, bool verbose) const
{
    ofstream file(filename.c_str(), ios_base::out);
    if (!file) {
        cerr << "c ERROR: could not open map - undo file " << filename << endl;
        return false;
    }

    // first line contains info about number of original variables and the number
    // of units (variables in the trail) - which is equal to the difference between
    // old and new variables
    file << "c Compression map (Coprocessor technique Dense)" << endl;

    // write a short format description if debug output is enabled
    if (verbose) {
        file << "c The first line contains the current compression mapping" << endl
             << "c and the second the trail (variable assignments, aka units)" << endl;

        file << "c " << endl
             << "c nvars:    " << nvars() << endl
             << "c postvars: " << postvars() << endl
             << "c diff:     " << diff() << endl
             << "c " << endl;
    }

    // the header contains a hint on the file format, the number of variable in the
    // original formula and the number of removed variables
    file << "p map " << nvars() << " " << diff() << endl;

    // write the whole mapping on a single line
    for (Var var = 0 ; var < nvars(); ++var) {
        file << importVar(var) << " ";
    }
    file << endl;

    // write a single line for the trail map
    for (Var var = 0 ; var < nvars(); ++var) {
        file << toInt(trail[var]) << " ";
    }
    file << endl;

    file.close();
    return true;
}

bool Compression::deserialize(const string& filename, bool verbose)
{
    string line;
    uint lineno = 0;

    uint variables = 0;
    uint diff = 0;

    ifstream file(filename.c_str(), ios_base::in);
    if (!file) {
        cerr << "c WARNING: could not open undo file " << filename << " (might not have been created)" << endl;
        return false;
    } else {
        cerr << "c opened var map " << filename << " for reading ... " << endl;
    }

    // skip leading comments
    skipComment(file, line, lineno);

    // mapping header
    if (line.find("p map") == 0) {
        istringstream stream;
        stream.str(line.substr(5));
        stream >> variables >> diff;

        // if a recoverable error occured during the integer parsing
        // cancel the whole parsing
        if (stream.fail()) {
            cerr << "Error: Cannot parse map header '" << line << "'" << endl;
            return false;
        }

    } else {
        cerr << "Expected map header at line " << lineno << ", get '" << line << "'" << endl;
        return false;
    }

    DOUT(cerr << "c variables: " << variables << endl << "c diff: " << diff << endl;);

    // initialze temporary mapping and trail
    // we do not use the member variables, because the parsing can fail and the current
    // compression must not be corrupted by an invalid mapping file
    vector<Var> tmpMapping;
    vector<lbool> tmpTrail;

    tmpMapping.reserve(variables);
    tmpTrail.reserve(variables);

    // go to next line not starting with a "c"
    skipComment(file, line, lineno);

    // parse compression mapping
    istringstream stream(line);

    Var var;

    while (stream >> var) {
        // validate mapping
        assert(var >= -1 && var < (int) variables
               && "Mapped variable must not be larger than number of variables");

        tmpMapping.push_back(var);
    }

    // assert, that we have all variables mapped
    if (tmpMapping.size() != variables) {
        cerr << "c Error: Mapping has invalid size. Variables="
             << variables << ", mapped=" << tmpMapping.size() << endl;
        return false;
    }

    skipComment(file, line, lineno);

    // parse trail mapping
    stream.clear();   // reset all flags
    stream.str(line);

    int unit;

    while (stream >> unit) {
        const lbool value = toLbool(unit);

        // validate lbool
        assert((value == l_True || value == l_False || value == l_Undef)
               && "Unknown lifted boolean value");

        tmpTrail.push_back(value);
    }

    // assert that the trail has sufficient length mapped
    if (tmpTrail.size() != variables) {
        cerr << "c Error: Trail has invalid size. Variables=" << variables
             << ", trail=" << tmpTrail.size() << endl;
        return false;
    }

    reset();
    update(tmpMapping, tmpTrail);

    return true;
}

void Compression::update(vector<Var>& _mapping, vector<lbool>& _trail)
{
    // get the highest possible variable in the compressed formula (the last element in the mapping), have to scan as we do not know the details in this method  
    Var highestVar = 0;
  
    // initialize mapping
    if (!isAvailable()) {

        assert(_mapping.size() == _trail.size() && "Mapping and trail must be of same size");

        // simply copying the content of the mappings
        mapping = _mapping;
        trail = _trail;

	// scan for the highest variable
	for (Var var = _mapping.size(); var > 0; var--) {
	  if( mapping[var - 1] != UNIT ) { // we found a variable that is eliminated. as we started from the end, this is the highest variable in the compressed formula
	    highestVar = mapping[var - 1];
	    break;
	  }
	}
	
        #if 0
        #ifndef NDEBUG
        // validate mapping in debug mode
        for (Var var = 0; var < mapping.size(); ++var) {
            const Var compressed = mapping[var];

            if (compressed == Compression::UNIT) {
                assert(trail[var] != l_Undef && "If variable is unit, the trail value must not be undefined");
            } else {
                assert(trail[var] == l_Undef && "If variable is not a unit, the trail value must be undefined");
            }
        }
        #endif
        #endif
    }
    // update current mapping
    else {

        assert(_mapping.size() <= forward.size() && "New mapping must be equal or smaller than current one");
        assert(_trail.size() <= trail.size() && "New trail must not be greater than orignal formula");

        // update mapping and trail at once
        for (Var var = 0; var < _mapping.size(); ++var) {
            const Var from = forward[var];  // old variable name translated into name in the original formula
            const Var to   = _mapping[var]; // new variable name in the compressed formula

            mapping[from] = to; // update mapping

            // update trail if the variable is a unit (will be removed in the compressed formula)
            if (to == UNIT) {
                assert(trail[from] == l_Undef && "Variable already marked as unit");

                trail[from] = _trail[var];
            } else { // if the variable is still present we have to consider it for being the highest possible variable
		if( highestVar < to ) highestVar = to;
	    }
        }
    }

    // update forward mapping
    // reduce size of the forward table
    // because 0 is also a variable, the forward mapping needs space for one additional variable
    forward.resize(highestVar + 1);

    // "to" is the variable in the compressed formula and "from" the name in the original formula
    if (forward.size() != 0) {
        for (Var from = 0, to = 0; from < mapping.size(); ++from) {
            // we found a variable in the old formula that is not a unit in the compressed
            // store the inverse direction in the forward map (to -> from) and increment
            // the "to" variable to write the next non-unit mapping to the next position
            if (mapping[from] != UNIT) {
                assert(((forward.size() == 0 && to == 0) || to < forward.size()) && "Compressed variable name must not be larger than forward mapping");

                forward[to++] = from;
            }
        }
    }
}

} // Riss namespace
