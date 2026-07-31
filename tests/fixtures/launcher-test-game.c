// 07 31 2026, 14 00
/* purpose
* Minimal 64-bit Windows test executable used as a fake mimita.exe in
* launcher integration tests.
* Writes test-game-launched.txt into its working directory and exits 0.
* Does NOT open a window, connect to a network, or touch real game data.
* Does NOT serve as a game, installer, or launcher.
*/
#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    FILE* f = fopen("test-game-launched.txt", "w");
    if (f) {
        fprintf(f, "launched\n");
        fclose(f);
    }
    return 0;
}
