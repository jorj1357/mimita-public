#pragma once

#include <string>

class LocalProfileSystem
{
public:
    static LocalProfileSystem& instance();

    void init();
    const std::string& currentUsername() const;
    bool signIn(const std::string& username, const std::string& password);
    const std::string& lastError() const;

private:
    LocalProfileSystem() = default;

    void ensureFiles();
    std::string makeFallbackUsername() const;

    std::string currentUsername_;
    std::string lastError_;
};

