// 09 23 2026
/* purpose
* One owner for the explicit C++/JSON behavior-source selector. Movement,
* collision, and animation each read their own `behaviorSource` key through this
* header so the selector vocabulary (`cpp` / `json`) and its parsing exist once.
*
* C++ is always the compiled fallback and the rollback path. JSON may own
* authored values/behavior data but must not silently change movement or
* collision semantics; the parity harness compares the two implementations.
*
* Hot-only header: not a GameAPI context field. Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace MimitaBehavior {

enum class Source : std::uint8_t { Cpp = 0, Json = 1 };

// Reads `behaviorSource` from one config file. Missing file or invalid JSON
// returns `fallback` (C++ by default), never throwing into a hot caller.
inline Source readSource(const char* path, Source fallback = Source::Cpp)
{
    std::ifstream file(path);
    if (!file)
        return fallback;
    try {
        const nlohmann::json j =
            nlohmann::json::parse(file, nullptr, true, true);
        return j.value("behaviorSource", std::string("cpp")) == "json"
                   ? Source::Json
                   : Source::Cpp;
    } catch (...) {
        return fallback;
    }
}

inline Source movementSource() { return readSource("config/movement.json"); }
inline Source collisionSource() { return readSource("config/collision.json"); }
inline Source animationSource() { return readSource("config/animations.json"); }

inline const char* name(Source source)
{
    return source == Source::Json ? "json" : "cpp";
}

} // namespace MimitaBehavior
