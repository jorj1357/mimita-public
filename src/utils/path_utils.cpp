#include "path_utils.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <fstream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif

std::string getExecutableDirectory() {
#ifdef __APPLE__
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        // Resolve symlinks
        char resolved[PATH_MAX];
        if (realpath(path, resolved) != nullptr) {
            std::string exePath(resolved);
            size_t lastSlash = exePath.find_last_of("/");
            if (lastSlash != std::string::npos) {
                return exePath.substr(0, lastSlash + 1);
            }
        }
    }
#elif defined(_WIN32)
    char path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, path, MAX_PATH) != 0) {
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash + 1);
        }
    }
#elif defined(__linux__)
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
    if (count != -1) {
        path[count] = '\0';
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("/");
        if (lastSlash != std::string::npos) {
            return exePath.substr(0, lastSlash + 1);
        }
    }
#endif
    
    // Fallback: return current directory
    return "./";
}

// jan 25 2026 jorj sas that this might not crash teh whole repo but idk 
std::string resolveAssetPath(std::string relativePath) {
// std::string resolveAssetPath(const std::string& relativePath) {
    std::string exeDir = getExecutableDirectory();
    
    // Try executable directory first
    std::string fullPath = exeDir + relativePath;
    
    // Check if file exists
    std::ifstream testFile(fullPath);
    if (testFile.good()) {
        testFile.close();
        return fullPath;
    }
    
    // Fallback to relative path
    return relativePath;
}

std::string findSystemFont(const std::string& preferredFont) {
#ifdef __APPLE__
    // macOS font locations
    const char* fontDirs[] = {
        "/System/Library/Fonts/Supplemental/",
        "/System/Library/Fonts/",
        "/Library/Fonts/",
        "~/Library/Fonts/",
        nullptr
    };
    
    // Common font name mappings
    std::string fontName = preferredFont;
    if (fontName == "Arial") {
        fontName = "Arial.ttf";
    } else if (fontName.find('.') == std::string::npos) {
        fontName += ".ttf";
    }
    
    // Try each font directory
    for (int i = 0; fontDirs[i] != nullptr; ++i) {
        std::string dir(fontDirs[i]);
        if (dir[0] == '~') {
            const char* home = getenv("HOME");
            if (home) {
                dir = std::string(home) + dir.substr(1);
            } else {
                continue;
            }
        }
        
        std::string fullPath = dir + fontName;
        std::ifstream testFile(fullPath);
        if (testFile.good()) {
            testFile.close();
            return fullPath;
        }
        
        // Also try with different case (macOS is case-insensitive but let's be thorough)
        std::string lowerPath = dir;
        for (char& c : lowerPath) {
            if (c >= 'A' && c <= 'Z') c += 32;
        }
        lowerPath += fontName;
        testFile.open(lowerPath);
        if (testFile.good()) {
            testFile.close();
            return lowerPath;
        }
    }
    
    // Try to find any TTF font as fallback
    for (int i = 0; fontDirs[i] != nullptr; ++i) {
        std::string dir(fontDirs[i]);
        if (dir[0] == '~') {
            const char* home = getenv("HOME");
            if (home) {
                dir = std::string(home) + dir.substr(1);
            } else {
                continue;
            }
        }
        
        // Try common fallback fonts
        const char* fallbacks[] = {
            "Helvetica.ttc",
            "HelveticaNeue.ttc",
            "Arial.ttf",
            "Menlo.ttc",
            "Monaco.ttf",
            nullptr
        };
        
        for (int j = 0; fallbacks[j] != nullptr; ++j) {
            std::string fullPath = dir + fallbacks[j];
            std::ifstream testFile(fullPath);
            if (testFile.good()) {
                testFile.close();
                return fullPath;
            }
        }
    }
    
#elif defined(_WIN32)
    // Windows font locations
    const char* fontDirs[] = {
        "C:\\Windows\\Fonts\\",
        nullptr
    };
    
    std::string fontName = preferredFont;
    if (fontName == "Arial") {
        fontName = "arial.ttf";
    } else if (fontName.find('.') == std::string::npos) {
        fontName += ".ttf";
    }
    
    for (int i = 0; fontDirs[i] != nullptr; ++i) {
        std::string fullPath = std::string(fontDirs[i]) + fontName;
        std::ifstream testFile(fullPath);
        if (testFile.good()) {
            testFile.close();
            return fullPath;
        }
    }
    
#elif defined(__linux__)
    // Linux font locations (check subdirectories too)
    const char* fontDirs[] = {
        "/usr/share/fonts/truetype/",
        "/usr/share/fonts/TTF/",
        "/usr/share/fonts/Adwaita/",
        "/usr/share/fonts/noto/",
        "/usr/share/fonts/liberation/",
        "/usr/share/fonts/",
        "~/.fonts/",
        "~/.local/share/fonts/",
        nullptr
    };
    
    std::string fontName = preferredFont;
    if (fontName.find('.') == std::string::npos) {
        fontName += ".ttf";
    }
    
    // Try preferred font first
    for (int i = 0; fontDirs[i] != nullptr; ++i) {
        std::string dir(fontDirs[i]);
        if (dir[0] == '~') {
            const char* home = getenv("HOME");
            if (home) {
                dir = std::string(home) + dir.substr(1);
            } else {
                continue;
            }
        }
        
        std::string fullPath = dir + fontName;
        std::ifstream testFile(fullPath);
        if (testFile.good()) {
            testFile.close();
            return fullPath;
        }
        
        // Try with .otf extension
        std::string otfPath = dir + preferredFont + ".otf";
        testFile.open(otfPath);
        if (testFile.good()) {
            testFile.close();
            return otfPath;
        }
    }
    
    // Fallback fonts common on Linux systems
    const char* fallbacks[] = {
        "DejaVuSans.ttf",
        "DejaVuSans-Bold.ttf",
        "LiberationSans-Regular.ttf",
        "NotoSans-Regular.ttf",
        "AdwaitaSans-Regular.ttf",
        "Ubuntu-Regular.ttf",
        "Roboto-Regular.ttf",
        nullptr
    };
    
    for (int i = 0; fontDirs[i] != nullptr; ++i) {
        std::string dir(fontDirs[i]);
        if (dir[0] == '~') {
            const char* home = getenv("HOME");
            if (home) {
                dir = std::string(home) + dir.substr(1);
            } else {
                continue;
            }
        }
        
        for (int j = 0; fallbacks[j] != nullptr; ++j) {
            std::string fullPath = dir + fallbacks[j];
            std::ifstream testFile(fullPath);
            if (testFile.good()) {
                testFile.close();
                return fullPath;
            }
        }
    }
#endif
    
    // No font found
    return "";
}

