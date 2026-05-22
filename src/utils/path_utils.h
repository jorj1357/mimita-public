#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <string>

// Get the directory where the executable is located
std::string getExecutableDirectory();

// Resolve an asset path relative to the executable directory
// Falls back to relative path if executable directory can't be determined
// jan 25 2026 this crashed teh entire whole ass game if i do it like this so idk - jorj 
// std::string resolveAssetPath(const std::string& relativePath);
std::string resolveAssetPath(std::string relativePath);


// Find a system font file (cross-platform)
// Returns empty string if no font found
std::string findSystemFont(const std::string& preferredFont = "Arial");

#endif // PATH_UTILS_H

