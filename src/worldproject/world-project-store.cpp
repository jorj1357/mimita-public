// 2026-11-05
#include "worldproject/world-project-store.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "utils/json-comments.h"
#include "worldproject/world-project-sha256.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace MimitaWorldProject {
namespace {

constexpr const char* kManifestName = "project.json";
constexpr const char* kLegacyManifestCopy = "project-format1.json";
constexpr const char* kWorkingDir = "working";
constexpr const char* kVersionsDir = "versions";
constexpr const char* kDraftsDir = "drafts";
constexpr const char* kBlobsDir = "blobs/sha256";

std::string timestampForName()
{
    const auto now = std::chrono::system_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    return std::to_string(micros);
}

bool fail(std::string* error, const std::string& message)
{
    if (error) *error = message;
    return false;
}

json parseObject(const std::string& text, std::string* error)
{
    try {
        std::istringstream input(text);
        json value = parseJsonConfig(input);
        if (!value.is_object()) {
            if (error) *error = "JSON root must be an object";
            return json();
        }
        return value;
    } catch (const std::exception& e) {
        if (error) *error = std::string("invalid JSON: ") + e.what();
        return json();
    }
}

std::string canonicalJson(const json& value)
{
    return value.dump() + "\n";
}

bool writeAtomic(const fs::path& path, const std::string& bytes, std::string* error)
{
    std::error_code ec;
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path(), ec);
    if (ec)
        return fail(error, "cannot create directory: " + ec.message());

    const fs::path temp = path.parent_path() /
        (".write-" + timestampForName() + "-" + path.filename().string());
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.is_open())
            return fail(error, "cannot write temporary file: " + temp.string());
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out.good())
            return fail(error, "write failed: " + temp.string());
    }
    fs::remove(path, ec);
    ec.clear();
    fs::rename(temp, path, ec);
    if (ec) {
        std::error_code cleanup;
        fs::remove(temp, cleanup);
        return fail(error, "cannot replace file: " + ec.message());
    }
    return true;
}

bool readText(const fs::path& path, std::string& out, std::string* error)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return fail(error, "cannot read file: " + path.string());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    if (!in.good() && !in.eof())
        return fail(error, "read failed: " + path.string());
    out = buffer.str();
    return true;
}

std::string dataRoot()
{
    if (const char* overrideRoot = std::getenv("MIMITA_WORLD_PROJECTS"))
        if (overrideRoot && *overrideRoot)
            return overrideRoot;
    if (const char* local = std::getenv("LOCALAPPDATA"))
        if (local && *local)
            return (fs::path(local) / "Mimita" / "WorldProjects").string();
    if (const char* roaming = std::getenv("APPDATA"))
        if (roaming && *roaming)
            return (fs::path(roaming) / "Mimita" / "WorldProjects").string();
    if (const char* home = std::getenv("HOME"))
        if (home && *home)
            return (fs::path(home) / ".local" / "share" / "Mimita" /
                    "WorldProjects").string();
    return (fs::current_path() / "WorldProjects").string();
}

json licenseToJson(const WorldProjectLicense& license)
{
    json dependencies = json::array();
    for (const auto& dep : license.dependencies) {
        dependencies.push_back({
            {"dependency_id", dep.dependencyId},
            {"license", dep.license},
            {"attribution", dep.attribution},
            {"source_version", dep.sourceVersion},
        });
    }
    return json{
        {"code_default", license.codeDefault},
        {"media_default", license.mediaDefault},
        {"status", license.status},
        {"dependency_licenses", dependencies},
    };
}

WorldProjectLicense licenseFromJson(const json& value)
{
    WorldProjectLicense license;
    if (!value.is_object()) return license;
    license.codeDefault = value.value("code_default", license.codeDefault);
    license.mediaDefault = value.value("media_default", license.mediaDefault);
    license.status = value.value("status", license.status);
    if (value.contains("dependency_licenses") && value["dependency_licenses"].is_array()) {
        for (const auto& item : value["dependency_licenses"]) {
            if (!item.is_object()) continue;
            WorldProjectDependencyLicense dep;
            dep.dependencyId = item.value("dependency_id", "");
            dep.license = item.value("license", "");
            dep.attribution = item.value("attribution", "");
            dep.sourceVersion = item.value("source_version", "");
            license.dependencies.push_back(std::move(dep));
        }
    }
    return license;
}

