#include "combat/attack-timeline.h"

#include <cstdio>

int main()
{
    AttackTimeline timeline;
    timeline.start(15, 8, 15);

    int startup = 0;
    int active = 0;
    int recovery = 0;
    int contactEnabled = 0;

    for (int tick = 0; tick < 38; ++tick)
    {
        if (timeline.phase() == AttackTimeline::Phase::Startup) ++startup;
        if (timeline.phase() == AttackTimeline::Phase::Active) {
            ++active;
            ++contactEnabled;
        }
        if (timeline.phase() == AttackTimeline::Phase::Recovery) ++recovery;
        timeline.advanceOneTick();
    }

    const bool ok = startup == 15 && active == 8 && recovery == 15 &&
                    contactEnabled == 8 && timeline.isFinished();
    std::printf("[attack-timeline-test] startup=%d active=%d recovery=%d contact=%d finished=%d\n",
                startup, active, recovery, contactEnabled,
                (int)timeline.isFinished());
    return ok ? 0 : 1;
}

