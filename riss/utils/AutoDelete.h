/************************************************************************************[AutoDelete.h]

Copyright (c) 2014, Norbert Manthey, LGPL v2, see LICENSE

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#ifndef RISS_AUTODELETE_H
#define RISS_AUTODELETE_H

#include <malloc.h>

/** this object frees a pointer before a method /statementblock is left */
class MethodFree
{
    void*& pointer;
  public:
    MethodFree(void*& ptr) : pointer(ptr) {}
    ~MethodFree() { free(pointer); pointer = 0; }   // also set pointer to 0!
};

#endif
