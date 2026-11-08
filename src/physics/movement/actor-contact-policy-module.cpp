// 2026-11-07
/* purpose
* live collision policy for actor seam/contact response and streamed-surface
* merge selection
* this file DOES NOT include World, RecoveryContact, the actor solver, or STL
* types; the host owns contact gathering, correction application, and the
* static union/face-removal execution
*/
#include "modules/api/actor-contact-policy-api.h"

namespace mimita_collision_policy_module {

using mimita_collision_policy::ContactPolicyV1;
using mimita_collision_policy::SurfacePolicyV1;

void fillContactPolicy(ContactPolicyV1* policy)
{
    if (!policy)
        return;
    policy->version = mimita_collision_policy::ABI_VERSION;
    policy->structSize = sizeof(ContactPolicyV1);
    policy->enabled = 1;
    policy->removeTouchingFaceSeams = 1;
    policy->collapseFeatureContacts = 1;
    policy->suppressLowFlushSeams = 1;
    policy->collapseSamePartFeatures = 1;
    policy->opposingFaceSeamDistance = 0.4f;
    policy->opposingFaceNormalDot = -0.98f;
    policy->featureCollapseDistance = 0.1f;
    policy->flushSeamHorizontalDistance = 0.2f;
    policy->flushWallTopHeight = 0.2f;
    policy->flushSupportZTolerance = 0.2f;
    policy->flushSupportHorizontalDistance = 1.2f;
    policy->result = 1;
}

void fillSurfacePolicy(SurfacePolicyV1* policy)
{
    if (!policy)
        return;
    policy->version = mimita_collision_policy::ABI_VERSION;
    policy->structSize = sizeof(SurfacePolicyV1);
    // Face removal is the always-on seam pass. Bounded union is opt-in because
    // Manifold cost on a moving streamed set is not acceptable per tick.
    policy->mergeMode = mimita_collision_policy::SURFACE_FACE_REMOVAL;
    policy->contactTolerance = 0.02f;
    policy->result = 1;
}

} // namespace mimita_collision_policy_module
