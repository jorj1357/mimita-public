// 2026-11-05
/* purpose
* Define the portable world-project manifest, version, dependency, export, and
* recovery-draft data model.
* This file DOES NOT own storage, networking, signing, or gameplay state.
*/
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MimitaWorldProject {

constexpr int kWorldProjectFormat = 2;
constexpr int kLegacyWorldProjectFormat = 1;

struct WorldProjectParent
{
    bool present = false;
    std::string projectId;
    std::string version;
};

struct WorldProjectDependencyLicense
{
    std::string dependencyId;
    std::string license;
    std::string attribution;
    std::string sourceVersion;
};

struct WorldProjectLicense
{
    std::string codeDefault = "MIT";
    std::string mediaDefault = "CC-BY-4.0";
    std::string status = "creator-confirmation-required";
    std::vector<WorldProjectDependencyLicense> dependencies;
};

struct WorldProjectDependency
{
    std::string id;
    std::string kind;
    std::string blobSha256;
    std::string license;
    std::string attribution;
    std::string sourceVersion;
};

struct WorldProjectManifest
{
    int format = kWorldProjectFormat;
    std::string projectId;
    std::string ownerAccountId;
    std::string title;
    std::string visibility = "public";
    WorldProjectLicense license;
    WorldProjectParent parent;
    std::string head;
    std::string createdUtc;
    std::string updatedUtc;
};

struct WorldProjectVersion
{
    std::string version;
    std::string parent;
    std::string mapSha256;
    std::string treeSha256;
    std::string createdUtc;
    std::string authorAccountId;
    int mapFormat = kWorldProjectFormat;
    std::string generatorVersion = "1";
    std::vector<WorldProjectDependency> dependencies;
};

struct WorldProjectSummary
{
    WorldProjectManifest manifest;
    size_t versionCount = 0;
    std::string directory;
};

struct WorldProjectDraft
{
    std::string draftId;
    std::string projectId;
    std::string sessionId;
    std::string authorAccountId;
    std::string baseVersion;
    std::string stateJson;
    std::string createdUtc;
    std::vector<std::string> pendingOperations;
};

struct WorldProjectExportResult
{
    std::string directory;
    std::string version;
    std::string manifestSha256;
    std::string mapSha256;
    size_t blobCount = 0;
};

struct WorldProjectImportResult
{
    std::string projectId;
    std::string version;
    std::string directory;
};

} // namespace MimitaWorldProject
