// 07 31 2026, 15 30
/* purpose
* Tests the badconn simulator core: latency, loss, reorder, blackout, session guards.
* Compiles the badconn module sources directly and drives the public API.
* Keeps assertions deterministic where possible and sleeps past real delays elsewhere.
* Does NOT open sockets, contact the network, or load the game engine.
* Does NOT modify game source; only compiles test sources into build/.
*/

#include "network/badconn/badconn.h"
#include "network/game-transport.h"
#include "network/packets.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>
#include <vector>

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

void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

std::string tempPath(const char* name)
{
    return std::string("build/badconn-core-") + name;
}

void writeTempConfig(const char* name, const std::string& json)
{
    std::ofstream out(tempPath(name), std::ios::trunc);
    out << json;
}

std::vector<uint8_t> makePacket(uint8_t type)
{
    MimitaNet::PacketHeader header;
    header.type = type;
    header.tick = 1;
    std::vector<uint8_t> bytes(sizeof(header));
    std::memcpy(bytes.data(), &header, sizeof(header));
    return bytes;
}

struct FakeTransport : IGameTransport
{
    std::vector<std::vector<uint8_t>> sent;

    bool send(const void* data, size_t size) override
    {
        const uint8_t* begin = static_cast<const uint8_t*>(data);
        sent.emplace_back(begin, begin + size);
        return true;
    }

    void poll(std::vector<ReceivedPacket>& out) override
    {
        out.clear();
    }

    bool connected() const override
    {
        return true;
    }

    void close() override {}
};

const char* kLatencyConfig =
    "{\"version\":1,\"presets\":{"
    "\"1\":{\"name\":\"Latency\",\"latency\":{\"min_ms\":30,\"max_ms\":30}}}}";

const char* kLossConfig =
    "{\"version\":1,\"presets\":{"
    "\"1\":{\"name\":\"Loss\",\"loss\":{\"min_percent\":100,\"max_percent\":100}}}}";

const char* kReorderConfig =
    "{\"version\":1,\"presets\":{"
    "\"1\":{\"name\":\"Reorder\",\"reorder\":{\"min_percent\":100,\"max_percent\":100,\"window\":8}}}}";

const char* kBlackoutConfig =
    "{\"version\":1,\"presets\":{"
    "\"1\":{\"name\":\"Blackout\",\"blackout\":{\"enabled\":true,"
    "\"start_probability_per_second\":1.0,\"min_ms\":100,\"max_ms\":100,"
    "\"cooldown_ms\":0,\"direction\":\"both\",\"release_queued\":false}}}}";

void testInactivePassThrough()
{
    badconn::disable();
    FakeTransport transport;
    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    check(!badconn::processOutgoing(packet.data(), packet.size()),
          "inactive: outgoing passes through");
    badconn::tick(&transport);
    check(transport.sent.empty(), "inactive: tick sends nothing");
}

void testLatencyDelivers()
{
    writeTempConfig("latency.json", kLatencyConfig);
    badconn::loadConfig(tempPath("latency.json"));
    check(badconn::activatePreset("1"), "latency: preset activates");

    FakeTransport transport;
    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    check(badconn::processOutgoing(packet.data(), packet.size()),
          "latency: packet consumed (delayed)");
    badconn::tick(&transport);
    check(transport.sent.empty(), "latency: nothing delivered before delay");

    sleepMs(60);
    badconn::tick(&transport);
    check(transport.sent.size() == 1, "latency: packet delivered after delay");
    check(badconn::metrics().packetsDelayed >= 1, "latency: delayed counter set");

    badconn::disable();
}

void testLossDrops()
{
    writeTempConfig("loss.json", kLossConfig);
    badconn::loadConfig(tempPath("loss.json"));
    check(badconn::activatePreset("1"), "loss: preset activates");

    FakeTransport transport;
    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    check(badconn::processOutgoing(packet.data(), packet.size()),
          "loss: packet dropped (consumed)");
    badconn::tick(&transport);
    check(transport.sent.empty(), "loss: nothing delivered");
    check(badconn::metrics().packetsDropped >= 1, "loss: dropped counter set");

    badconn::disable();
}

void testExemptPacketBypasses()
{
    writeTempConfig("loss.json", kLossConfig);
    badconn::loadConfig(tempPath("loss.json"));
    badconn::activatePreset("1");

    std::vector<uint8_t> hello = makePacket(MimitaNet::PACKET_HELLO);
    check(!badconn::processOutgoing(hello.data(), hello.size()),
          "exempt: hello packet not consumed under 100% loss");

    badconn::disable();
}

void testReorderQueues()
{
    writeTempConfig("reorder.json", kReorderConfig);
    badconn::loadConfig(tempPath("reorder.json"));
    check(badconn::activatePreset("1"), "reorder: preset activates");

    FakeTransport transport;
    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    check(badconn::processOutgoing(packet.data(), packet.size()),
          "reorder: packet consumed (held)");
    check(badconn::metrics().packetsReordered >= 1, "reorder: reordered counter set");

    sleepMs(80);
    badconn::tick(&transport);
    check(transport.sent.size() == 1, "reorder: held packet delivered");

    badconn::disable();
}

