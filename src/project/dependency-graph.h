// 09 12 2026
/* purpose
* Incremental dependency graph built from compiler-generated .d files.
* Maps a translation unit to its headers and lets a change compute the affected
* source set. No hand-maintained dependency list.
* Does NOT own builds, compilation, or the project tree.
*/
#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Project {

class DependencyGraph {
public:
    void clear();

    // Parses a compiler .d file (Makefile syntax with \ continuations).
    bool addCompilerDepFile(const std::filesystem::path& depFile);

    // Explicitly add a rule: unit depends on `dependencies`.
    void addRule(const std::string& unit, const std::vector<std::string>& dependencies);

    // Units that must be rebuilt when `changed` changes (includes the changed
    // units themselves when they are known units).
    std::set<std::string> affected(const std::vector<std::string>& changed) const;

    const std::map<std::string, std::set<std::string>>& forward() const { return forward_; }
    const std::map<std::string, std::set<std::string>>& reverse() const { return reverse_; }

    static bool isSourceFile(const std::string& path);

private:
    std::map<std::string, std::set<std::string>> forward_;  // unit -> dependencies
    std::map<std::string, std::set<std::string>> reverse_;  // dependency -> units
};

} // namespace Project
