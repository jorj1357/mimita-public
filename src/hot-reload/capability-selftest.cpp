// 09 14 2026
/* purpose
* Implements the capability-provider falsification selftest.
* Proves: brand-new ids resolve, providers are generation-scoped and replaceable,
* signature mismatch and missing providers fail activation while the last-good
* generation stays active, duplicates are rejected, and kernel primitives use the
* exact same mechanism.
* Does NOT own gameplay state.
*/
#include "hot-reload/capability-selftest.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"

namespace {

bool gPass = true;

void check(bool condition, const char* what, std::string& report)
{
    if (!condition)
        gPass = false;
    report += condition ? "  [ok] " : "  [FAIL] ";
    report += what;
    report += "\n";
}

using LaunchFn = std::uint32_t (MIMITA_GAME_CALL *)(void*, std::uint32_t);

std::uint32_t MIMITA_GAME_CALL providerA(void*, std::uint32_t input) { return input + 100u; }
std::uint32_t MIMITA_GAME_CALL providerB(void*, std::uint32_t input) { return input + 200u; }
std::uint32_t MIMITA_GAME_CALL kernelEcho(void*, std::uint32_t input) { return input; }

const std::uint64_t kBanana = gameHash("banana.launch");
const std::uint64_t kSigV1 = gameHash("sig.banana.launch.v1");
const std::uint64_t kSigV2 = gameHash("sig.banana.launch.v2");
const std::uint64_t kKernelEcho = gameHash("selftest.kernel.echo");

GamePackageDescriptorV1 makePackage(const GameCapabilityDescriptorV1* providers,
                                    std::uint32_t providerCount,
                                    const GameCapabilityRequirementV1* requirements,
                                    std::uint32_t requirementCount,
                                    std::uint64_t packageId)
{
    GamePackageDescriptorV1 p{};
    p.structSize = sizeof(GamePackageDescriptorV1);
    p.abiVersion = MIMITA_PACKAGE_ABI_VERSION;
    p.packageId = packageId;
    p.logicalHash = packageId;
    p.name = "capability.selftest";
    p.capabilityProviders = providers;
    p.capabilityProviderCount = providerCount;
    p.capabilityRequirements = requirements;
    p.capabilityRequirementCount = requirementCount;
    return p;
}

} // namespace

