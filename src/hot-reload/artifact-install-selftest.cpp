// 09 15 2026
/* purpose
* Proves the remote-artifact install seam with REAL bytes: the actual hot
* package DLL (the same file a local build stages) is read, hashed, and installed
* through HotReloadSystem::installCandidateArtifact, which loads it as a real
* inactive candidate via the SAME loader path used by a local build. A tampered
* artifact is rejected before any load.
*/
#include "hot-reload/artifact-install-selftest.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "hot-reload/hot-reload-system.h"
#include "hot-reload/artifact-cache.h"

namespace {

bool check(bool condition, const std::string& name, std::string& report)
{
    report += std::string(condition ? "[ok] " : "[FAIL] ") + name + "\n";
    return condition;
}

std::vector<unsigned char> readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)),
                                      std::istreambuf_iterator<char>());
}

} // namespace

bool runArtifactInstallSelfTest(std::string& report)
{
    bool ok = true;

    // Load the real active generation so we have a live loader to install into.
    HotReloadSystem::instance().startup();
    const std::uint32_t activeGen =
        HotReloadSystem::instance().status().activeGeneration;

    const std::vector<unsigned char> bytes =
        readFile("build/mimita-game.dll");
    if (bytes.empty()) {
        report += "[info] build/mimita-game.dll not present; install seam needs a "
                  "built hot package\n";
        return true;  // environment without a built hot DLL
    }
    const std::uint64_t hash =
        MimitaRuntime::hashArtifactBytes(bytes.data(), bytes.size());
    report += "  [info] artifact bytes=" + std::to_string(bytes.size()) + "\n";

    // The remote install enters the SAME candidate path as a local build.
    std::string err;
    const std::uint32_t targetGen = activeGen == 0 ? 1 : activeGen + 1;
    const bool installed = HotReloadSystem::instance().installCandidateArtifact(
        bytes, targetGen, hash, err);
    ok &= check(installed && err.empty() &&
                    HotReloadSystem::instance().hasInstalledCandidate() &&
                    HotReloadSystem::instance().installedCandidateGeneration() ==
                        targetGen,
                "real artifact installs as an inactive candidate via the loader",
                report);

    // Tampered bytes must be rejected by the hash gate before any load.
    {
        std::vector<unsigned char> tampered = bytes;
        tampered[tampered.size() / 2] ^= 0xFF;
        const bool rejected = HotReloadSystem::instance().installCandidateArtifact(
            tampered, targetGen + 1, hash, err);
        ok &= check(!rejected && !err.empty(),
                    "tampered artifact rejected before load", report);
    }

    return ok;
}
