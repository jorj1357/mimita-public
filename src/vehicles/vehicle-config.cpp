// 2026-10-27

#include "vehicles/vehicle-config.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

#include <nlohmann/json.hpp>

#include "debug/debug-log.h"
#include "utils/json-comments.h"

using json = nlohmann::json;

namespace {

int64_t modifiedTime(const std::string& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec)
        return 0;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               time.time_since_epoch())
        .count();
}

float readFloat(const json& root, const char* key, float fallback,
                float minimum, float maximum)
{
    if (!root.contains(key) || !root[key].is_number())
        return fallback;
    const float value = root[key].get<float>();
    if (!std::isfinite(value))
        return fallback;
    return std::clamp(value, minimum, maximum);
}

glm::vec3 readVec3(const json& root, const char* key, const glm::vec3& fallback)
{
    if (!root.contains(key) || !root[key].is_array() || root[key].size() != 3)
        return fallback;
    glm::vec3 value(fallback);
    for (int i = 0; i < 3; ++i)
    {
        if (!root[key][i].is_number())
            return fallback;
        value[i] = root[key][i].get<float>();
        if (!std::isfinite(value[i]))
            return fallback;
    }
    return value;
}

bool readBool(const json& root, const char* key, bool fallback)
{
    if (!root.contains(key) || !root[key].is_boolean())
        return fallback;
    return root[key].get<bool>();
}

std::string readString(const json& root, const char* key,
                       const std::string& fallback)
{
    if (!root.contains(key) || !root[key].is_string())
        return fallback;
    return root[key].get<std::string>();
}

VehicleWheelSpec parseWheel(const json& wheel)
{
    VehicleWheelSpec spec;
    spec.hub = readVec3(wheel, "hub", spec.hub);
    spec.radius = readFloat(wheel, "radius", spec.radius, 0.05f, 2.0f);
    spec.width = readFloat(wheel, "width", spec.width, 0.02f, 1.5f);
    spec.suspensionAxis =
        readVec3(wheel, "suspensionAxis", spec.suspensionAxis);
    if (glm::dot(spec.suspensionAxis, spec.suspensionAxis) < 1e-6f)
        spec.suspensionAxis = glm::vec3(0.0f, 0.0f, -1.0f);
    spec.suspensionAxis = glm::normalize(spec.suspensionAxis);
    spec.spinAxis = readVec3(wheel, "spinAxis", spec.spinAxis);
    if (glm::dot(spec.spinAxis, spec.spinAxis) < 1e-6f)
        spec.spinAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    spec.spinAxis = glm::normalize(spec.spinAxis);
    spec.steerAxis = readVec3(wheel, "steerAxis", spec.steerAxis);
    if (glm::dot(spec.steerAxis, spec.steerAxis) < 1e-6f)
        spec.steerAxis = glm::vec3(0.0f, 0.0f, 1.0f);
    spec.steerAxis = glm::normalize(spec.steerAxis);
    spec.restLength =
        readFloat(wheel, "restLength", spec.restLength, 0.0f, 3.0f);
    spec.travel = readFloat(wheel, "travel", spec.travel, 0.0f, 2.0f);
    spec.springRate =
        readFloat(wheel, "springRate", spec.springRate, 0.0f, 2.0e6f);
    spec.damperRate =
        readFloat(wheel, "damperRate", spec.damperRate, 0.0f, 2.0e5f);
    spec.antiRoll =
        readFloat(wheel, "antiRoll", spec.antiRoll, 0.0f, 2.0e6f);
    spec.unsprungMass =
        readFloat(wheel, "unsprungMass", spec.unsprungMass, 0.1f, 500.0f);
    spec.steer = readBool(wheel, "steer", spec.steer);
    spec.drive = readBool(wheel, "drive", spec.drive);
    spec.brake = readBool(wheel, "brake", spec.brake);
    return spec;
}

VehicleTireSpec parseTire(const json& root)
{
    VehicleTireSpec spec;
    spec.mu = readFloat(root, "mu", spec.mu, 0.05f, 4.0f);
    spec.loadSensitivity = readFloat(root, "loadSensitivity",
                                     spec.loadSensitivity, 0.0f, 0.01f);
    spec.nominalLoad = readFloat(root, "nominalLoad", spec.nominalLoad,
                                 1.0f, 1.0e6f);
    spec.peakSlipRatio = readFloat(root, "peakSlipRatio",
                                   spec.peakSlipRatio, 0.01f, 2.0f);
    spec.peakSlipAngle = readFloat(root, "peakSlipAngle",
                                   spec.peakSlipAngle, 0.01f, 1.5f);
    spec.frictionCircle = readBool(root, "frictionCircle", spec.frictionCircle);
    spec.lowSpeed = readFloat(root, "lowSpeed", spec.lowSpeed, 0.01f, 10.0f);
    return spec;
}

