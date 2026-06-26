#include "utils/pak-file.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define mkdir(a, b) _mkdir(a)
#define fseeko _fseeki64
#else
#include <sys/stat.h>
#endif

namespace {

bool isSep(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

void normalizePath(std::string& path)
{
#ifdef _WIN32
    for (auto& c : path)
        if (c == '\\') c = '/';
#endif
}

bool createDirs(const std::string& path)
{
    std::string p = path;
    normalizePath(p);
    for (size_t i = 0; i < p.size(); ++i) {
        if (p[i] == '/') {
            p[i] = '\0';
            mkdir(p.c_str(), 0755);
            p[i] = '/';
        }
    }
    mkdir(p.c_str(), 0755);
    return true;
}

bool readFileData(FILE* f, uint64_t offset, uint64_t size, std::vector<char>& out)
{
    if (fseeko(f, offset, SEEK_SET) != 0)
        return false;
    out.resize((size_t)size);
    if (size == 0) return true;
    return fread(out.data(), 1, (size_t)size, f) == (size_t)size;
}

}

PakFile::~PakFile()
{
    close();
}

bool PakFile::open(const std::string& pakPath)
{
    close();

    FILE* f = fopen(pakPath.c_str(), "rb");
    if (!f) return false;

    // Read magic
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "PAK1", 4) != 0) {
        fclose(f);
        return false;
    }

    // Read file count
    uint32_t numFiles = 0;
    if (fread(&numFiles, 4, 1, f) != 1) {
        fclose(f);
        return false;
    }

    mEntries.resize(numFiles);
    for (uint32_t i = 0; i < numFiles; ++i) {
        uint16_t pathLen = 0;
        if (fread(&pathLen, 2, 1, f) != 1) {
            fclose(f);
            return false;
        }
        std::string path((size_t)pathLen, '\0');
        if (fread(&path[0], 1, pathLen, f) != pathLen) {
            fclose(f);
            return false;
        }
        uint64_t offset = 0, size = 0;
        if (fread(&offset, 8, 1, f) != 1 || fread(&size, 8, 1, f) != 1) {
            fclose(f);
            return false;
        }
        normalizePath(path);
        mEntries[i] = { path, offset, size };
    }

    mFile = f;
    return true;
}

void PakFile::close()
{
    if (mFile) {
        fclose((FILE*)mFile);
        mFile = nullptr;
    }
    mEntries.clear();
}

const PakEntry* PakFile::find(const std::string& path) const
{
    std::string key = path;
    normalizePath(key);
    for (const auto& e : mEntries) {
        if (e.path == key)
            return &e;
    }
    return nullptr;
}

bool PakFile::fileExists(const std::string& path) const
{
    return find(path) != nullptr;
}

bool PakFile::readFile(const std::string& path, std::vector<char>& out) const
{
    if (!mFile) return false;
    const PakEntry* e = find(path);
    if (!e) return false;
    return readFileData((FILE*)mFile, e->offset, e->size, out);
}

bool PakFile::extractTo(const std::string& path, const std::string& destDir) const
{
    if (!mFile) return false;
    const PakEntry* e = find(path);
    if (!e) return false;

    std::string destPath = destDir + "/" + e->path;
    normalizePath(destPath);
    size_t slash = destPath.find_last_of('/');
    if (slash != std::string::npos)
        createDirs(destPath.substr(0, slash));

    FILE* out = fopen(destPath.c_str(), "wb");
    if (!out) return false;

    std::vector<char> buf;
    readFileData((FILE*)mFile, e->offset, e->size, buf);
    if (!buf.empty())
        fwrite(buf.data(), 1, buf.size(), out);
    fclose(out);
    return true;
}

bool PakFile::extractAllTo(const std::string& destDir) const
{
    if (!mFile) return false;
    createDirs(destDir);
    for (const auto& e : mEntries) {
        std::string destPath = destDir + "/" + e.path;
        normalizePath(destPath);
        size_t slash = destPath.find_last_of('/');
        if (slash != std::string::npos)
            createDirs(destPath.substr(0, slash));

        FILE* out = fopen(destPath.c_str(), "wb");
        if (!out) return false;

        std::vector<char> buf;
        readFileData((FILE*)mFile, e.offset, e.size, buf);
        if (!buf.empty())
            fwrite(buf.data(), 1, buf.size(), out);
        fclose(out);
    }
    return true;
}
