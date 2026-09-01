// 09 01 2026, 00 00
/* purpose
* Provide terminal commands for observing and debugging the persistence queue.
* Expose queue depth, sent count, retry count, and reset stats.
* Does NOT implement persistence logic, rewards, or database schema.
*/

#include "persistence/persistence-queue.h"
#include "persistence/persistence-level.h"

#include <cstdio>
#include <string>

void registerPersistenceCommands()
{
    printf("[PERSISTENCE] Terminal commands registered: persistence_queue, persistence_level\n");
}

void handlePersistenceCommand(const std::string& cmd)
{
    if (cmd == "persistence_queue")
    {
        auto& q = PersistenceQueue::instance();
        printf("=== PERSISTENCE QUEUE ===\n");
        printf("Queue depth:    %zu\n", q.queueDepth());
        printf("Total sent:     %llu\n", (unsigned long long)q.totalSent());
        printf("Total retries:  %llu\n", (unsigned long long)q.totalRetries());
        printf("Total dropped:  %llu\n", (unsigned long long)q.totalDuplicates());
    }
    else if (cmd == "persistence_level")
    {
        printf("=== XP LEVEL TABLE ===\n");
        const int64_t xpValues[] = {0, 100, 8100, 57600, 240100, 600600, 980100, 2500000LL, 10000000LL};
        for (int64_t xp : xpValues)
            printf("  %10lld XP -> Level %d\n", (long long)xp, calculateLevel(xp));
    }
}
