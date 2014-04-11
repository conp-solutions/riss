/*********************************************************************************[PfolioConfig.cc]

Copyright (c) 2012-2013, Norbert Manthey

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "pfolio-src/PfolioConfig.h"

using namespace Minisat;

static const char* _cat = "PFOLIO";

PfolioConfig::PfolioConfig(const std::string & presetOptions) // add new options here!
:
 Config( &configOptions, presetOptions )

 ,send(         _cat, "ps", "enable clause sharing for all clients", true, optionListPtr )
 ,receive(      _cat, "pr", "enable receiving clauses for all clients", true, optionListPtr )
 ,proofCounting(_cat, "pc", "enable avoiding duplicate clauses in the pfolio DRUP proof", true, optionListPtr )
 
{}

