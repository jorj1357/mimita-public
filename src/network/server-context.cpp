// 09 14 2026
/* purpose
* Implements the transient authoritative server-context handle.
* Does NOT own the server state.
*/
#include "network/server-context.h"

namespace MimitaNet {

namespace {
ServerContextV1* gActiveContext = nullptr;
}

ServerContextV1* activeServerContext()
{
    return gActiveContext;
}

void setActiveServerContext(ServerContextV1* context)
{
    gActiveContext = context;
}

} // namespace MimitaNet
