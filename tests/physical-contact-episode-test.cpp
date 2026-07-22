// 07 21 2026, 21 45
/* purpose
* Tests generic physical-contact episode batching and final confirmation decisions.
* Verifies active, pending, batch-size, and ending-contact cases without server sockets.
* Protects cumulative confirmation behavior shared by Godball and Swordsword.
* Does NOT apply health damage, mutate ServerPlayer, send packets, or simulate movement.
* Does NOT test hitscan, projectile, or old melee packet handlers.
* Does NOT depend on rendering or local weapon presentation state.
*/

#include "combat/weapon-execution.h"

#include <cstdio>

static int gFailures = 0;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

int main()
{
    WeaponExecution::PhysicalContactEpisode episode;
    check(!WeaponExecution::episodeShouldConfirm(episode, false, 4),
          "inactive episode does not confirm");
    episode.active = true;
    check(!WeaponExecution::episodeShouldConfirm(episode, true, 4),
          "active episode with no pending damage does not confirm");
    episode.pendingConfirmationDamage = 10;
    episode.samplesSinceConfirmation = 3;
    check(!WeaponExecution::episodeShouldConfirm(episode, false, 4),
          "batch waits for configured sample count");
    episode.samplesSinceConfirmation = 4;
    check(WeaponExecution::episodeShouldConfirm(episode, false, 4),
          "batch confirms at configured sample count");
    episode.samplesSinceConfirmation = 1;
    check(WeaponExecution::episodeShouldConfirm(episode, true, 4),
          "final contact flush confirms pending damage");

    if (gFailures)
    {
        std::printf("[physical-contact-episode-test] FAIL failures=%d\n", gFailures);
        return 1;
    }
    std::printf("[physical-contact-episode-test] PASS\n");
    return 0;
}
