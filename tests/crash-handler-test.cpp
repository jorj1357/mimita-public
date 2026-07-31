// 07 31 2026, 00 00
/* purpose
* Deterministic tests for the crash handler's path building, directory
* creation, text report writing, minidump writing, and TEMP fallback.
* Compiles crash-handler.cpp in-process so internal helpers are testable.
* Does NOT launch mimita.exe, open a window, or trigger a real process crash.
* Does NOT test the launcher, networking, or gameplay systems.
*/
#include "debug/crash-handler.h"
#include "tests/crash-test-common.h"

// Pull the crash-handler implementation into this TU so the anonymous
// namespace helpers (buildCrashDir, writeTextReport, writeMinidump, etc.)
// are directly callable.
#include "../src/debug/crash-handler.cpp"

#include <cstdio>
#include <cstring>
#include <string>

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-58s ", name)
#define PASS() do { ++gPassed; printf("PASS\n"); } while (0)
#define FAIL(msg) do { ++gFailed; printf("FAIL: %s\n", msg); } while (0)

static bool dirExists(const std::string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Use a dedicated scratch directory under TEMP so tests never touch the real
// %LOCALAPPDATA%\MiMITA\crashes or game install.
static std::string scratchDir()
{
    char td[MAX_PATH];
    GetTempPathA(MAX_PATH, td);
    std::string dir = std::string(td) + "mimita-crash-test-" + std::to_string(GetCurrentProcessId());
    return dir;
}

static void wipeDir(const std::string& dir)
{
    std::string search = dir + "\\*";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            DeleteFileA((dir + "\\" + fd.cFileName).c_str());
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir.c_str());
}

// 1. Creating the nested crash directory from nothing.
static void testCreateNestedDir()
{
    TEST("create nested dir from nothing");
    std::string dir = scratchDir() + "\\A\\B\\crashes";
    char buf[MAX_PATH];
    strcpy(buf, dir.c_str());
    if (!createDirectoryRecursive(buf)) { FAIL("createDirectoryRecursive returned false"); return; }
    if (!dirExists(dir)) { FAIL("dir does not exist after create"); return; }
    RemoveDirectoryA((scratchDir() + "\\A\\B").c_str());
    RemoveDirectoryA((scratchDir() + "\\A").c_str());
    RemoveDirectoryA(scratchDir().c_str());
    PASS();
}

// 2. buildCrashDir produces %LOCALAPPDATA%\MiMITA\crashes (real path) and
//    creates it when missing.
static void testBuildCrashDirPrimary()
{
    TEST("buildCrashDir primary success");
    char buf[MAX_PATH];
    char exeDir[MAX_PATH] = "C:\\Mimita";
    if (!buildCrashDir(buf, sizeof(buf), exeDir)) { FAIL("buildCrashDir returned false"); return; }
    std::string expected = localAppData() + "\\MiMITA\\crashes";
    if (strcmp(buf, expected.c_str()) != 0) { FAIL(("path mismatch: " + std::string(buf)).c_str()); return; }
    if (!dirExists(expected)) { FAIL("crashes dir not created"); return; }
    PASS();
}

// 3. createDirectoryRecursive creates the missing parent chain for the real
//    crash dir and verifyDirWritable passes on it.
static void testVerifyDirWritable()
{
    TEST("verifyDirWritable on real crash dir");
    std::string dir = localAppData() + "\\MiMITA\\crashes";
    char buf[MAX_PATH];
    strcpy(buf, dir.c_str());
    createDirectoryRecursive(buf);
    if (!verifyDirWritable(dir.c_str())) { FAIL("probe write failed"); return; }
    PASS();
}

// 4. Text report produces a nonzero final .txt and no .tmp leftover.
static void testTextReportWrites()
{
    TEST("text report nonzero final size");
    std::string dir = scratchDir() + "\\crashes";
    char dbuf[MAX_PATH];
    strcpy(dbuf, dir.c_str());
    createDirectoryRecursive(dbuf);
    const char* prefix = "test-report";
    const char* report = "Crash Report\nLine2\n";
    CrashOpResult res;
    writeTextReport(dir.c_str(), prefix, report, res);
    if (!res.ok) { FAIL(("writeTextReport failed: " + std::string(res.failOp ? res.failOp : "?")).c_str()); return; }
    if (!fileHasData(dir + "\\test-report.txt")) { FAIL("final .txt missing or zero"); return; }
    if (GetFileAttributesA((dir + "\\test-report.txt.tmp").c_str()) != INVALID_FILE_ATTRIBUTES)
        { FAIL(".tmp file left behind"); return; }
    wipeDir(dir);
    RemoveDirectoryA(scratchDir().c_str());
    PASS();
}

