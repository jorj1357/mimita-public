// 2026-11-05
/* purpose
* Register the native world-project browser commands used by the terminal,
* scenarios, and the in-game editor panel.
* Own the shared creator-project action used by both the Create screen and
* the terminal command aliases.
*/
#pragma once

#include <string>

void registerWorldProjectCommands();

// The project currently opened through the command/browser surface, or empty.
std::string currentWorldProjectId();

// Working map JSON path for the active project, or empty.
const std::string& currentWorldProjectMapPath();

// GLB base world the project is authored on, or empty.
const std::string& currentWorldProjectBaseMapPath();

// Forget the active creator project without deleting any files.
void clearCurrentWorldProject();

// Make an already-persisted project the active creator identity.
void activateWorldProject(const std::string& projectId,
                          const std::string& workingPath,
                          const std::string& baseMapPath);

struct CreatorProjectResult
{
    bool ok = false;
    std::string projectId;
    std::string mapId;
    std::string title;
    std::string workingPath;
    std::string initialVersion;
    std::string manifestHash;
    std::string baseMapPath;
    std::string floorId;
    std::string spawnId;
    std::string failedStep;
    std::string error;
};

// Create a new native project from a blank starter map document.
bool creatorProjectCreate(const std::string& projectId,
                          const std::string& title,
                          CreatorProjectResult& out);

// Create an independent project fork of an existing project.
bool creatorProjectClone(const std::string& sourceProjectId,
                         const std::string& projectId,
                         const std::string& title,
                         CreatorProjectResult& out);

// Save the live authored map as a new version of the active project and fork
// that version into a new project. Leaves the active live project unchanged.
bool creatorProjectForkLive(const std::string& projectId,
                            const std::string& title,
                            CreatorProjectResult& out);

// Load a project's working map into the live MapConfigRegistry and make it the
// active creator project. Does not load a world or activate the editor.
bool creatorProjectOpen(const std::string& projectId,
                        const std::string& version,
                        CreatorProjectResult& out);

// Log the locally stored creator projects to the terminal.
void creatorProjectListCommand();
