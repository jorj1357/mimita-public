// 2026-11-05
/* purpose
* Own native world-project files: manifests, immutable SHA-256 versions,
* dependency blobs, recovery drafts, and package import/export.
* The store is the only writer of project files. It does not own runtime
* gameplay state or the live edit session.
*/
#pragma once

#include <string>
#include <vector>

#include "worldproject/world-project-types.h"

namespace MimitaWorldProject {

class WorldProjectStore
{
public:
    static WorldProjectStore& instance();

    // Tests and portable installs may override the per-user root.
    void setRoot(const std::string& root) { mRootOverride = root; }
    void clearRootOverride() { mRootOverride.clear(); }
    std::string root() const;

    bool createFromMap(const std::string& projectId,
                       const std::string& ownerAccountId,
                       const std::string& title,
                       const std::string& visibility,
                       const std::string& mapJson,
                       std::string* error = nullptr);

    bool saveVersion(const std::string& projectId,
                     const std::string& authorAccountId,
                     const std::string& mapJson,
                     const std::string& title,
                     std::string* outVersion = nullptr,
                     std::string* error = nullptr);

    bool fork(const std::string& sourceProjectId,
              const std::string& sourceVersion,
              const std::string& newProjectId,
              const std::string& ownerAccountId,
              const std::string& title,
              const std::string& visibility,
              std::string* error = nullptr);

    bool readManifest(const std::string& projectId,
                      WorldProjectManifest& out,
                      std::string* error = nullptr);
    bool writeManifest(const WorldProjectManifest& manifest,
                       std::string* error = nullptr);
    bool listProjects(std::vector<WorldProjectSummary>& out,
                      std::string* error = nullptr);

    bool listVersions(const std::string& projectId,
                      std::vector<WorldProjectVersion>& out,
                      std::string* error = nullptr);
    bool readVersion(const std::string& projectId,
                     const std::string& version,
                     std::string& outMapJson,
                     WorldProjectVersion* outVersion = nullptr,
                     std::string* error = nullptr);
    bool checkout(const std::string& projectId,
                  const std::string& version,
                  std::string* error = nullptr);

    bool readWorking(const std::string& projectId,
                     std::string& outMapJson,
                     std::string* error = nullptr);
    bool writeWorking(const std::string& projectId,
                      const std::string& mapJson,
                      std::string* error = nullptr);

    bool exportPackage(const std::string& projectId,
                       const std::string& version,
                       const std::string& destination,
                       WorldProjectExportResult* result = nullptr,
                       std::string* error = nullptr);
    bool importPackage(const std::string& sourceDirectory,
                       const std::string& projectIdOverride,
                       WorldProjectImportResult* result = nullptr,
                       std::string* error = nullptr);

    bool writeDraft(const WorldProjectDraft& draft,
                    std::string* error = nullptr);
    bool readDraft(const std::string& projectId,
                   const std::string& draftId,
                   WorldProjectDraft& out,
                   std::string* error = nullptr);
    bool listDrafts(const std::string& projectId,
                    std::vector<WorldProjectDraft>& out,
                    std::string* error = nullptr);
    bool promoteDraft(const std::string& projectId,
                      const std::string& draftId,
                      const std::string& authorAccountId,
                      std::string* outVersion = nullptr,
                      std::string* error = nullptr);

    // Upgrade one format-1 project without deleting its source manifest.
    bool migrateFormat1(const std::string& projectId,
                        std::string* error = nullptr);
    bool migrateAllFormat1(std::vector<std::string>& migrated,
                           std::string* error = nullptr);

    static std::string utcNow();
    static bool validProjectId(const std::string& projectId);

private:
    WorldProjectStore() = default;

    bool ensureFormat2Impl(const std::string& projectId,
                           WorldProjectManifest& out,
                           std::string* error);

    std::string mRootOverride;
};

// Filesystem-level self-test. Uses a temporary root and restores the previous
// root before returning.
bool worldProjectStoreSelfTest(std::string& report);

} // namespace MimitaWorldProject
