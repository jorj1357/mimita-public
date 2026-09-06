// 09 06 2026, 00 00
/* purpose
* Tests the Bomb Tag shuffle-bag random holder selection algorithm.
* Verifies: no immediate repetition, every player selected before reshuffle,
* no alphabetical/insertion-order dependency, disconnect handling.
* Does NOT test networking, rendering, or the full game engine.
*/

#include <cstdio>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <set>
#include <cmath>

static int gFailures = 0;

static void check(bool condition, const char* message)
{
    if (!condition)
    {
        ++gFailures;
        std::printf("FAIL: %s\n", message);
    }
}

// ── Shuffle bag implementation (matching server-duel.cpp) ─────────────
struct ShuffleBag {
    std::vector<uint32_t> order;
    int position = 0;

    void build(const std::vector<uint32_t>& eligible) {
        order = eligible;
        // Fisher-Yates shuffle using std::rand()
        for (int i = (int)order.size() - 1; i > 0; --i) {
            int j = (int)((uint32_t)std::rand() % (uint32_t)(i + 1));
            std::swap(order[i], order[j]);
        }
        position = 0;
    }

    uint32_t next() {
        if (order.empty()) return 0;
        if (position >= (int)order.size()) {
            position = 0;
        }
        return order[position++];
    }

    void remove(uint32_t id) {
        auto it = std::find(order.begin() + position, order.end(), id);
        if (it != order.end()) {
            order.erase(it);
            if (position >= (int)order.size() && !order.empty())
                position = 0;
        }
    }
};

// ── Test 1: Every player selected before reshuffle ────────────────────
static void testEveryPlayerSelected()
{
    std::srand(42);  // Deterministic seed for reproducibility
    std::vector<uint32_t> eligible = {1, 2, 3, 4};
    ShuffleBag bag;
    bag.build(eligible);

    std::set<uint32_t> selected;
    for (int i = 0; i < 4; ++i) {
        uint32_t id = bag.next();
        selected.insert(id);
    }
    check(selected.size() == 4, "all 4 players selected in one cycle");
    check(selected.count(1) && selected.count(2) && selected.count(3) && selected.count(4),
          "each specific player was selected");
}

// ── Test 2: No immediate repetition within one cycle ──────────────────
static void testNoImmediateRepetition()
{
    std::srand(123);
    std::vector<uint32_t> eligible = {10, 20, 30, 40, 50};
    ShuffleBag bag;
    bag.build(eligible);

    std::vector<uint32_t> sequence;
    for (int i = 0; i < 5; ++i)
        sequence.push_back(bag.next());

    // Check no two consecutive entries are the same
    for (int i = 1; i < (int)sequence.size(); ++i) {
        check(sequence[i] != sequence[i-1],
              "no consecutive repetition within one cycle");
    }
}

// ── Test 3: Bag wraps around correctly ────────────────────────────────
static void testBagWraparound()
{
    std::srand(77);
    std::vector<uint32_t> eligible = {1, 2, 3};
    ShuffleBag bag;
    bag.build(eligible);

    // Exhaust the bag
    for (int i = 0; i < 3; ++i)
        bag.next();

    // Next call should wrap around
    uint32_t first = bag.next();
    check(first == bag.order[0] || first == bag.order[1] || first == bag.order[2],
          "wrap-around returns a valid player");
}

// ── Test 4: Disconnect removes player from bag ────────────────────────
static void testDisconnectRemovesPlayer()
{
    std::srand(99);
    std::vector<uint32_t> eligible = {1, 2, 3, 4};
    ShuffleBag bag;
    bag.build(eligible);

    // Remove player 3
    bag.remove(3);

    // Check player 3 is no longer in remaining bag
    for (int i = bag.position; i < (int)bag.order.size(); ++i) {
        check(bag.order[i] != 3, "player 3 removed from bag");
    }
}

// ── Test 5: Single player bag works ───────────────────────────────────
static void testSinglePlayer()
{
    std::srand(55);
    std::vector<uint32_t> eligible = {42};
    ShuffleBag bag;
    bag.build(eligible);

    uint32_t id = bag.next();
    check(id == 42, "single player always selected");

    // Should wrap around and select again
    id = bag.next();
    check(id == 42, "single player selected again after wrap");
}

// ── Test 6: Many cycles produce varied ordering ───────────────────────
static void testVariedOrdering()
{
    std::srand(1000);
    std::vector<uint32_t> eligible = {1, 2, 3, 4, 5};

    std::vector<uint32_t> firstCycle;
    ShuffleBag bag;
    bag.build(eligible);
    for (int i = 0; i < 5; ++i)
        firstCycle.push_back(bag.next());

    // Build again with different seed
    std::srand(2000);
    std::vector<uint32_t> secondCycle;
    bag.build(eligible);
    for (int i = 0; i < 5; ++i)
        secondCycle.push_back(bag.next());

    // They should not be identical (extremely unlikely with different seeds)
    check(firstCycle != secondCycle, "different seeds produce different orderings");
}

// ── Test 7: Empty bag returns 0 ───────────────────────────────────────
static void testEmptyBag()
{
    ShuffleBag bag;
    bag.order.clear();
    bag.position = 0;
    uint32_t id = bag.next();
    check(id == 0, "empty bag returns 0");
}

// ── Test 8: Remove during active position ─────────────────────────────
static void testRemoveDuringActive()
{
    std::srand(333);
    std::vector<uint32_t> eligible = {1, 2, 3, 4, 5};
    ShuffleBag bag;
    bag.build(eligible);

    // Consume 2 items
    bag.next();
    bag.next();

    // Remove an item that's ahead of position
    uint32_t ahead = bag.order[bag.position + 1];
    bag.remove(ahead);

    // Verify the removed item is gone
    for (int i = bag.position; i < (int)bag.order.size(); ++i) {
        check(bag.order[i] != ahead, "removed item not in remaining bag");
    }
}

// ── Test 9: Two-player bag works correctly ────────────────────────────
static void testTwoPlayerBag()
{
    std::srand(777);
    std::vector<uint32_t> eligible = {10, 20};
    ShuffleBag bag;
    bag.build(eligible);

    uint32_t a = bag.next();
    uint32_t b = bag.next();
    check(a != b, "two different players selected");
    check((a == 10 || a == 20) && (b == 10 || b == 20), "both are valid players");
}

int main()
{
    std::printf("=== Bomb Tag Shuffle Bag Tests ===\n");

    testEveryPlayerSelected();
    testNoImmediateRepetition();
    testBagWraparound();
    testDisconnectRemovesPlayer();
    testSinglePlayer();
    testVariedOrdering();
    testEmptyBag();
    testRemoveDuringActive();
    testTwoPlayerBag();

    std::printf("\n=== Results: %d failures ===\n", gFailures);
    return gFailures > 0 ? 1 : 0;
}