VehicleEngineSpec parseEngine(const json& root)
{
    VehicleEngineSpec spec;
    spec.inertia = readFloat(root, "inertia", spec.inertia, 0.01f, 50.0f);
    spec.idleRpm = readFloat(root, "idleRpm", spec.idleRpm, 1.0f, 5000.0f);
    spec.redlineRpm =
        readFloat(root, "redlineRpm", spec.redlineRpm, spec.idleRpm + 100.0f,
                  20000.0f);
    spec.frictionTorque = readFloat(root, "frictionTorque",
                                    spec.frictionTorque, 0.0f, 5000.0f);
    if (root.contains("torqueCurve") && root["torqueCurve"].is_array())
    {
        spec.torqueCurve.clear();
        for (const json& point : root["torqueCurve"])
        {
            if (!point.is_array() || point.size() != 2 ||
                !point[0].is_number() || !point[1].is_number())
                continue;
            const float rpm = point[0].get<float>();
            const float torque = point[1].get<float>();
            if (!std::isfinite(rpm) || !std::isfinite(torque) || rpm < 0.0f)
                continue;
            spec.torqueCurve.emplace_back(rpm, std::max(torque, 0.0f));
        }
        std::sort(spec.torqueCurve.begin(), spec.torqueCurve.end(),
                  [](const glm::vec2& a, const glm::vec2& b) {
                      return a.x < b.x;
                  });
    }
    if (root.contains("turbo") && root["turbo"].is_object())
    {
        const json& turbo = root["turbo"];
        spec.turboMaxBoost = readFloat(turbo, "maxBoost", 0.0f, 0.0f, 5.0f);
        spec.turboSpoolRate =
            readFloat(turbo, "spoolRate", spec.turboSpoolRate, 0.0f, 50.0f);
        spec.turboLag = readFloat(turbo, "lag", spec.turboLag, 0.0f, 2.0f);
    }
    return spec;
}

VehicleGearboxSpec parseGearbox(const json& root)
{
    VehicleGearboxSpec spec;
    if (root.contains("ratios") && root["ratios"].is_array())
    {
        spec.ratios.clear();
        for (const json& value : root["ratios"])
        {
            if (!value.is_number())
                continue;
            const float ratio = value.get<float>();
            if (std::isfinite(ratio))
                spec.ratios.push_back(ratio);
        }
    }
    if (spec.ratios.size() < 3)
        spec.ratios = VehicleGearboxSpec{}.ratios;
    spec.finalDrive = readFloat(root, "finalDrive", spec.finalDrive,
                                0.1f, 20.0f);
    spec.shiftTime = readFloat(root, "shiftTime", spec.shiftTime, 0.0f, 2.0f);
    spec.clutchCapacity = readFloat(root, "clutchCapacity",
                                    spec.clutchCapacity, 1.0f, 20000.0f);
    return spec;
}

} // namespace

VehicleConfigRegistry& VehicleConfigRegistry::instance()
{
    static VehicleConfigRegistry registry;
    return registry;
}

