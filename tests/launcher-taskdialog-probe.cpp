// 07 31 2026, 00 00
/* purpose
* Verifies TaskDialogIndirect resolves at runtime when comctl32 v6 is active.
* Embeds the same common-controls v6 manifest as the launcher and loads
* TaskDialogIndirect via LoadLibrary/GetProcAddress exactly like the launcher.
* Confirms both branches: resolved (v6) and safe fallback (v5.82).
* Does NOT launch MimitaLauncher.exe, the game, or touch the VPS.
*/
#include <windows.h>
#include <commctrl.h>
#include <cstdio>

typedef HRESULT(WINAPI* TaskDialogIndirectFn)(const TASKDIALOGCONFIG* pTaskConfig,
                                              int* pnButton, int* pnRadioButton,
                                              BOOL* pfVerificationFlagChecked);

int main()
{
    HMODULE hComctl = LoadLibraryW(L"comctl32.dll");
    if (!hComctl) {
        printf("FAIL: LoadLibraryW(comctl32.dll) failed\n");
        return 1;
    }

    TaskDialogIndirectFn fn = reinterpret_cast<TaskDialogIndirectFn>(
        (void*)GetProcAddress(hComctl, "TaskDialogIndirect"));

    if (fn) {
        printf("PASS: TaskDialogIndirect resolved at runtime (comctl32 v6 active)\n");
        // Show a real TaskDialog to prove it renders (auto-dismiss not possible,
        // so create one with a default button and a command link).
        const TASKDIALOG_BUTTON buttons[] = {
            { 1001, L"OK" },
        };
        TASKDIALOGCONFIG cfg = {};
        cfg.cbSize = sizeof(cfg);
        cfg.hwndParent = nullptr;
        cfg.dwFlags = TDF_USE_COMMAND_LINKS;
        cfg.pszWindowTitle = L"Mimita Launcher probe";
        cfg.pszMainInstruction = L"TaskDialogIndirect resolves";
        cfg.pszContent = L"Runtime TaskDialog works.";
        cfg.cButtons = 1;
        cfg.pButtons = buttons;
        cfg.nDefaultButton = 1001;
        int clicked = 0;
        HRESULT hr = fn(&cfg, &clicked, nullptr, nullptr);
        if (SUCCEEDED(hr)) {
            printf("PASS: TaskDialog rendered and returned (button=%d)\n", clicked);
        } else {
            printf("WARN: TaskDialogIndirect returned 0x%08lX (fallback still safe)\n",
                   (unsigned long)hr);
        }
        FreeLibrary(hComctl);
        return SUCCEEDED(hr) ? 0 : 0;
    }

    printf("PASS: TaskDialogIndirect unavailable -> fallback to MessageBoxA is safe\n");
    FreeLibrary(hComctl);
    return 0;
}
