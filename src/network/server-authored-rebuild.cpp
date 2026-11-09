// 2026-11-08
#include "network/server-authored-rebuild.h"

#include <algorithm>
#include <chrono>

#include "map/map-authored-geometry.h"

namespace MimitaNet {

namespace {

double elapsedMs(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

HeadlessGLBObject* findObjectById(HeadlessWorld& world, const std::string& id)
{
    for (HeadlessGLBObject& object : world.glbObjects)
        if (object.id == id)
            return &object;
    return nullptr;
}

bool applyDestructibleSnapshot(HeadlessWorld& world,
                               const AuthoredRebuildRequest& request,
                               std::string& failure)
{
    world.destructionBaselineSolidIds = world.solidIds;
    for (const AuthoredDestructibleSnapshot& snapshot :
         request.activeSnapshots) {
        HeadlessGLBObject* object = findObjectById(world, snapshot.objectId);
        if (!object) {
            failure = "destructible-stable-id-missing";
            return false;
        }
        if (object->triangleFirst != snapshot.triangleFirst ||
            object->triangleCount != snapshot.triangleCount) {
            failure = "destructible-range-mismatch";
            return false;
        }
        for (const auto& removed : snapshot.removed) {
            if (removed.first >= world.solidIds.size() ||
                world.solidIds[removed.first] != removed.second) {
                failure = "destructible-baseline-mismatch";
                return false;
            }
        }
        world.activeDestructibleNodes[object->nodeIndex] = snapshot.entityId;
        for (const auto& removed : snapshot.removed)
            world.solidIds[removed.first] = kRemovedCollisionSolidId;
    }
    for (const auto& rejected : request.rejectedNodes) {
        HeadlessGLBObject* object = findObjectById(world, rejected.first);
        if (object)
            world.rejectedDestructibleNodes.insert(object->nodeIndex);
    }
    world.lastDestructionMatchId = request.lastDestructionMatchId;
    world.lastNpcDestructionRevision = 0;
    return true;
}

} // namespace

struct AuthoredWorldRebuildWorker::Impl
{
    std::thread thread;
    mutable std::mutex mutex;
    std::condition_variable cv;
    AuthoredRebuildRequest pending;
    bool hasPending = false;
    bool stop = false;
    std::shared_ptr<AuthoredRebuildResult> result;
    std::vector<std::shared_ptr<AuthoredRebuildResult>> retired;

    void run()
    {
        for (;;) {
            AuthoredRebuildRequest request;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [this] { return stop || hasPending; });
                retired.clear();
                if (stop && !hasPending)
                    return;
                request = std::move(pending);
                pending = AuthoredRebuildRequest{};
                hasPending = false;
            }
            auto out = std::make_shared<AuthoredRebuildResult>();
            out->input = request;
            const auto start = std::chrono::steady_clock::now();
            HeadlessWorld candidate = request.cacheHit && request.cachedWorld
                ? *request.cachedWorld : *request.baseWorld;
            double authoredMs = 0.0;
            if (!request.cacheHit) {
                const auto authoredStart = std::chrono::steady_clock::now();
                if (!applyAuthoredMapGeometry(candidate, request.mapId)) {
                    out->failure = "authored-apply-failed";
                    std::lock_guard<std::mutex> lock(mutex);
                    result = std::move(out);
                    continue;
                }
                authoredMs = elapsedMs(authoredStart);
            }
            out->authoredWorld = candidate;
            if (!applyDestructibleSnapshot(candidate, request, out->failure)) {
                std::lock_guard<std::mutex> lock(mutex);
                result = std::move(out);
                continue;
            }
            out->liveWorld = std::move(candidate);

            out->npcWorld = std::make_unique<World>();
            const auto npcStart = std::chrono::steady_clock::now();
            if (!buildNpcWorldCollisionDetached(
                    *out->npcWorld, out->liveWorld,
                    request.destructibleSurfaceTriangles,
                    request.recastWanted, request.recastProfile,
                    &out->navMesh)) {
                out->failure = "npc-world-build-failed";
                std::lock_guard<std::mutex> lock(mutex);
                result = std::move(out);
                continue;
            }
            out->npcMs = elapsedMs(npcStart);
            out->authoredMs = authoredMs;
            out->workerMs = elapsedMs(start);
            out->ok = true;
            {
                std::lock_guard<std::mutex> lock(mutex);
                result = std::move(out);
            }
        }
    }
};

AuthoredWorldRebuildWorker& AuthoredWorldRebuildWorker::instance()
{
    static AuthoredWorldRebuildWorker worker;
    return worker;
}

AuthoredWorldRebuildWorker::AuthoredWorldRebuildWorker()
    : mImpl(std::make_unique<Impl>())
{
    mImpl->thread = std::thread([this] { mImpl->run(); });
}

AuthoredWorldRebuildWorker::~AuthoredWorldRebuildWorker()
{
    shutdown();
}

void AuthoredWorldRebuildWorker::submit(AuthoredRebuildRequest request)
{
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        mImpl->pending = std::move(request);
        mImpl->hasPending = true;
    }
    mImpl->cv.notify_one();
}

std::shared_ptr<AuthoredRebuildResult>
AuthoredWorldRebuildWorker::tryTakeResult()
{
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    return std::move(mImpl->result);
}

void AuthoredWorldRebuildWorker::retire(
    std::shared_ptr<AuthoredRebuildResult> result)
{
    if (!result)
        return;
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        mImpl->retired.push_back(std::move(result));
    }
    mImpl->cv.notify_one();
}

bool AuthoredWorldRebuildWorker::busy() const
{
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    return mImpl->hasPending || mImpl->result != nullptr;
}

void AuthoredWorldRebuildWorker::cancelAndWait()
{
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        mImpl->pending = AuthoredRebuildRequest{};
        mImpl->hasPending = false;
        mImpl->result.reset();
    }
}

void AuthoredWorldRebuildWorker::shutdown()
{
    if (!mImpl || !mImpl->thread.joinable())
        return;
    {
        std::lock_guard<std::mutex> lock(mImpl->mutex);
        mImpl->stop = true;
    }
    mImpl->cv.notify_one();
    mImpl->thread.join();
}

} // namespace MimitaNet
