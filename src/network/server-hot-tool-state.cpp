// 09 22 2026
/* purpose
* Implements cold-side access to the single per-instance tool state.
* See the header for scope.
*/
#include "network/server-hot-tool-state.h"

#include "ecs/dynamic-components.h"
#include "ecs/entity-registry.h"
#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"

namespace MimitaNet {

namespace {

void ensureSchema()
{
    // Always (re)register: a store clear must not leave the schema missing.
    MimitaRuntime::DynamicComponentSchema schema;
    schema.typeId = HOT_TOOL_STATE_COMPONENT;
    schema.schemaHash = gameHash("ToolInstanceState.v1");
    schema.version = HOT_TOOL_STATE_VERSION;
    schema.size = sizeof(ToolInstanceStateV1);
    schema.align = 8;
    schema.copyPolicy = GAME_COPY_RUNTIME_ONLY;
    schema.name = "ToolInstanceState";
    MimitaRuntime::DynamicComponentStore::instance().registerSchema(schema);
}

} // namespace

bool serverHotToolStateHas(std::uint64_t toolEntity)
{
    if (toolEntity == 0)
        return false;
    ensureSchema();
    return MimitaRuntime::DynamicComponentStore::instance().has(
        static_cast<EntityId>(toolEntity), HOT_TOOL_STATE_COMPONENT);
}

bool serverHotToolStateRead(std::uint64_t toolEntity, ToolInstanceStateV1& out)
{
    if (toolEntity == 0)
        return false;
    ensureSchema();
    ToolInstanceStateV1 state{};
    if (!MimitaRuntime::DynamicComponentStore::instance().read(
            static_cast<EntityId>(toolEntity), HOT_TOOL_STATE_COMPONENT, &state,
            sizeof(state)))
        return false;
    if (state.version != HOT_TOOL_STATE_VERSION)
        return false;
    out = state;
    return true;
}

bool serverHotToolStateWrite(std::uint64_t toolEntity,
                             const ToolInstanceStateV1& in)
{
    if (toolEntity == 0)
        return false;
    ensureSchema();
    return MimitaRuntime::DynamicComponentStore::instance().write(
        static_cast<EntityId>(toolEntity), HOT_TOOL_STATE_COMPONENT, &in,
        sizeof(in));
}

} // namespace MimitaNet
