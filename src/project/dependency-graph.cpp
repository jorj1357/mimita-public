// 09 12 2026
/* purpose
* Implements the .d dependency graph and affected-set traversal.
* Does NOT own builds or compilation.
*/
#include "project/dependency-graph.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Project {

namespace {

std::string normalize(const std::string& path)
{
    std::string out = path;
    std::replace(out.begin(), out.end(), '\\', '/');
    while (out.rfind("./", 0) == 0)
        out.erase(0, 2);
    return out;
}

std::string extensionOf(const std::string& path)
{
    const std::size_t dot = path.find_last_of('.');
    return dot == std::string::npos ? std::string() : path.substr(dot);
}

} // namespace

bool DependencyGraph::isSourceFile(const std::string& path)
{
    const std::string ext = extensionOf(path);
    return ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c";
}

void DependencyGraph::clear()
{
    forward_.clear();
    reverse_.clear();
}

void DependencyGraph::addRule(const std::string& unit,
                              const std::vector<std::string>& dependencies)
{
    const std::string normalizedUnit = normalize(unit);
    for (const auto& dependency : dependencies) {
        const std::string normalizedDep = normalize(dependency);
        forward_[normalizedUnit].insert(normalizedDep);
        reverse_[normalizedDep].insert(normalizedUnit);
    }
}

bool DependencyGraph::addCompilerDepFile(const std::filesystem::path& depFile)
{
    std::ifstream in(depFile);
    if (!in.is_open())
        return false;

    std::string text;
    std::string line;
    while (std::getline(in, line)) {
        // Join Makefile line continuations.
        if (!line.empty() && line.back() == '\\')
            line.pop_back();
        text += line;
        text += ' ';
    }

    const std::size_t colon = text.find(':');
    if (colon == std::string::npos)
        return false;

    // The target is an object file; find the source among the dependencies.
    std::istringstream deps(text.substr(colon + 1));
    std::vector<std::string> tokens;
    std::string token;
    while (deps >> token)
        tokens.push_back(normalize(token));

    std::string unit;
    for (const auto& t : tokens) {
        if (isSourceFile(t)) {
            unit = t;
            break;
        }
    }
    if (unit.empty() && !tokens.empty())
        unit = tokens.front();
    if (unit.empty())
        return false;

    addRule(unit, tokens);
    return true;
}

std::set<std::string> DependencyGraph::affected(const std::vector<std::string>& changed) const
{
    std::set<std::string> result;
    std::vector<std::string> frontier;
    for (const auto& path : changed) {
        const std::string normalized = normalize(path);
        result.insert(normalized);
        frontier.push_back(normalized);
    }

    while (!frontier.empty()) {
        const std::string current = frontier.back();
        frontier.pop_back();
        auto it = reverse_.find(current);
        if (it == reverse_.end())
            continue;
        for (const auto& unit : it->second) {
            if (result.insert(unit).second)
                frontier.push_back(unit);
        }
    }
    return result;
}

} // namespace Project
