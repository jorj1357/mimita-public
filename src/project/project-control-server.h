// 09 12 2026
/* purpose
* Minimal loopback TCP line server so an external agent can drive the same
* project authoring commands as the terminal.
* One command per line in, one response line out. Localhost only.
* Does NOT own project logic; it forwards to ProjectControl.
*/
#pragma once

#include <cstdint>

namespace Project {

class ControlServer {
public:
    static ControlServer& instance();

    bool start(std::uint16_t port);
    void stop();
    bool running() const;
    std::uint16_t port() const;

    // Implementation detail, public only so the worker function can name it.
    struct Impl;

private:
    ControlServer() = default;
    ControlServer(const ControlServer&) = delete;
    ControlServer& operator=(const ControlServer&) = delete;

    Impl* impl_ = nullptr;
};

} // namespace Project