json dependencyToJson(const WorldProjectDependency& dep)
{
    return json{
        {"id", dep.id},
        {"kind", dep.kind},
        {"blob_sha256", dep.blobSha256},
        {"license", dep.license},
        {"attribution", dep.attribution},
        {"source_version", dep.sourceVersion},
    };
}

WorldProjectDependency dependencyFromJson(const json& item)
{
    WorldProjectDependency dep;
    if (!item.is_object()) return dep;
    dep.id = item.value("id", "");
    dep.kind = item.value("kind", "");
    dep.blobSha256 = item.value("blob_sha256", "");
    dep.license = item.value("license", "");
    dep.attribution = item.value("attribution", "");
    dep.sourceVersion = item.value("source_version", "");
    return dep;
}

json manifestToJson(const WorldProjectManifest& manifest)
{
    json parent = nullptr;
    if (manifest.parent.present) {
        parent = json{{"project_id", manifest.parent.projectId},
                      {"version", manifest.parent.version}};
    }
    return json{
        {"format", manifest.format},
        {"project_id", manifest.projectId},
        {"owner_account_id", manifest.ownerAccountId},
        {"title", manifest.title},
        {"visibility", manifest.visibility},
        {"license", licenseToJson(manifest.license)},
        {"parent", parent},
        {"head", manifest.head},
        {"created_utc", manifest.createdUtc},
        {"updated_utc", manifest.updatedUtc},
    };
}

WorldProjectManifest manifestFromJson(const json& root)
{
    WorldProjectManifest manifest;
    manifest.format = root.value("format", kLegacyWorldProjectFormat);
    manifest.projectId = root.value("project_id", "");
    manifest.ownerAccountId = root.value("owner_account_id", "");
    manifest.title = root.value("title", manifest.projectId);
    manifest.visibility = root.value("visibility", "public");
    if (root.contains("license"))
        manifest.license = licenseFromJson(root["license"]);
    if (root.contains("parent") && root["parent"].is_object()) {
        manifest.parent.present = true;
        manifest.parent.projectId = root["parent"].value("project_id", "");
        manifest.parent.version = root["parent"].value("version", "");
    }
    manifest.head = root.value("head", "");
    manifest.createdUtc = root.value("created_utc", "");
    manifest.updatedUtc = root.value("updated_utc", "");
    return manifest;
}

json versionToJson(const WorldProjectVersion& version)
{
    json deps = json::array();
    for (const auto& dep : version.dependencies)
        deps.push_back(dependencyToJson(dep));
    return json{
        {"version", version.version},
        {"parent", version.parent},
        {"map_sha256", version.mapSha256},
        {"tree_sha256", version.treeSha256},
        {"created_utc", version.createdUtc},
        {"author_account_id", version.authorAccountId},
        {"map_format", version.mapFormat},
        {"generator_version", version.generatorVersion},
        {"dependencies", deps},
    };
}

WorldProjectVersion versionFromJson(const json& root)
{
    WorldProjectVersion version;
    version.version = root.value("version", "");
    if (root.contains("parent") && root["parent"].is_string())
        version.parent = root["parent"].get<std::string>();
    version.mapSha256 = root.value("map_sha256", "");
    version.treeSha256 = root.value("tree_sha256", "");
    version.createdUtc = root.value("created_utc", "");
    version.authorAccountId = root.value("author_account_id", "");
    version.mapFormat = root.value("map_format", kWorldProjectFormat);
    version.generatorVersion = root.value("generator_version", "1");
    if (root.contains("dependencies") && root["dependencies"].is_array()) {
        for (const auto& item : root["dependencies"])
            version.dependencies.push_back(dependencyFromJson(item));
    }
    return version;
}

