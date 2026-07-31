// 07 31 2026, 00 00
/* purpose
* Intentional-crash test: installs the crash handler, then raises an access
* violation on a worker thread. Verifies a nonzero .txt report and a valid
* nonzero .dmp minidump appear in %LOCALAPPDATA%\MiMITA\crashes.
* Does NOT open a window, launch the game, or require user interaction.
* Does NOT test networking, gameplay, or the launcher.
*/
#include "debug/crash-handler.h"
#include "tests/crash-test-common.h"

// Pull the crash-handler implementation into this TU (test-only).
#include "../src/debug/crash-handler.cpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

int main()
{
    installCrashHandler();
    setCrashHandlerTestMode(true);

    std::string crashDir = localAppData() + "\\MiMITA\\crashes";
    DWORD attr = GetFileAttributesA(crashDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        printf("FAIL: crash dir missing before crash: %s\n", crashDir.c_str());
        return 1;
    }

    // Intentional worker-thread access violation.
    std::thread t([]() {
        volatile int* p = nullptr;
        int x = *p; // ACCESS_VIOLATION on this thread
        (void)x;
    });
    t.join(); // The handler writes artifacts, then the process terminates.

    printf("FAIL: process did not terminate after worker-thread access violation\n");
    return 2;
}
