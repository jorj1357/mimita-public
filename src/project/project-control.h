// 09 12 2026
/* purpose
* Own the live project runtime (scan, record, history, undo/redo/checkpoint)
* and expose one command surface shared by terminal (humans) and a control
* socket (agents).
* Does NOT compile code or activate runtime generations.
*/
#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "project/project-history.h"

namespace Project {

class ProjectControl {
public:
    static ProjectControl& instance();

    // Initializes the project root and filter. Safe to call more than once.
    void init(const std::filesystem::path& root);
    bool ready() const;

    // Records any on-disk change as a new project version. Returns a one-line
    // human/agent summary.
    std::string record();

    bool undo();
    bool redo();
    bool checkpoint(const std::string& label);
    bool restore(const std::string& versionHash);

    std::string statusJson() const;
    std::string historyJson() const;
    std::string diffJson(const std::string& a, const std::string& b) const;

    // Generic command surface. Returns a text/JSON response. Commands:
    //   status | history | diff <a> <b> | record | undo | redo
    //   checkpoint <label> | restore <hash>
    std::string handle(const std::string& commandLine);

private:
    ProjectControl() = default;
    ProjectControl(const ProjectControl&) = delete;
    ProjectControl& operator=(const ProjectControl&) = delete;

    std::unique_ptr<ProjectHistory> history_;
    std::filesystem::path root_;
    TreeFilter filter_;
    std::string lastResponse_;
    bool initialized_ = false;
};

} // namespace Project