json draftToJson(const WorldProjectDraft& draft)
{
    return json{
        {"draft_id", draft.draftId},
        {"project_id", draft.projectId},
        {"session_id", draft.sessionId},
        {"author_account_id", draft.authorAccountId},
        {"base_version", draft.baseVersion},
        {"state_json", draft.stateJson},
        {"created_utc", draft.createdUtc},
        {"pending_operations", draft.pendingOperations},
    };
}

WorldProjectDraft draftFromJson(const json& root)
{
    WorldProjectDraft draft;
    draft.draftId = root.value("draft_id", "");
    draft.projectId = root.value("project_id", "");
    draft.sessionId = root.value("session_id", "");
    draft.authorAccountId = root.value("author_account_id", "");
    draft.baseVersion = root.value("base_version", "");
    draft.stateJson = root.value("state_json", "");
    draft.createdUtc = root.value("created_utc", "");
    if (root.contains("pending_operations") && root["pending_operations"].is_array()) {
        for (const auto& item : root["pending_operations"])
            if (item.is_string())
                draft.pendingOperations.push_back(item.get<std::string>());
    }
    return draft;
}

bool safeRelativePath(const std::string& value, fs::path& out)
{
    if (value.empty()) return false;
    const fs::path path(value);
    if (path.is_absolute()) return false;
    for (const auto& part : path) {
        if (part == "..") return false;
    }
    out = path;
    return true;
}

} // namespace

WorldProjectStore& WorldProjectStore::instance()
{
    static WorldProjectStore store;
    return store;
}

std::string WorldProjectStore::root() const
{
    if (!mRootOverride.empty()) return mRootOverride;
    return dataRoot();
}

std::string WorldProjectStore::utcNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    char buffer[40] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
}

bool WorldProjectStore::validProjectId(const std::string& projectId)
{
    if (projectId.empty() || projectId.size() > 64) return false;
    if (!std::isalnum(static_cast<unsigned char>(projectId.front()))) return false;
    for (char c : projectId) {
        const bool okay = std::isalnum(static_cast<unsigned char>(c)) ||
                          c == '.' || c == '_' || c == '-';
        if (!okay) return false;
    }
    return true;
}

bool WorldProjectStore::readManifest(const std::string& projectId,
                                     WorldProjectManifest& out,
                                     std::string* error)
{
    if (!validProjectId(projectId))
        return fail(error, "invalid project id");
    const fs::path path = fs::path(root()) / projectId / kManifestName;
    std::string text;
    if (!readText(path, text, error))
        return false;
    const json rootJson = parseObject(text, error);
    if (rootJson.is_null()) return false;
    out = manifestFromJson(rootJson);
    if (out.projectId != projectId)
        return fail(error, "manifest project_id does not match directory");
    if (out.format != kWorldProjectFormat &&
        out.format != kLegacyWorldProjectFormat)
        return fail(error, "unsupported project format");
    return true;
}

bool WorldProjectStore::writeManifest(const WorldProjectManifest& manifest,
                                      std::string* error)
{
    if (!validProjectId(manifest.projectId))
        return fail(error, "invalid project id");
    const fs::path path = fs::path(root()) / manifest.projectId / kManifestName;
    return writeAtomic(path, canonicalJson(manifestToJson(manifest)), error);
}

bool WorldProjectStore::ensureFormat2Impl(const std::string& projectId,
                                          WorldProjectManifest& out,
                                          std::string* error)
{
    if (!readManifest(projectId, out, error))
        return false;
    if (out.format == kWorldProjectFormat)
        return true;

    const fs::path directory = fs::path(root()) / projectId;
    const fs::path legacy = directory / kLegacyManifestCopy;
    std::error_code ec;
    if (!fs::exists(legacy, ec)) {
        if (!fs::copy_file(directory / kManifestName, legacy,
                           fs::copy_options::none, ec))
            return fail(error, "cannot preserve format-1 manifest: " + ec.message());
    }

    WorldProjectManifest migrated = out;
    migrated.format = kWorldProjectFormat;
    migrated.updatedUtc = utcNow();
    if (migrated.head.empty()) {
        const fs::path versions = directory / kVersionsDir;
        if (fs::is_directory(versions, ec)) {
            for (const auto& entry : fs::directory_iterator(versions, ec)) {
                if (ec) break;
                if (entry.is_directory()) {
                    migrated.head = entry.path().filename().string();
                    break;
                }
            }
        }
    }
    if (migrated.createdUtc.empty()) migrated.createdUtc = migrated.updatedUtc;
    out = migrated;
    return writeManifest(out, error);
}