// 5. Text report fails cleanly when directory does not exist (error 3 path).
static void testTextReportMissingDir()
{
    TEST("text report missing dir reports failure");
    std::string dir = scratchDir() + "\\does-not-exist\\crashes";
    const char* prefix = "test-report";
    CrashOpResult res;
    writeTextReport(dir.c_str(), prefix, "x", res);
    if (res.ok) { FAIL("writeTextReport should fail with missing dir"); return; }
    if (!res.failOp) { FAIL("no failOp recorded"); return; }
    if (GetFileAttributesA((dir + "\\test-report.txt").c_str()) != INVALID_FILE_ATTRIBUTES)
        { FAIL("final .txt created in missing dir"); return; }
    PASS();
}

// 6. Minidump produces a nonzero final .dmp.
static void testMinidumpWrites()
{
    TEST("minidump nonzero final size");
    std::string dir = scratchDir() + "\\crashes";
    char dbuf[MAX_PATH];
    strcpy(dbuf, dir.c_str());
    createDirectoryRecursive(dbuf);
    EXCEPTION_RECORD er{};
    er.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    er.ExceptionAddress = (void*)0x140000000;
    er.NumberParameters = 2;
    er.ExceptionInformation[0] = 0;
    er.ExceptionInformation[1] = 0x48;
    CONTEXT ctx{};
    EXCEPTION_POINTERS ep{ &er, &ctx };
    CrashOpResult res;
    writeMinidump(dir.c_str(), "test-dump", &ep, res);
    if (!res.ok) { FAIL(("writeMinidump failed: " + std::string(res.failOp ? res.failOp : "?")).c_str()); return; }
    if (!fileHasData(dir + "\\test-dump.dmp")) { FAIL("final .dmp missing or zero"); return; }
    if (GetFileAttributesA((dir + "\\test-dump.dmp.tmp").c_str()) != INVALID_FILE_ATTRIBUTES)
        { FAIL(".dmp.tmp file left behind"); return; }
    wipeDir(dir);
    RemoveDirectoryA(scratchDir().c_str());
    PASS();
}

// 7. Minidump fails cleanly on missing dir.
static void testMinidumpMissingDir()
{
    TEST("minidump missing dir reports failure");
    std::string dir = scratchDir() + "\\missing\\crashes";
    EXCEPTION_RECORD er{};
    er.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    er.ExceptionAddress = (void*)0x140000000;
    CONTEXT ctx{};
    EXCEPTION_POINTERS ep{ &er, &ctx };
    CrashOpResult res;
    writeMinidump(dir.c_str(), "test-dump", &ep, res);
    if (res.ok) { FAIL("writeMinidump should fail with missing dir"); return; }
    if (!res.failOp) { FAIL("no failOp recorded"); return; }
    PASS();
}

// 8. InstallCrashHandler creates the real crash dir at startup (no crash needed).
static void testInstallCreatesDir()
{
    TEST("installCrashHandler creates crash dir");
    installCrashHandler();
    std::string expected = localAppData() + "\\MiMITA\\crashes";
    if (!dirExists(expected)) { FAIL("crash dir not created at startup"); return; }
    PASS();
}

// 9. Force the crash handler through its TEMP fallback path and confirm it
//    lands on %TEMP%\MiMITA\crashes (no crash needed: call crashHandler).
static void testTempFallback()
{
    TEST("crash handler TEMP fallback dir");
    setCrashHandlerTestMode(true);
    setCrashHandlerForceTempFallback(true);
    EXCEPTION_RECORD er{};
    er.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    er.ExceptionAddress = (void*)0x140000000;
    er.NumberParameters = 2;
    er.ExceptionInformation[0] = 0;
    er.ExceptionInformation[1] = 0x48;
    CONTEXT ctx{};
    EXCEPTION_POINTERS ep{ &er, &ctx };
    // crashHandler writes report + minidump to the TEMP fallback dir.
    crashHandler(&ep);
    char td[MAX_PATH];
    GetTempPathA(MAX_PATH, td);
    std::string tempCrash = std::string(td) + "MiMITA\\crashes";
    if (!dirExists(tempCrash)) { FAIL("temp fallback dir missing"); return; }
    // Confirm a nonzero text report landed there.
    std::string search = tempCrash + "\\crash-*.txt";
    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(search.c_str(), &fd);
    bool found = false;
    if (h != INVALID_HANDLE_VALUE) {
        std::string p = tempCrash + "\\" + fd.cFileName;
        found = fileHasData(p);
        FindClose(h);
    }
    if (!found) { FAIL("no nonzero text report in temp fallback dir"); return; }
    wipeDir(tempCrash);
    RemoveDirectoryA((std::string(td) + "MiMITA").c_str());
    PASS();
}

int main()
{
    printf("=== Crash Handler Tests ===\n\n");
    testCreateNestedDir();
    testBuildCrashDirPrimary();
    testVerifyDirWritable();
    testTextReportWrites();
    testTextReportMissingDir();
    testMinidumpWrites();
    testMinidumpMissingDir();
    testInstallCreatesDir();
    testTempFallback();
    printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
    return gFailed > 0 ? 1 : 0;
}
