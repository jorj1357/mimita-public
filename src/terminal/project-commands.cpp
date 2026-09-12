// 09 12 2026
/* purpose
* Implements the live-project terminal commands.
* Does NOT own project logic.
*/
#include "terminal/project-commands.h"

#include "devtools/terminal.h"
#include "project/project-control.h"
#include "project/project-control-server.h"

#include <filesystem>
#include <string>
#include <vector>

void registerProjectCommands()
{
    Terminal::instance().registerCommand(
        {
            "project", "Inspect and mutate the live project version graph",
            "project [status|history|diff <a> <b>|record|undo|redo|checkpoint <label>|restore <hash>|serve [port]|stop]",
            [](const std::vector<std::string>& args) {
                if (args.empty()) {
                    Terminal::instance().addLog(
                        "usage: project [status|history|diff|record|undo|redo|checkpoint|restore|serve|stop]");
                    return;
                }
                if (args[0] == "serve") {
                    const std::uint16_t port =
                        args.size() > 1 ? (std::uint16_t)std::stoi(args[1]) : 7757;
                    if (Project::ControlServer::instance().start(port))
                        Terminal::instance().addLog("[PROJECT] control server on 127.0.0.1:" +
                                                    std::to_string(port));
                    else
                        Terminal::instance().addLog("[PROJECT] control server failed to start");
                    return;
                }
                if (args[0] == "stop") {
                    Project::ControlServer::instance().stop();
                    Terminal::instance().addLog("[PROJECT] control server stopped");
                    return;
                }

                std::string line;
                for (const auto& arg : args) {
                    if (!line.empty())
                        line += ' ';
                    line += arg;
                }
                const std::string response = Project::ProjectControl::instance().handle(line);
                Terminal::instance().addLog("[PROJECT] " + response);
            },
        },
        "2026-09-12", CommandCategory::Debug);
}