bool WorldProjectStore::createFromMap(const std::string& projectId,
                                      const std::string& ownerAccountId,
                                      const std::string& title,
                                      const std::string& visibility,
                                      const std::string& mapJson,
                                      std::string* error)
{
    if (!validProjectId(projectId))
        return fail(error, "project id must be 1-64 letters, digits, '.', '_' or '-'");
    const fs::path directory = fs::path(root()) / projectId;
    std::error_code ec;
    if (fs::exists(directory, ec))
        return fail(error, "project already exists: " + projectId);

    const json map = parseObject(mapJson, error);
    if (map.is_null()) return false;

    fs::create_directories(directory / kWorkingDir, ec);
    if (ec)
        return fail(error, "cannot create project directory: " + ec.message());

    WorldProjectManifest manifest;
    manifest.format = kWorldProjectFormat;
    manifest.projectId = projectId;
    manifest.ownerAccountId = ownerAccountId;
    manifest.title = title.empty() ? projectId : title;
    manifest.visibility = visibility == "private" ? "private" : "public";
    manifest.createdUtc = utcNow();
    manifest.updatedUtc = manifest.createdUtc;

    if (!writeAtomic(directory / kWorkingDir / "map.json", canonicalJson(map), error))
        return false;
    if (!writeManifest(manifest, error))
        return false;
    return saveVersion(projectId, ownerAccountId, canonicalJson(map), title,
                       nullptr, error);
}

bool WorldProjectStore::saveVersion(const std::string& projectId,
                                    const std::string& authorAccountId,
                                    const std::string& mapJson,
                                    const std::string& title,
                                    std::string* outVersion,
                                    std::string* error)
{
    WorldProjectManifest manifest;
    if (!ensureFormat2Impl(projectId, manifest, error))
        return false;

    const json map = parseObject(mapJson, error);
    if (map.is_null()) return false;
    const std::string canonical = canonicalJson(map);
    const std::string hash = sha256Hex(canonical);
    const fs::path directory = fs::path(root()) / projectId;
    const fs::path versionDir = directory / kVersionsDir / hash;
    const fs::path mapPath = versionDir / "map.json";
    const fs::path versionPath = versionDir / "version.json";

    std::error_code ec;
    fs::create_directories(versionDir, ec);
    if (ec)
        return fail(error, "cannot create version directory: " + ec.message());

    if (fs::exists(mapPath, ec)) {
        std::string existing;
        if (!readText(mapPath, existing, error))
            return false;
        if (sha256Hex(existing) != hash)
            return fail(error, "immutable version content hash mismatch");
    } else if (!writeAtomic(mapPath, canonical, error)) {
        return false;
    }

    if (!fs::exists(versionPath, ec)) {
        WorldProjectVersion version;
        version.version = hash;
        version.parent = manifest.head;
        version.mapSha256 = hash;
        version.treeSha256 = sha256Hex(canonicalJson(json{
            {"map_sha256", hash},
            {"dependencies", json::array()},
        }));
        version.createdUtc = utcNow();
        version.authorAccountId = authorAccountId;
        version.mapFormat = kWorldProjectFormat;
        if (!writeAtomic(versionPath, canonicalJson(versionToJson(version)), error))
            return false;
    }

    if (!title.empty()) manifest.title = title;
    manifest.format = kWorldProjectFormat;
    manifest.head = hash;
    manifest.updatedUtc = utcNow();
    if (!writeManifest(manifest, error))
        return false;

    if (outVersion) *outVersion = hash;
    return true;
}

