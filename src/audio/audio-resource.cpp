// 09 23 2026
/* purpose
* Implements AudioResourceRegistry: logical sound -> immutable generation with
* off-thread decode/validate and refcounted lifetime via shared_ptr. Publishing
* a new generation at a safe boundary never invalidates a voice that already
* holds the old generation.
*/
#include "audio/audio-resource.h"

#include "audio/audio.h"
#include "live-code/live-journal.h"

#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace {

std::uint64_t fnv1a(const std::string& s)
{
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::uint64_t fnv1aBytes(const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t h = 1469598103934665603ull;
    for (std::uint8_t b : bytes) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

bool readFileBytes(const std::string& path, std::vector<std::uint8_t>& out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;
    const std::streamsize size = file.tellg();
    if (size <= 0)
        return false;
    file.seekg(0);
    out.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(out.data()), size);
    return file.good() || file.eof();
}

void journalResource(const char* type, const std::string& name,
                     std::uint32_t generation, std::uint64_t hash,
                     const char* result, const char* error)
{
    LiveEventJournal::Fields f;
    if (generation != 0)
        f.hasGeneration = true, f.generation = generation;
    if (result && *result)
        f.result = result;
    if (error && *error)
        f.error = error;
    char extra[224] = {};
    std::snprintf(extra, sizeof(extra),
                  "\"sound\":\"%s\",\"resource_generation\":%u,\"content_hash\":%llu",
                  name.c_str(), generation, static_cast<unsigned long long>(hash));
    f.extra = extra;
    LiveEventJournal::instance().record(type, f);
}

} // namespace

struct AudioResourceRegistry::Impl {
    struct Pending {
        std::string name;
        std::vector<std::uint8_t> bytes;
        std::uint64_t hash = 0;
        bool valid = false;
        std::string error;
    };
    struct Request {
        std::uint64_t id = 0;
        std::string name;
    };

    std::mutex mutex;
    std::unordered_map<std::uint64_t, std::shared_ptr<const AudioResourceGeneration>> current;
    std::unordered_map<std::uint64_t, Pending> pending;
    std::deque<Request> requests;
    std::condition_variable cv;
    std::thread worker;
    bool stop = false;
    bool started = false;

    void ensureWorker()
    {
        if (started)
            return;
        started = true;
        worker = std::thread([this] { workerLoop(); });
    }

    void workerLoop()
    {
        for (;;) {
            Request req;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this] { return stop || !requests.empty(); });
                if (stop && requests.empty())
                    return;
                req = requests.front();
                requests.pop_front();
            }
            processReload(req);
        }
    }

    void processReload(const Request& req)
    {
        journalResource("audio.resource_decode_started", req.name, 0, 0,
                        "started", nullptr);

        Pending result;
        result.name = req.name;
        const std::string path = resolveSoundPath(req.name);
        std::vector<std::uint8_t> bytes;
        if (!readFileBytes(path, bytes) || bytes.empty()) {
            result.error = "file-missing";
        } else {
            // Real decode on the worker: validates format, channels, and length.
            std::vector<int16_t> pcm;
            std::uint32_t sampleRate = 0, channels = 0;
            if (!decodeAudioToPCM(path, pcm, sampleRate, channels, 48000, 2) ||
                pcm.empty()) {
                result.error = "decode-failed";
            } else {
                result.bytes = std::move(bytes);
                result.hash = fnv1aBytes(result.bytes);
                result.valid = true;
            }
        }

        const bool ok = result.valid;
        const std::uint64_t hash = result.hash;
        const std::string error = result.error;
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending[req.id] = std::move(result);
        }
        if (ok)
            journalResource("audio.resource_decode_finished", req.name, 0, hash,
                            "ok", nullptr);
        else
            journalResource("audio.resource_decode_failed", req.name, 0, 0,
                            "failed", error.c_str());
    }
};

AudioResourceRegistry& AudioResourceRegistry::instance()
{
    static AudioResourceRegistry registry;
    return registry;
}

AudioResourceRegistry::AudioResourceRegistry() : m_impl(new Impl())
{
    m_impl->ensureWorker();
}

AudioResourceRegistry::~AudioResourceRegistry()
{
    shutdown();
    delete m_impl;
}

void AudioResourceRegistry::shutdown()
{
    if (!m_impl || !m_impl->started)
        return;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->stop = true;
    }
    m_impl->cv.notify_all();
    if (m_impl->worker.joinable())
        m_impl->worker.join();
    m_impl->started = false;
}

std::shared_ptr<const AudioResourceGeneration>
AudioResourceRegistry::acquire(const std::string& name)
{
    if (name.empty())
        return nullptr;
    const std::uint64_t id = fnv1a(name);
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto it = m_impl->current.find(id);
    if (it != m_impl->current.end())
        return it->second;

    // Lazy generation 1: bytes only (full decode is the reload worker's job), so
    // first playback matches the legacy synchronous cache read.
    std::vector<std::uint8_t> bytes;
    if (!readFileBytes(resolveSoundPath(name), bytes) || bytes.empty())
        return nullptr;
    auto gen = std::make_shared<AudioResourceGeneration>();
    gen->logicalId = id;
    gen->generation = 1;
    gen->contentHash = fnv1aBytes(bytes);
    gen->bytes = std::move(bytes);
    gen->valid = true;
    m_impl->current[id] = gen;
    return gen;
}

bool AudioResourceRegistry::requestReload(const std::string& name)
{
    if (name.empty())
        return false;
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->requests.push_back({fnv1a(name), name});
    }
    m_impl->cv.notify_one();
    return true;
}

void AudioResourceRegistry::invalidate(const std::string& name)
{
    if (name.empty())
        return;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->current.erase(fnv1a(name));
}

void AudioResourceRegistry::update()
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    for (auto& kv : m_impl->pending) {
        const std::uint64_t id = kv.first;
        Impl::Pending& p = kv.second;
        if (!p.valid)
            continue;   // failed decode keeps the last-good generation
        auto it = m_impl->current.find(id);
        if (it != m_impl->current.end() &&
            it->second->contentHash == p.hash)
            continue;   // unchanged bytes
        auto gen = std::make_shared<AudioResourceGeneration>();
        gen->logicalId = id;
        gen->generation = it != m_impl->current.end()
                              ? it->second->generation + 1
                              : 1;
        gen->contentHash = p.hash;
        gen->bytes = std::move(p.bytes);
        gen->valid = true;
        m_impl->current[id] = gen;
        journalResource("audio.resource_generation_changed", p.name,
                        gen->generation, gen->contentHash, "changed", nullptr);
    }
    m_impl->pending.clear();
}

std::uint32_t AudioResourceRegistry::generationOf(const std::string& name) const
{
    if (name.empty() || !m_impl)
        return 0;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    const auto it = m_impl->current.find(fnv1a(name));
    return it == m_impl->current.end() ? 0 : it->second->generation;
}

std::uint32_t AudioResourceRegistry::loadedCount() const
{
    if (!m_impl)
        return 0;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return static_cast<std::uint32_t>(m_impl->current.size());
}

std::uint32_t AudioResourceRegistry::pendingCount() const
{
    if (!m_impl)
        return 0;
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return static_cast<std::uint32_t>(m_impl->pending.size());
}
