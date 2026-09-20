#include "live-probe.h"

#include <algorithm>

namespace LiveProbe {

bool enabled(const std::string& name)
{
    return StructuredLogger::instance().probeEnabled(name);
}

Event::Event(std::string name)
    : name_(std::move(name)), started_(std::chrono::steady_clock::now())
{
}

Event::~Event()
{
    if (!emitted_)
        emit();
}

Event& Event::field(const std::string& key, const nlohmann::json& value)
{
    fields_[key] = value;
    return *this;
}

Event& Event::number(const std::string& key, double value)
{
    return field(key, value);
}

Event& Event::boolean(const std::string& key, bool value)
{
    return field(key, value);
}

void Event::emit()
{
    emitted_ = true;
    if (!enabled(name_))
        return;

    const auto elapsed = std::chrono::steady_clock::now() - started_;
    const auto durationUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
    debug::Event event;
    event.category = "PERFORMANCE";
    event.name = name_;
    event.level = debug::Level::Info;
    event.fields = fields_;
    event.durationUs = durationUs;
    event.aggregationKey = "probe:" + name_;
    debug::logEvent(event);
}

Event begin(const std::string& name)
{
    return Event(name);
}

void emit(const std::string& name, const nlohmann::json& fields)
{
    Event event(name);
    event.field("payload", fields);
    event.emit();
}

} // namespace LiveProbe