bool WorldProjectStore::readVersion(const std::string& projectId,
                                    const std::string& version,
                                    std::string& outMapJson,
                                    WorldProjectVersion* outVersion,
                                    std::string* error)
{
    if (!validProjectId(projectId))
        return fail(error, "invalid project id");
    if (version.size() != 64 ||
        version.find_first_not_of("0123456789abcdef") != std::string::npos)
        return fail(error, "invalid version hash");

    const fs::path versionDir = fs::path(root()) / projectId / kVersionsDir / version;
    std::string mapText;
    if (!readText(versionDir / "map.json", mapText, error))
        return false;
    const std::string actual = sha256Hex(mapText);
    if (actual != version)
        return fail(error, "version content hash mismatch");

    WorldProjectVersion parsed;
    parsed.version = version;
    parsed.mapSha256 = actual;
    const fs::path versionPath = versionDir / "version.json";
    std::error_code ec;
    if (fs::exists(versionPath, ec)) {
        std::string versionText;
        if (!readText(versionPath, versionText, error))
            return false;
        const json versionJson = parseObject(versionText, error);
        if (versionJson.is_null()) return false;
        parsed = versionFromJson(versionJson);
        if (!parsed.version.empty() && parsed.version != version)
            return fail(error, "version metadata does not match its directory");
        if (!parsed.mapSha256.empty() && parsed.mapSha256 != actual)
            return fail(error, "version metadata map hash mismatch");
        parsed.version = version;
        parsed.mapSha256 = actual;
        if (parsed.treeSha256.empty())
            parsed.treeSha256 = sha256Hex(canonicalJson(json{
                {"map_sha256", actual},
                {"dependencies", json::array()},
            }));
    }

    outMapJson = mapText;
    if (outVersion) *outVersion = parsed;
    return true;
}

bool WorldProjectStore::checkout(const std::string& projectId,
                                 const std::string& version,
                                 std::string* error)
{
    std::string mapJson;
    if (!readVersion(projectId, version, mapJson, nullptr, error))
        return false;
    if (!writeWorking(projectId, mapJson, error))
        return false;
    WorldProjectManifest manifest;
    if (!ensureFormat2Impl(projectId, manifest, error))
        return false;
    manifest.head = version;
    manifest.updatedUtc = utcNow();
    return writeManifest(manifest, error);
}

bool WorldProjectStore::readWorking(const std::string& projectId,
                                    std::string& outMapJson,
                                    std::string* error)
{
    if (!validProjectId(projectId))
        return fail(error, "invalid project id");
    return readText(fs::path(root()) / projectId / kWorkingDir / "map.json",
                    outMapJson, error);
}

bool WorldProjectStore::writeWorking(const std::string& projectId,
                                     const std::string& mapJson,
                                     std::string* error)
{
    if (!validProjectId(projectId))
        return fail(error, "invalid project id");
    const json map = parseObject(mapJson, error);
    if (map.is_null()) return false;
    return writeAtomic(fs::path(root()) / projectId / kWorkingDir / "map.json",
                       canonicalJson(map), error);
}

bool WorldProjectStore::listProjects(std::vector<WorldProjectSummary>& out,
                                     std::string* error)
{
    out.clear();
    const fs::path directory = fs::path(root());
    std::error_code ec;
    if (!fs::is_directory(directory, ec))
        return true;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (ec) return fail(error, ec.message());
        if (!entry.is_directory()) continue;
        WorldProjectManifest manifest;
        std::string localError;
        if (!readManifest(entry.path().filename().string(), manifest, &localError))
            continue;
        WorldProjectSummary summary;
        summary.manifest = manifest;
        summary.directory = entry.path().string();
        const fs::path versions = entry.path() / kVersionsDir;
        if (fs::is_directory(versions, ec)) {
            for (const auto& version : fs::directory_iterator(versions, ec)) {
                if (version.is_directory()) ++summary.versionCount;
            }
        }
        out.push_back(std::move(summary));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.manifest.projectId < b.manifest.projectId;
    });
    return true;
}

