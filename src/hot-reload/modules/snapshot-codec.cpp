// 09 23 2026
/* purpose
* Hot snapshot-codec module. Registers the generic `net.snapshot-codecs`
* capability and serves the shared chunk/quantize/reassemble implementation from
* `hot-reload/hot-snapshot-codec.h`. Editing that header (or this file) changes
* snapshot serialization live: the cold dispatcher in `network/snapshot-chunks.cpp`
* prefers this provider over its compiled fallback, with no EXE call site.
* Does NOT own transport, chunk buffers, or snapshot storage.
*/
#if defined(MIMITA_GAME_DLL)

#include <cstring>

#include "hot-reload/hot-package.h"
#include "hot-reload/hot-snapshot-codec.h"

namespace {

using namespace MimitaNet;

void MIMITA_GAME_CALL snapshotBuild(void* /*host*/, GameSnapshotBuildV1* request)
{
    if (!request)
        return;
    char error[64] = {};
    std::uint32_t chunkCount = 0;
    const bool ok = HotSnapshotCodecImpl::build(
        request->entities, request->entityCount, request->serverTick,
        request->ownerPlayerId, request->logicalGenerationId, request->outChunks,
        request->maxChunks, &chunkCount, error);
    request->outChunkCount = chunkCount;
    request->result = ok ? 1u : 0u;
    HotSnapshotCodecImpl::setError(request->error, ok ? nullptr : error);
}

void MIMITA_GAME_CALL snapshotParse(void* /*host*/, GameSnapshotParseV1* request)
{
    if (!request || !request->out) {
        if (request)
            request->result = 0u;
        return;
    }
    char error[64] = {};
    const bool ok = HotSnapshotCodecImpl::parse(
        request->data, request->bytes, *request->out, error);
    request->result = ok ? 1u : 0u;
    HotSnapshotCodecImpl::setError(request->error, ok ? nullptr : error);
}

void MIMITA_GAME_CALL snapshotReassemble(void* /*host*/, GameSnapshotReassembleV1* request)
{
    if (!request)
        return;
    char error[64] = {};
    std::uint32_t entityCount = 0;
    std::uint32_t logicalGenerationId = 0;
    const bool ok = HotSnapshotCodecImpl::reassemble(
        request->chunks, request->chunkCount, request->outEntities,
        request->maxEntities, &entityCount, &logicalGenerationId, error);
    request->outEntityCount = entityCount;
    request->outLogicalGenerationId = logicalGenerationId;
    request->result = ok ? 1u : 0u;
    HotSnapshotCodecImpl::setError(request->error, ok ? nullptr : error);
}

const GameSnapshotCodecV1 kSnapshotCodec{
    sizeof(GameSnapshotCodecV1), 1, &snapshotBuild, &snapshotParse,
    &snapshotReassemble, "net.snapshot-codecs"};

// The one generic lookup doorway the cold dispatcher resolves by capability id.
const GameSnapshotCodecV1* MIMITA_GAME_CALL lookupSnapshotCodec(void* /*host*/)
{
    return &kSnapshotCodec;
}

const GameCapabilityDescriptorV1 kSnapshotCodecProvider{
    GAME_CAP_SNAPSHOT_CODECS, GAME_SIG_SNAPSHOT_CODECS, 0,
    reinterpret_cast<void*>(&lookupSnapshotCodec), "net.snapshot-codecs"};

} // namespace

const MimitaHotPackage::CapabilityRegistrar s_snapshotCodecProviderRegistrar{
    kSnapshotCodecProvider};
const MimitaHotPackage::CapabilityRequirementRegistrar s_snapshotCodecRequirement{
    GAME_CAP_SNAPSHOT_CODECS, GAME_SIG_SNAPSHOT_CODECS, 0};

#endif // MIMITA_GAME_DLL