void testStaleSessionDiscarded()
{
    writeTempConfig("latency.json", kLatencyConfig);
    badconn::loadConfig(tempPath("latency.json"));
    badconn::activatePreset("1");

    FakeTransport transport;
    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    check(badconn::processOutgoing(packet.data(), packet.size()),
          "stale: packet queued under old session");
    badconn::noteConnectionEstablished();

    sleepMs(60);
    badconn::tick(&transport);
    check(transport.sent.empty(), "stale: old-session packet never sent");
    check(badconn::metrics().staleDiscarded >= 1, "stale: discarded counter set");

    badconn::disable();
}

void testTeardownClearsQueues()
{
    writeTempConfig("latency.json", kLatencyConfig);
    badconn::loadConfig(tempPath("latency.json"));
    badconn::activatePreset("1");

    FakeTransport transport;
    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    badconn::processOutgoing(packet.data(), packet.size());
    badconn::noteConnectionTeardown();

    sleepMs(60);
    badconn::tick(&transport);
    check(transport.sent.empty(), "teardown: queued packets cleared");

    badconn::disable();
}

void testBlackoutStartsAndEnds()
{
    writeTempConfig("blackout.json", kBlackoutConfig);
    badconn::loadConfig(tempPath("blackout.json"));
    check(badconn::activatePreset("1"), "blackout: preset activates");

    FakeTransport transport;
    badconn::tick(&transport);
    check(badconn::metrics().blackoutActive, "blackout: active after tick");

    std::vector<uint8_t> packet = makePacket(MimitaNet::PACKET_INPUT);
    check(badconn::processOutgoing(packet.data(), packet.size()),
          "blackout: packet dropped while active");
    check(badconn::metrics().packetsDropped >= 1, "blackout: dropped counter set");

    sleepMs(150);
    badconn::tick(&transport);
    check(!badconn::metrics().blackoutActive, "blackout: ended after duration");
    check(!badconn::processOutgoing(packet.data(), packet.size()),
          "blackout: traffic resumes after end");

    badconn::disable();
}

void testIncomingLatency()
{
    writeTempConfig("latency.json", kLatencyConfig);
    badconn::loadConfig(tempPath("latency.json"));
    badconn::activatePreset("1");

    std::vector<ReceivedPacket> raw;
    ReceivedPacket rp;
    rp.bytes = makePacket(MimitaNet::PACKET_INPUT);
    raw.push_back(std::move(rp));

    badconn::processIncoming(raw);
    check(raw.empty(), "incoming: packet delayed (not delivered yet)");

    sleepMs(60);
    std::vector<ReceivedPacket> empty;
    badconn::processIncoming(empty);
    check(empty.size() == 1, "incoming: delayed packet delivered after delay");

    badconn::disable();
}

void testExemptIncomingPassThrough()
{
    writeTempConfig("latency.json", kLatencyConfig);
    badconn::loadConfig(tempPath("latency.json"));
    badconn::activatePreset("1");

    const std::vector<uint8_t> spawnActivated =
        makePacket(MimitaNet::PACKET_SPAWN_ACTIVATED);
    const size_t originalSize = spawnActivated.size();

    std::vector<ReceivedPacket> raw;
    ReceivedPacket rp;
    rp.bytes = spawnActivated;
    raw.push_back(std::move(rp));

    badconn::processIncoming(raw);

    // Exempt control packets must survive pass-through with their bytes
    // intact. Regression: processIncoming previously moved bytes into a
    // by-value classifier, emptying the pass-through packet so mpTick dropped
    // SpawnActivated and gameplay never re-enabled after respawn.
    check(raw.size() == 1, "exempt: packet survives processIncoming");
    if (raw.size() == 1)
    {
        check(raw[0].bytes.size() == originalSize,
              "exempt: packet bytes intact after processIncoming");
        check(raw[0].bytes.size() >= sizeof(MimitaNet::PacketHeader) &&
              reinterpret_cast<const MimitaNet::PacketHeader*>(raw[0].bytes.data())->type ==
                  MimitaNet::PACKET_SPAWN_ACTIVATED,
              "exempt: header type preserved");
    }

    badconn::disable();
}

} // namespace

int main()
{
    testInactivePassThrough();
    testLatencyDelivers();
    testLossDrops();
    testExemptPacketBypasses();
    testReorderQueues();
    testStaleSessionDiscarded();
    testTeardownClearsQueues();
    testBlackoutStartsAndEnds();
    testIncomingLatency();
    testExemptIncomingPassThrough();

    std::printf("[badconn-core-test] passed=%d failed=%d\n", gPassed, gFailed);
    return gFailed > 0 ? 1 : 0;
}