bool VehicleConfigRegistry::loadFile(const std::string& path,
                                     VehicleSpec& out) const
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    const json root = parseJsonConfig(file);
    if (!root.is_object())
        return false;

    VehicleSpec spec;
    spec.id = readString(root, "id", "");
    if (spec.id.empty())
    {
        const std::filesystem::path filePath(path);
        spec.id = filePath.stem().string();
    }
    spec.massKg = readFloat(root, "massKg", spec.massKg, 1.0f, 1.0e6f);
    spec.density = readFloat(root, "density", spec.density, 0.001f, 1.0e6f);
    spec.centerOfMass =
        readVec3(root, "centerOfMass", spec.centerOfMass);
    const float length = readFloat(root, "length", spec.halfExtents.x * 2.0f,
                                   0.2f, 30.0f);
    const float width = readFloat(root, "width", spec.halfExtents.y * 2.0f,
                                  0.2f, 10.0f);
    const float height = readFloat(root, "height", spec.halfExtents.z * 2.0f,
                                   0.2f, 10.0f);
    spec.halfExtents = glm::vec3(length, width, height) * 0.5f;
    spec.drag = readFloat(root, "drag", spec.drag, 0.0f, 10.0f);
    spec.downforce = readFloat(root, "downforce", spec.downforce, 0.0f, 10.0f);
    spec.interactionRadius = readFloat(root, "interactionRadius",
                                       spec.interactionRadius, 1.0f, 12.0f);
    spec.bodyTexture = readString(root, "bodyTexture", spec.bodyTexture);
    spec.wheelTexture = readString(root, "wheelTexture", spec.wheelTexture);
    spec.engineSound = readString(root, "engineSound", spec.engineSound);
    spec.wheelSegments = static_cast<int>(
        readFloat(root, "wheelSegments", (float)spec.wheelSegments, 8.0f,
                  64.0f));

    if (root.contains("wheels") && root["wheels"].is_array())
    {
        spec.wheels.clear();
        for (const json& wheel : root["wheels"])
        {
            if (wheel.is_object())
                spec.wheels.push_back(parseWheel(wheel));
        }
    }
    if (spec.wheels.size() < 2)
        return false;

    if (root.contains("tire") && root["tire"].is_object())
        spec.tire = parseTire(root["tire"]);
    if (root.contains("engine") && root["engine"].is_object())
        spec.engine = parseEngine(root["engine"]);
    if (root.contains("gearbox") && root["gearbox"].is_object())
        spec.gearbox = parseGearbox(root["gearbox"]);
    if (root.contains("differential") && root["differential"].is_object())
    {
        const json& diff = root["differential"];
        spec.differential.type =
            readString(diff, "type", spec.differential.type);
        spec.differential.frontRearSplit =
            readFloat(diff, "frontRearSplit", spec.differential.frontRearSplit,
                      0.0f, 1.0f);
    }
    if (root.contains("steering") && root["steering"].is_object())
    {
        const json& steering = root["steering"];
        spec.steering.maxAngleDeg =
            readFloat(steering, "maxAngleDeg", spec.steering.maxAngleDeg,
                      1.0f, 80.0f);
        spec.steering.speedFalloff =
            readFloat(steering, "speedFalloff", spec.steering.speedFalloff,
                      0.0f, 2.0f);
        spec.steering.returnRate =
            readFloat(steering, "returnRate", spec.steering.returnRate,
                      0.0f, 20.0f);
    }
    if (root.contains("brakes") && root["brakes"].is_object())
    {
        const json& brakes = root["brakes"];
        spec.brakes.maxTorque =
            readFloat(brakes, "maxTorque", spec.brakes.maxTorque,
                      0.0f, 100000.0f);
        spec.brakes.handbrakeTorque =
            readFloat(brakes, "handbrakeTorque", spec.brakes.handbrakeTorque,
                      0.0f, 100000.0f);
    }
    if (root.contains("seats") && root["seats"].is_array())
    {
        spec.seats.clear();
        for (const json& seat : root["seats"])
        {
            if (!seat.is_object())
                continue;
            VehicleSeatSpec seatSpec;
            seatSpec.name = readString(seat, "name", "seat");
            seatSpec.anchor = readVec3(seat, "anchor", seatSpec.anchor);
            seatSpec.pose = readString(seat, "pose", seatSpec.pose);
            spec.seats.push_back(seatSpec);
        }
    }
    if (spec.seats.empty())
    {
        spec.seats.push_back(VehicleSeatSpec{"driver",
                                             glm::vec3(-0.38f, 0.0f, 0.1f),
                                             "sit"});
        spec.seats.push_back(VehicleSeatSpec{"passenger",
                                             glm::vec3(0.38f, 0.0f, 0.1f),
                                             "sit"});
    }

    out = std::move(spec);
    return true;
}

bool VehicleConfigRegistry::loadDirectory(const std::string& directory)
{
    mDirectory = directory;
    std::unordered_map<std::string, VehicleSpec> specs;
    std::unordered_map<std::string, int64_t> times;

    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec))
    {
        Debug::warn(Debug::Category::Physics,
                    "[VEHICLE CONFIG] missing directory %s\n",
                    directory.c_str());
        mSpecs.clear();
        mFileTimes.clear();
        ++mRevision;
        return false;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(directory, ec))
    {
        if (ec || !entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;
        VehicleSpec spec;
        if (!loadFile(entry.path().string(), spec))
        {
            Debug::warn(Debug::Category::Physics,
                        "[VEHICLE CONFIG] invalid %s; skipped\n",
                        entry.path().string().c_str());
            continue;
        }
        times[entry.path().string()] = modifiedTime(entry.path().string());
        specs[spec.id] = std::move(spec);
    }

    mSpecs = std::move(specs);
    mFileTimes = std::move(times);
    ++mRevision;
    Debug::log(Debug::Category::Physics,
               "[VEHICLE CONFIG] loaded %zu templates from %s\n",
               mSpecs.size(), directory.c_str());
    return !mSpecs.empty();
}

bool VehicleConfigRegistry::pollReload()
{
    std::error_code ec;
    if (!std::filesystem::is_directory(mDirectory, ec))
        return false;

    bool changed = false;
    std::set<std::string> seen;
    for (const auto& entry :
         std::filesystem::directory_iterator(mDirectory, ec))
    {
        if (ec || !entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;
        const std::string path = entry.path().string();
        seen.insert(path);
        const int64_t time = modifiedTime(path);
        auto it = mFileTimes.find(path);
        if (it == mFileTimes.end() || it->second != time)
            changed = true;
    }
    if (seen.size() != mFileTimes.size())
        changed = true;
    if (changed)
        return loadDirectory(mDirectory);
    return false;
}

const VehicleSpec* VehicleConfigRegistry::find(const std::string& id) const
{
    auto it = mSpecs.find(id);
    return it != mSpecs.end() ? &it->second : nullptr;
}

std::vector<std::string> VehicleConfigRegistry::ids() const
{
    std::vector<std::string> out;
    out.reserve(mSpecs.size());
    for (const auto& pair : mSpecs)
        out.push_back(pair.first);
    std::sort(out.begin(), out.end());
    return out;
}
