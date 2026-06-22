#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class AnalyticsUploader
{
public:
    void enqueue(const nlohmann::json& event);
    void update(float dt, const std::string& endpoint);
    void flush(const std::string& endpoint, bool blocking);

    static bool postJson(const std::string& endpoint, const nlohmann::json& body);

private:
    std::mutex mMutex;
    std::vector<nlohmann::json> mQueue;
    float mFlushTimer = 0.0f;
};