bool runCapabilitySelfTest(std::string& report)
{
    gPass = true;
    using MimitaRuntime::GenericRuntime;
    GenericRuntime& rt = GenericRuntime::instance();

    // Kernel primitives are ordinary registry entries.
    rt.registerKernelCapability(kKernelEcho, gameHash("sig.selftest.kernel.v1"), 0,
                                reinterpret_cast<void*>(&kernelEcho), "selftest.kernel.echo");
    check(rt.kernelProvidesCapability(kKernelEcho), "kernel capability registered", report);

    // P1: brand-new id `banana.launch` provided by a package; a requirement with a
    // matching signature resolves. No EXE knowledge of the id.
    const GameCapabilityDescriptorV1 p1Providers[] = {
        {kBanana, kSigV1, 0, reinterpret_cast<void*>(&providerA), "banana.provider.a"}};
    const GameCapabilityRequirementV1 p1Requirements[] = {{kBanana, kSigV1, 0}};
    GamePackageDescriptorV1 p1 = makePackage(p1Providers, 1, p1Requirements, 1,
                                             gameHash("selftest.pkg1"));
    std::string error;
    check(rt.activate(&p1, error, 41), "P1 activates (new id provided + required)", report);
    check(reinterpret_cast<LaunchFn>(rt.capability(kBanana)) == &providerA,
          "P1 resolve returns provider A", report);
    check(rt.capabilityProviderGeneration(kBanana) == 41,
          "P1 provider generation = 41", report);
    check(reinterpret_cast<LaunchFn>(rt.capability(kBanana))(nullptr, 1) == 101u,
          "P1 provider callable works", report);

    // P2: requirement present, provider missing -> activation fails, last-good stays.
    GamePackageDescriptorV1 p2 = makePackage(nullptr, 0, p1Requirements, 1,
                                             gameHash("selftest.pkg2"));
    error.clear();
    check(!rt.activate(&p2, error, 42), "P2 missing provider fails activation", report);
    check(error.find("missing capability provider") != std::string::npos,
          "P2 failure reason is missing-provider", report);
    check(reinterpret_cast<LaunchFn>(rt.capability(kBanana)) == &providerA,
          "P2 failure keeps provider A active (rollback)", report);
    check(rt.capabilityProviderGeneration(kBanana) == 41,
          "P2 failure keeps generation 41 active", report);

    // P3: provider present but signature incompatible -> activation fails.
    const GameCapabilityDescriptorV1 p3Providers[] = {
        {kBanana, kSigV2, 0, reinterpret_cast<void*>(&providerB), "banana.provider.b"}};
    GamePackageDescriptorV1 p3 = makePackage(p3Providers, 1, p1Requirements, 1,
                                             gameHash("selftest.pkg3"));
    error.clear();
    check(!rt.activate(&p3, error, 43), "P3 signature mismatch fails activation", report);
    check(error.find("signature mismatch") != std::string::npos,
          "P3 failure reason is signature-mismatch", report);
    check(reinterpret_cast<LaunchFn>(rt.capability(kBanana)) == &providerA,
          "P3 failure keeps provider A active", report);

    // P4: replacement provider with the matching signature -> activates and the
    // resolved callable changes (live replacement semantics).
    const GameCapabilityDescriptorV1 p4Providers[] = {
        {kBanana, kSigV1, 0, reinterpret_cast<void*>(&providerB), "banana.provider.b"}};
    GamePackageDescriptorV1 p4 = makePackage(p4Providers, 1, p1Requirements, 1,
                                             gameHash("selftest.pkg4"));
    error.clear();
    check(rt.activate(&p4, error, 44), "P4 replacement activates", report);
    check(reinterpret_cast<LaunchFn>(rt.capability(kBanana)) == &providerB,
          "P4 resolve returns provider B", report);
    check(rt.capabilityProviderGeneration(kBanana) == 44,
          "P4 provider generation = 44", report);
    check(reinterpret_cast<LaunchFn>(rt.capability(kBanana))(nullptr, 1) == 201u,
          "P4 provider callable works", report);

    // P5: duplicate provider ids in one package -> rejected.
    const GameCapabilityDescriptorV1 p5Providers[] = {
        {kBanana, kSigV1, 0, reinterpret_cast<void*>(&providerA), "dup.a"},
        {kBanana, kSigV1, 0, reinterpret_cast<void*>(&providerB), "dup.b"}};
    GamePackageDescriptorV1 p5 = makePackage(p5Providers, 2, nullptr, 0,
                                             gameHash("selftest.pkg5"));
    error.clear();
    check(!rt.activate(&p5, error, 45), "P5 duplicate providers fail activation", report);
    check(error.find("duplicate capability provider") != std::string::npos,
          "P5 failure reason is duplicate-provider", report);

    // P6: a kernel capability marked overridable may be replaced by a package
    // provider, while the kernel entry stays resolvable so every caller keeps
    // one stable ABI id. Non-overridable kernel ids are never overridden.
    const std::uint64_t kOverridable = gameHash("selftest.overridable");
    rt.registerKernelCapability(kOverridable, gameHash("sig.selftest.override.v1"), 0,
                                reinterpret_cast<void*>(&kernelEcho),
                                "selftest.overridable", /*overridable=*/true);
    check(rt.kernelCapabilityOverridable(kOverridable),
          "P6 kernel capability marked overridable", report);
    check(rt.overrideCapability(kOverridable) == nullptr,
          "P6 no override before a provider registers", report);

    const GameCapabilityDescriptorV1 p6Providers[] = {
        {kOverridable, gameHash("sig.selftest.override.v1"), 0,
         reinterpret_cast<void*>(&providerA), "override.provider.a"}};
    GamePackageDescriptorV1 p6 = makePackage(p6Providers, 1, nullptr, 0,
                                             gameHash("selftest.pkg6"));
    error.clear();
    check(rt.activate(&p6, error, 46), "P6 override provider activates", report);
    check(rt.overrideCapability(kOverridable) == reinterpret_cast<void*>(&providerA),
          "P6 override resolves to the package provider", report);
    check(rt.capability(kOverridable) == reinterpret_cast<void*>(&kernelEcho),
          "P6 kernel bridge entry stays stable for callers", report);
    check(rt.overrideProviderGeneration(kOverridable) == 46,
          "P6 override provider generation = 46", report);
    check(rt.overrideCapability(kKernelEcho) == nullptr,
          "P6 non-overridable kernel id has no override", report);

    // Deactivate: package providers retire; kernel primitives survive.
    rt.deactivate();
    check(rt.capability(kBanana) == nullptr, "deactivate retires package providers", report);
    check(rt.kernelProvidesCapability(kKernelEcho) &&
              reinterpret_cast<LaunchFn>(rt.capability(kKernelEcho))(nullptr, 7) == 7u,
          "kernel capability survives package deactivation", report);
    check(rt.overrideCapability(kOverridable) == nullptr &&
              rt.capability(kOverridable) == reinterpret_cast<void*>(&kernelEcho),
          "P6 deactivate retires override, kernel fallback survives", report);

    report += gPass ? "[CAPABILITY SELFTEST] PASS\n" : "[CAPABILITY SELFTEST] FAIL\n";
    return gPass;
}
