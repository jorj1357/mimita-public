#pragma once

#include "dev-types.h"
#include <vector>
#include <string>

class DevConfig {
public:
    static DevConfig& instance();
    
    bool load(const char* path);
    void reload();
    
    const std::vector<DevBinding>& bindings() const { return mBindings; }
    const DevBinding* findByKey(int key) const;
    const DevBinding* findByAction(const std::string& action) const;
    const std::string& configPath() const { return mConfigPath; }
    
private:
    DevConfig() = default;
    std::vector<DevBinding> mBindings;
    std::string mConfigPath;
};
