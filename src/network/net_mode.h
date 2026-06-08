#pragma once

#include <string>

namespace MimitaNet {

struct LaunchOptions
{
    bool server = false;
    bool client = false;
    std::string connect = "127.0.0.1:1357";
    std::string name;
};

LaunchOptions parseLaunchOptions(int argc, char** argv);
void printLaunchUsage();
int runServer(const LaunchOptions& options);
int runClient(const LaunchOptions& options);

} // namespace MimitaNet
