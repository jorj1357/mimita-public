// 2026-10-31
#include "impact/destructible-async-cut.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "debug/structured-log.h"
#include "impact/destructible-geometry.h"
#include "impact/impact-system.h"
#include "physics/physical-entity.h"

namespace MimitaImpact {

struct DestructibleAsyncCutWorker::Impl
{
    struct Job
    {
        uint32_t networkId = 0;
        BooleanMesh baseMesh;
        std::vector<BooleanCutter> cutters;
        uint64_t cutCount = 0;
    };

    struct Done
    {
        uint32_t networkId = 0;
        uint64_t cutCount = 0;
        BooleanCutResult result;
        double workerMs = 0.0;
    };

    std::mutex mutex;
    std::condition_variable cv;
    std::unordered_map<uint32_t, Job> pending;
    std::deque<uint32_t> order;
    std::vector<Done> completed;
    bool stop = false;
    std::thread thread;
};

DestructibleAsyncCutWorker& DestructibleAsyncCutWorker::instance()
{
    static DestructibleAsyncCutWorker worker;
    return worker;
}

DestructibleAsyncCutWorker::DestructibleAsyncCutWorker()
    : mImpl(new Impl())
{
    mImpl->thread = std::thread([this] {
#ifdef _WIN32
        // Boolean replays can take tens of ms; keep them below the render thread.
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        Impl& impl = *mImpl;
        for (;;)
        {
            Impl::Job job;
            {
                std::unique_lock<std::mutex> lock(impl.mutex);
                impl.cv.wait(lock, [&impl] {
                    return impl.stop || !impl.order.empty();
                });
                if (impl.stop && impl.order.empty())
                    return;
                const uint32_t id = impl.order.front();
                impl.order.pop_front();
                const auto it = impl.pending.find(id);
                if (it == impl.pending.end())
                    continue;
                job = std::move(it->second);
                impl.pending.erase(it);
            }

            const auto start = std::chrono::steady_clock::now();
            BooleanCutResult result =
                booleanSubtractAll(job.baseMesh, job.cutters);
            const double workerMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();

            std::lock_guard<std::mutex> lock(impl.mutex);
            impl.completed.push_back(
                {job.networkId, job.cutCount, std::move(result), workerMs});
        }
    });
}

DestructibleAsyncCutWorker::~DestructibleAsyncCutWorker()
{
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        mImpl->stop = true;
    }
    mImpl->cv.notify_all();
    if (mImpl->thread.joinable())
        mImpl->thread.join();
    delete mImpl;
}

void DestructibleAsyncCutWorker::submit(uint32_t networkId,
                                        BooleanMesh baseMesh,
                                        std::vector<BooleanCutter> cutters,
                                        uint64_t cutCount)
{
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    const auto existing = mImpl->pending.find(networkId);
    if (existing != mImpl->pending.end())
    {
        existing->second.baseMesh = std::move(baseMesh);
        existing->second.cutters = std::move(cutters);
        existing->second.cutCount = cutCount;
    }
    else
    {
        mImpl->pending.emplace(networkId,
            Impl::Job{networkId, std::move(baseMesh), std::move(cutters),
                      cutCount});
        mImpl->order.push_back(networkId);
    }
    mImpl->cv.notify_one();
}

void DestructibleAsyncCutWorker::drainAndApply()
{
    std::vector<Impl::Done> completed;
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        completed.swap(mImpl->completed);
    }
    for (Impl::Done& done : completed)
    {
        PhysicalEntity* entity =
            PhysicalEntitySystem::instance().findByNetworkId(done.networkId);
        if (!entity)
            continue;
        // A newer cut arrived while this job ran; the newer job owns the
        // surface. The full cut history remains authoritative.
        if (done.cutCount != entity->destructible.cuts.size())
            continue;
        if (!done.result.success)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::World, StructuredLevel::Verbose,
                "world.destructible-async-cut", std::to_string(done.networkId),
                "async client cut rebuild failed", 0,
                {{"network_id", done.networkId},
                 {"cut_count", done.cutCount},
                 {"worker_ms", done.workerMs},
                 {"error", (int)done.result.error},
                 {"message", done.result.message}});
            continue;
        }

        const auto applyStart = std::chrono::steady_clock::now();
        DestructibleGeometrySystem::instance().applyExternalMesh(
            entity->destructible, std::move(done.result.mesh),
            done.result.remainingVolume, done.result.shellCount,
            done.result.componentCount);
        entity->localTriangles = entity->destructible.collisionTriangles;
        refreshEntityMassProperties(*entity);
        const double applyMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - applyStart).count();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::World, StructuredLevel::Verbose,
            "world.destructible-async-cut", std::to_string(done.networkId),
            "async client cut installed outside the receive frame", 0,
            {{"network_id", done.networkId},
             {"cut_count", done.cutCount},
             {"worker_ms", done.workerMs},
             {"apply_ms", applyMs},
             {"components", done.result.componentCount},
             {"remaining_volume", done.result.remainingVolume}});
    }
}

void DestructibleAsyncCutWorker::forget(uint32_t networkId)
{
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    mImpl->pending.erase(networkId);
    for (auto it = mImpl->order.begin(); it != mImpl->order.end();)
    {
        if (*it == networkId)
            it = mImpl->order.erase(it);
        else
            ++it;
    }
    mImpl->completed.erase(
        std::remove_if(mImpl->completed.begin(), mImpl->completed.end(),
            [networkId](const Impl::Done& done) {
                return done.networkId == networkId;
            }),
        mImpl->completed.end());
}

} // namespace MimitaImpact
