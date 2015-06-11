#include "LevelPool.h"

using namespace Pcasso;
using namespace std;
using namespace Riss;

namespace PcassoDavide
{

LevelPool::LevelPool(int _max_size)
{
    full          = false;
    writeP        = 0;
    endP          = 0;
    levelPoolLock = RWLock();
    max_size      = (unsigned int)_max_size; // Davide> Safe, _max_size < INT_MAX
    shared_clauses.growTo((_max_size * 1000) / 4, lit_Undef);
}

bool LevelPool::add_shared(vec<Lit>& lits, unsigned int nodeID, bool disable_dupl_removal, bool disable_dupl_check)
{

    vec<Lit> temp;
    temp.push(lit_Undef);
    for (int i = 0; i < lits.size(); i++) {
        temp.push(lits[i]);
    }

    assert(temp[0] == lit_Undef);

    int i = 0;

    temp[0] = toLit(nodeID);

    assert(temp.size() < shared_clauses.size());

    if (writeP + temp.size() < shared_clauses.size() - 2) {   // Space for the ending lit_Undef
        for (i = 0; i < temp.size(); ++i) {
            shared_clauses[writeP + i] = temp[i];
        }
        writeP += temp.size();
        int k = writeP;
        ++writeP;
        while (shared_clauses[k] != lit_Undef) {
            shared_clauses[k++] = lit_Undef;
        }
        if (writeP > endP) { endP = writeP; }
    } else { // write from the beginning
        Debug::PRINTLN_NOTE("write from beginning!");
        full = true;

        endP = writeP;
        assert(shared_clauses[endP - 1] == lit_Undef);

        for (writeP = 0; writeP < temp.size(); ++writeP) {
            shared_clauses[writeP] = temp[writeP];
        }
        int k = writeP;
        ++writeP;
        while (shared_clauses[k] != lit_Undef) {
            shared_clauses[k++] = lit_Undef;
        }
        assert(endP > writeP);
    }
    return true;
}
void
LevelPool::getChunk(int readP, vec<Lit>& chunk)
{
    assert(writeP == 0 || shared_clauses[writeP - 1] == lit_Undef);

    if (readP >= endP) { return; }

    if (((readP != 0 && shared_clauses[readP - 1] == lit_Undef) || readP == 0)) {
        if (readP <= writeP) {
            chunk.growTo(writeP - readP);
            int j = 0;
            for (int i = readP; i < writeP; ++i) {
                assert(writeP - readP == chunk.size());
                assert(j < chunk.size());
                chunk[j++] = shared_clauses[i];
            }
            assert(chunk.size() == j);
            assert(j == 0 || chunk[j - 1] == lit_Undef);
        } else {
            chunk.growTo((endP - readP) + writeP);
            int j = 0;
            for (int i = readP; i < endP; ++i) {
                assert(j < chunk.size());
                chunk[j++] = shared_clauses[i];
            }
            for (int i = 0; i < writeP; ++i) {
                assert(j < chunk.size());
                chunk[j++] = shared_clauses[i];
            }
            assert(chunk.size() == j);
            assert(j == 0 || chunk[j - 1] == lit_Undef);
        }
    } else {
        chunk.growTo(writeP);
        int j = 0;
        for (int i = 0; i < writeP; ++i) {
            assert(j < chunk.size());
            chunk[j++] = shared_clauses[i];
        }
        assert(chunk.size() == j);
        assert(j == 0 || chunk[j - 1] == lit_Undef);
    }
    // The else case is not strictly necessary. The clauses will be read at next
    // iteration, even thoug some will be missed.
}

} // namespace PcassoDavide