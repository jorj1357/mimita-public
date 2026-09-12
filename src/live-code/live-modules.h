// 09 12 2026
/* purpose
* Locate a named, ABI-matched function table inside the active GameAPI modules.
* Owns the module-name and struct-size validation used by EXE-side bridges.
* Does NOT call module functions or own hot-reload policy.
*/
#pragma once

#include <cstdint>

namespace LiveModules {

// Returns the function table for `name` only when its declared structSize
// matches expectedStructSize; otherwise nullptr.
const void* findFunctions(const char* name, std::uint32_t expectedStructSize);

} // namespace LiveModules
