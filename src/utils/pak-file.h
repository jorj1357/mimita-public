#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct PakEntry {
    std::string path;
    uint64_t offset;
    uint64_t size;
};

class PakFile {
public:
    PakFile() = default;
    ~PakFile();

    PakFile(const PakFile&) = delete;
    PakFile& operator=(const PakFile&) = delete;

    bool open(const std::string& pakPath);
    void close();

    bool fileExists(const std::string& path) const;
    bool readFile(const std::string& path, std::vector<char>& out) const;
    bool extractTo(const std::string& path, const std::string& destDir) const;
    bool extractAllTo(const std::string& destDir) const;

    int numFiles() const { return (int)mEntries.size(); }

private:
    const PakEntry* find(const std::string& path) const;

    std::vector<PakEntry> mEntries;
    FILE* mFile = nullptr;
};
