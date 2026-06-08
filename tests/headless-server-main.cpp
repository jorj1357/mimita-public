#include "network/net_mode.h"

int main()
{
    MimitaNet::LaunchOptions options;
    options.server = true;
    options.connect = "127.0.0.1:2357";
    return MimitaNet::runServer(options);
}