bool WorldProjectStore::listVersions(const std::string& projectId,
                                     std::vector<WorldProjectVersion>& out,
                                     std::string* error)
{
    out.clear();
    if (!validProjectId(projectId))
        return fail(error, "invalid project id");
    const fs::path versions = fs::path(root()) / projectId / kVersionsDir;
    std::error_code ec;
    if (!fs::is_directory(versions, ec))
        return true;
    for (const auto& entry : fs::directory_iterator(versions, ec)) {
        if (ec) return fail(error, ec.message());
        if (!entry.is_directory()) continue;
        std::string mapJson;
        WorldProjectVersion version;
        std::string localError;
        if (!readVersion(projectId, entry.path().filename().string(), mapJson,
                         &version, &localError))
            continue;
        out.push_back(std::move(version));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.createdUtc < b.createdUtc;
    });
    return true;
}

bool WorldProjectStore::fork(const std::string& sourceProjectId,
                             const std::string& sourceVersion,
                             const std::string& newProjectId,
                             const std::string& ownerAccountId,
                             const std::string& title,
                             const std::string& visibility,
                             std::string* error)
{
    if (!validProjectId(newProjectId))
        return fail(error, "invalid fork project id");
    const fs::path target = fs::path(root()) / newProjectId;
    std::error_code ec;
    if (fs::exists(target, ec))
        return fail(error, "fork project already exists: " + newProjectId);

    WorldProjectManifest source;
    if (!ensureFormat2Impl(sourceProjectId, source, error))
        return false;
    const std::string version = sourceVersion.empty() ? source.head : sourceVersion;
    if (version.empty())
        return fail(error, "source project has no saved version");

    std::string mapJson;
    WorldProjectVersion sourceVersionData;
    if (!readVersion(sourceProjectId, version, mapJson, &sourceVersionData, error))
        return false;

    const fs::path sourceDir = fs::path(root()) / sourceProjectId / kVersionsDir / version;
    const fs::path targetVersionDir = target / kVersionsDir / version;
    fs::create_directories(targetVersionDir.parent_path(), ec);
    if (ec)
        return fail(error, "cannot create fork directory: " + ec.message());
    fs::copy(sourceDir, targetVersionDir,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec)
        return fail(error, "cannot copy fork version: " + ec.message());

    WorldProjectManifest manifest;
    manifest.format = kWorldProjectFormat;
    manifest.projectId = newProjectId;
    manifest.ownerAccountId = ownerAccountId;
    manifest.title = title.empty() ? newProjectId : title;
    manifest.visibility = visibility == "private" ? "private" : "public";
    manifest.license = source.license;
    manifest.parent.present = true;
    manifest.parent.projectId = sourceProjectId;
    manifest.parent.version = version;
    manifest.head = version;
    manifest.createdUtc = utcNow();
    manifest.updatedUtc = manifest.createdUtc;

    if (!writeWorking(newProjectId, mapJson, error))
        return false;
    return writeManifest(manifest, error);
}

