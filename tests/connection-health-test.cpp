// 08 08 2026, 12 00
/* purpose
* Unit tests for the pure client connection-health state machine.
* Verifies Connected <-> WeakConnection <-> Reconnecting transitions, the
* 60s reconnect grace give-up, and that terminal states are never moved.
* Does NOT touch sockets, the notification system, or any game UI.
*/

#include "network/connection-health.h"

#include <cstdio>

using MimitaNet::ConnectionState;

static int gFailures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++gFailures;                                                  \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        const auto va = (a);                                              \
        const auto vb = (b);                                              \
        if (va != vb) {                                                   \
            std::printf("FAIL %s:%d: %s == %s (got %d, want %d)\n",       \
                        __FILE__, __LINE__, #a, #b, (int)va, (int)vb);    \
            ++gFailures;                                                  \
        }                                                                 \
    } while (0)

static ConnectionState step(ConnectionState cur, uint64_t now, uint64_t lastHeardAge,
                            bool heardSinceDisconnect, uint64_t graceDeadline = 0)
{
    constexpr uint64_t STALE_MS = 2500;
    constexpr uint64_t HARD_MS = 10000;
    constexpr uint64_t GRACE_MS = 60000;
    return MimitaNet::mpNextConnectionHealth(
        cur, now, lastHeardAge, heardSinceDisconnect,
        STALE_MS, HARD_MS, graceDeadline, GRACE_MS);
}

int main()
{
    const uint64_t t0 = 1'000'000;

    // A. Fresh packets keep the session Connected.
    CHECK_EQ(step(ConnectionState::Connected, t0, 100, false),
             ConnectionState::Connected);

    // B. Packets older than the stale threshold -> WeakConnection (no kick).
    CHECK_EQ(step(ConnectionState::Connected, t0, 4000, false),
             ConnectionState::WeakConnection);

    // Recovery from weak back to connected when packets flow again.
    CHECK_EQ(step(ConnectionState::WeakConnection, t0, 100, false),
             ConnectionState::Connected);

    // Boundary: exactly at the stale threshold is still connected (> required).
    CHECK_EQ(step(ConnectionState::Connected, t0, 2500, false),
             ConnectionState::Connected);

    // C. Packets older than the hard timeout -> Reconnecting (grace starts).
    CHECK_EQ(step(ConnectionState::WeakConnection, t0, 12000, false),
             ConnectionState::Reconnecting);
    // A healthy session can jump straight to Reconnecting too.
    CHECK_EQ(step(ConnectionState::Connected, t0, 12000, false),
             ConnectionState::Reconnecting);
    // Boundary: exactly at the hard timeout is still weak, not reconnecting
    // (> required), but still over the stale threshold.
    CHECK_EQ(step(ConnectionState::Connected, t0, 10000, false),
             ConnectionState::WeakConnection);

    // D. Reconnecting with a fresh packet heard since disconnect -> recovered.
    CHECK_EQ(step(ConnectionState::Reconnecting, t0, 3000, true),
             ConnectionState::Connected);

    // E. Reconnecting, no fresh packet, grace still open -> keep trying.
    CHECK_EQ(step(ConnectionState::Reconnecting, t0 + 30'000, 40000, false,
                  t0 + 60'000),
             ConnectionState::Reconnecting);

    // F. Reconnecting, grace window expired -> ReconnectFailed.
    CHECK_EQ(step(ConnectionState::Reconnecting, t0 + 61'000, 71000, false,
                  t0 + 60'000),
             ConnectionState::ReconnectFailed);
    // Expired at exactly the deadline.
    CHECK_EQ(step(ConnectionState::Reconnecting, t0 + 60'000, 70000, false,
                  t0 + 60'000),
             ConnectionState::ReconnectFailed);

    // G. Terminal / pre-connect states are never moved by the machine.
    CHECK_EQ(step(ConnectionState::Disconnected, t0, 12000, false),
             ConnectionState::Disconnected);
    CHECK_EQ(step(ConnectionState::ReconnectFailed, t0, 12000, false),
             ConnectionState::ReconnectFailed);
    CHECK_EQ(step(ConnectionState::HostClosed, t0, 12000, false),
             ConnectionState::HostClosed);
    CHECK_EQ(step(ConnectionState::Kicked, t0, 12000, false),
             ConnectionState::Kicked);
    CHECK_EQ(step(ConnectionState::ServerCrashed, t0, 12000, false),
             ConnectionState::ServerCrashed);
    CHECK_EQ(step(ConnectionState::Connecting, t0, 5000, false),
             ConnectionState::Connecting);
    CHECK_EQ(step(ConnectionState::WaitJoinAccept, t0, 5000, false),
             ConnectionState::WaitJoinAccept);

    if (gFailures == 0)
    {
        std::printf("[connection-health-test] ALL PASS\n");
        return 0;
    }
    std::printf("[connection-health-test] %d FAILURES\n", gFailures);
    return 1;
}
