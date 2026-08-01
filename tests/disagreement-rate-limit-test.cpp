// 07 31 2026, 15 30
/* purpose
* Tests the disagreement broadcast rate-limit window.
* Verifies the exact example: events at ticks 1,5,17,18,35,53,61 with a 60-tick
* minimum render only at ticks 1 and 61.
* Compiles the standalone rate-limit header with no other dependencies.
* Does NOT open sockets, contact the network, or load the game engine.
*/

#include "network/disagreement-rate-limit.h"

#include <cstdio>

namespace {

int gPassed = 0;
int gFailed = 0;

void check(bool condition, const char* message)
{
    if (condition)
    {
        ++gPassed;
    }
    else
    {
        ++gFailed;
        std::printf("[FAIL] %s\n", message);
    }
}

void testExampleWindow()
{
    MimitaNet::DisagreementRateLimitState state;
    constexpr uint32_t kMinTicks = 60;

    const uint32_t events[] = {1, 5, 17, 18, 35, 53, 61};
    const bool expected[] = {true, false, false, false, false, false, true};

    for (size_t i = 0; i < 7; ++i)
    {
        const bool allowed =
            MimitaNet::shouldEmitDisagreement(state, events[i], kMinTicks);
        char message[128];
        std::snprintf(message, sizeof(message),
                      "tick %u allowed=%d (expected %d)",
                      events[i], (int)allowed, (int)expected[i]);
        check(allowed == expected[i], message);
    }
}

void testFirstEventAlwaysAllowed()
{
    MimitaNet::DisagreementRateLimitState state;
    check(MimitaNet::shouldEmitDisagreement(state, 1000, 60),
          "first event allowed regardless of tick");
}

void testExactBoundaryAllowed()
{
    MimitaNet::DisagreementRateLimitState state;
    MimitaNet::shouldEmitDisagreement(state, 10, 60);
    check(!MimitaNet::shouldEmitDisagreement(state, 69, 60),
          "event one tick before the window is blocked");
    check(MimitaNet::shouldEmitDisagreement(state, 70, 60),
          "event exactly minTicks later is allowed");
}

void testTickWraparound()
{
    MimitaNet::DisagreementRateLimitState state;
    MimitaNet::shouldEmitDisagreement(state, 0xFFFFFFF0u, 60);
    // Wraps past 0 back to a low tick; 70 ticks later in unsigned arithmetic.
    const uint32_t later = 0xFFFFFFF0u + 70;
    check(MimitaNet::shouldEmitDisagreement(state, later, 60),
          "unsigned tick difference handles wraparound");
}

} // namespace

int main()
{
    testExampleWindow();
    testFirstEventAlwaysAllowed();
    testExactBoundaryAllowed();
    testTickWraparound();

    std::printf("[disagreement-rate-limit-test] passed=%d failed=%d\n",
                gPassed, gFailed);
    return gFailed > 0 ? 1 : 0;
}