bool WorldProjectStore::exportPackage(const std::string& projectId,
                                      const std::string& version,
                                      const std::string& destination,
                                      WorldProjectExportResult* result,
                                      std::string* error)
{
    WorldProjectManifest manifest;
    if (!ensureFormat2Impl(projectId, manifest, error))
        return false;
    const std::string selected = version.empty() ? manifest.head : version;
    std::string mapJson;
    WorldProjectVersion versionData;
    if (!readVersion(projectId, selected, mapJson, &versionData, error))
        return false;

    const fs::path target(destination);
    std::error_code ec;
    fs::create_directories(target / kVersionsDir / selected, ec);
    if (ec)
        return fail(error, "cannot create export directory: " + ec.message());

    const fs::path mapPath = target / kVersionsDir / selected / "map.json";
    const fs::path versionPath = target / kVersionsDir / selected / "version.json";
    if (!writeAtomic(mapPath, mapJson, error)) return false;
    if (!writeAtomic(versionPath, canonicalJson(versionToJson(versionData)), error))
        return false;

    json files = json::array();
    files.push_back({{"path", (fs::path(kVersionsDir) / selected / "map.json").generic_string()},
                     {"sha256", sha256Hex(mapJson)}});
    files.push_back({{"path", (fs::path(kVersionsDir) / selected / "version.json").generic_string()},
                     {"sha256", sha256File(versionPath.string())}});

    json dependencies = json::array();
    size_t blobCount = 0;
    for (const auto& dep : versionData.dependencies) {
        json dependency = dependencyToJson(dep);
        bool included = false;
        if (!dep.blobSha256.empty()) {
            const fs::path blobSource = fs::path(root()) / kBlobsDir / dep.blobSha256;
            if (fs::exists(blobSource, ec)) {
                const fs::path blobTarget = target / kBlobsDir / dep.blobSha256;
                fs::create_directories(blobTarget.parent_path(), ec);
                if (!ec) {
                    fs::copy_file(blobSource, blobTarget,
                                  fs::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        included = true;
                        ++blobCount;
                        files.push_back({
                            {"path", (fs::path(kBlobsDir) / dep.blobSha256).generic_string()},
                            {"sha256", sha256File(blobTarget.string())},
                        });
                    }
                }
            }
        }
        dependency["included"] = included;
        dependencies.push_back(std::move(dependency));
    }

    const json package = json{
        {"format", kWorldProjectFormat},
        {"project_id", projectId},
        {"version", selected},
        {"manifest", manifestToJson(manifest)},
        {"version_metadata", versionToJson(versionData)},
        {"files", files},
        {"dependencies", dependencies},
        {"attribution", json{{"creator", manifest.ownerAccountId},
                              {"license", licenseToJson(manifest.license)}}},
        {"exported_utc", utcNow()},
    };
    if (!writeAtomic(target / "package.json", canonicalJson(package), error))
        return false;

    if (result) {
        result->directory = target.string();
        result->version = selected;
        result->manifestSha256 = sha256Hex(canonicalJson(package));
        result->mapSha256 = sha256Hex(mapJson);
        result->blobCount = blobCount;
    }
    return true;
}

