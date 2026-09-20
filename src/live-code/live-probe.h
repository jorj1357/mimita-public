#pragma once

#include "debug/structured-log.h"

#include <chrono>
#include <string>

namespace LiveProbe {

bool enabled(const std::string& name);

class Event {
public:
    explicit Event(std::string name);
    ~Event();

    Event& field(const std::string& key, const nlohmann::json& value);
    Event& number(const std::string& key, double value);
    Event& boolean(const std::string& key, bool value);
    void emit();

private:
    std::string name_;
    nlohmann::json fields_ = nlohmann::json::object();
    std::chrono::steady_clock::time_point started_;
    bool emitted_ = false;
};

Event begin(const std::string& name);
void emit(const std::string& name, const nlohmann::json& fields);

} // namespace LiveProbe

class LiveProbeScope {
public:
    explicit LiveProbeScope(const char* name) : event_(LiveProbe::begin(name)) {}
    ~LiveProbeScope() = default;

    LiveProbeScope& field(const std::string& key, const nlohmann::json& value)
    {
        event_.field(key, value);
        return *this;
    }

private:
    LiveProbe::Event event_;
};
