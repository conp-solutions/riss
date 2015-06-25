/************************************************************************************[CoreConfig.h]

Copyright (c) 2013, Norbert Manthey, All rights reserved.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_CoreConfig_h
#define RISS_CoreConfig_h

#include "riss/utils/Config.h"
#include "riss/utils/Options.h"

namespace Riss
{

/** This class should contain all options that can be specified for the pfolio solver
 */
class PfolioConfig : public Config
{
    /** pointer to all options in this object - used for parsing and printing the help! */
    Riss::vec<Option*> configOptions;


  public:
    /** default constructor, which sets up all options in their standard format */
    PfolioConfig(const std::string& presetOptions = "");


    BoolOption send;
    BoolOption receive; // remove elements on watch list faster, but unsorted
    BoolOption proofCounting; // interrupt after preprocessing
};

}

#endif