bool WorldProjectStore::importPackage(const std::string& sourceDirectory,
                                      const std::string& projectIdOverride,
                                      WorldProjectImportResult* result,
                                      std::string* error)
{
    const fs::path source(sourceDirectory);
    std::string packageText;
    if (!readText(source / "package.json", packageText, error))
        return false;
    const json package = parseObject(packageText, error);
    if (package.is_null()) return false;
    if (!package.contains("manifest") || !package["manifest"].is_object())
        return fail(error, "package has no manifest");

    WorldProjectManifest manifest = manifestFromJson(package["manifest"]);
    const std::string projectId = projectIdOverride.empty()
        ? manifest.projectId : projectIdOverride;
    if (!validProjectId(projectId))
        return fail(error, "invalid package project id");
    const fs::path target = fs::path(root()) / projectId;
    std::error_code ec;
    if (fs::exists(target, ec))
        return fail(error, "project already exists: " + projectId);

    if (package.contains("files") && package["files"].is_array()) {
        for (const auto& item : package["files"]) {
            if (!item.is_object()) continue;
            fs::path relative;
            if (!safeRelativePath(item.value("path", ""), relative))
                return fail(error, "package contains an unsafe path");
            const std::string expected = item.value("sha256", "");
            const fs::path sourceFile = source / relative;
            std::string localError;
            const std::string actual = sha256File(sourceFile.string(), &localError);
            if (actual.empty() || actual != expected)
                return fail(error, "package file hash mismatch: " + relative.string());
        }
    }

    const std::string version = package.value("version", "");
    if (version.empty())
        return fail(error, "package has no version");
    const fs::path sourceVersion = source / kVersionsDir / version;
    const fs::path targetVersion = target / kVersionsDir / version;
    fs::create_directories(targetVersion, ec);
    if (ec)
        return fail(error, "cannot create import directory: " + ec.message());
    fs::copy(sourceVersion, targetVersion,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec)
        return fail(error, "cannot copy package version: " + ec.message());

    const fs::path sourceBlobs = source / kBlobsDir;
    if (fs::is_directory(sourceBlobs, ec)) {
        const fs::path targetBlobs = target / kBlobsDir;
        fs::create_directories(targetBlobs, ec);
        if (!ec) {
            fs::copy(sourceBlobs, targetBlobs,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        }
    }

    manifest.format = kWorldProjectFormat;
    manifest.projectId = projectId;
    manifest.head = version;
    manifest.updatedUtc = utcNow();
    if (manifest.createdUtc.empty()) manifest.createdUtc = manifest.updatedUtc;
    if (!writeManifest(manifest, error))
        return false;

    std::string mapJson;
    if (!readVersion(projectId, version, mapJson, nullptr, error))
        return false;
    if (!writeWorking(projectId, mapJson, error))
        return false;

    if (result) {
        result->projectId = projectId;
        result->version = version;
        result->directory = target.string();
    }
    return true;
}

bool WorldProjectStore::writeDraft(const WorldProjectDraft& draft,
                                   std::string* error)
{
    if (!validProjectId(draft.projectId))
        return fail(error, "invalid draft project id");
    if (!validProjectId(draft.draftId))
        return fail(error, "invalid draft id");
    WorldProjectDraft copy = draft;
    if (copy.createdUtc.empty()) copy.createdUtc = utcNow();
    const fs::path path = fs::path(root()) / draft.projectId / kDraftsDir /
                          (draft.draftId + ".json");
    return writeAtomic(path, canonicalJson(draftToJson(copy)), error);
}

bool WorldProjectStore::readDraft(const std::string& projectId,
                                  const std::string& draftId,
                                  WorldProjectDraft& out,
                                  std::string* error)
{
    if (!validProjectId(projectId) || !validProjectId(draftId))
        return fail(error, "invalid draft identity");
    const fs::path path = fs::path(root()) / projectId / kDraftsDir /
                          (draftId + ".json");
    std::string text;
    if (!readText(path, text, error))
        return false;
    const json rootJson = parseObject(text, error);
    if (rootJson.is_null()) return false;
    out = draftFromJson(rootJson);
    return true;
}

bool WorldProjectStore::listDrafts(const std::string& projectId,
                                   std::vector<WorldProjectDraft>& out,
                                   std::string* error)
{
    out.clear();
    if (!validProjectId(projectId))
        return fail(error, "invalid project id");
    const fs::path directory = fs::path(root()) / projectId / kDraftsDir;
    std::error_code ec;
    if (!fs::is_directory(directory, ec))
        return true;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (ec) return fail(error, ec.message());
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;
        WorldProjectDraft draft;
        std::string localError;
        if (readDraft(projectId, entry.path().stem().string(), draft, &localError))
            out.push_back(std::move(draft));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.createdUtc < b.createdUtc;
    });
    return true;
}

bool WorldProjectStore::promoteDraft(const std::string& projectId,
                                     const std::string& draftId,
                                     const std::string& authorAccountId,
                                     std::string* outVersion,
                                     std::string* error)
{
    WorldProjectDraft draft;
    if (!readDraft(projectId, draftId, draft, error))
        return false;
    if (!writeWorking(projectId, draft.stateJson, error))
        return false;
    return saveVersion(projectId, authorAccountId, draft.stateJson, "",
                       outVersion, error);
}

bool WorldProjectStore::migrateFormat1(const std::string& projectId,
                                       std::string* error)
{
    WorldProjectManifest ignored;
    return ensureFormat2Impl(projectId, ignored, error);
}

bool WorldProjectStore::migrateAllFormat1(std::vector<std::string>& migrated,
                                          std::string* error)
{
    migrated.clear();
    const fs::path directory = fs::path(root());
    std::error_code ec;
    if (!fs::is_directory(directory, ec))
        return true;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (ec) return fail(error, ec.message());
        if (!entry.is_directory()) continue;
        WorldProjectManifest manifest;
        std::string localError;
        if (!readManifest(entry.path().filename().string(), manifest, &localError))
            continue;
        if (manifest.format == kLegacyWorldProjectFormat) {
            if (!migrateFormat1(manifest.projectId, &localError)) {
                if (error) *error = localError;
                return false;
            }
            migrated.push_back(manifest.projectId);
        }
    }
    return true;
}

} // namespace MimitaWorldProject
