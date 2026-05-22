// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-scale.h
// mar 8 2026
/**
 * purpose
 * makes it so WE CAN SCALE
 * TE GUII
 * WITH ALL WINDOW SIZES
 * THIS IS AWESOME
 * AND GOOD
 * ALSO
 * exposes ONE fcuntion
 * guiGetScale(args)
 * thats IT
 * other files just call this function
 */

#include <GLFW/glfw3.h>
#include <cstdio>

// maibe define these in config.h but idk  mar 8 2026
static const float UI_BASE_WIDTH  = 1920.0f;
static const float UI_BASE_HEIGHT = 1080.0f;

struct GuiScale
{
    float scaleX;
    float scaleY;
};

GuiScale guiGetScale(GLFWwindow* win)
{
    printf("[GUI SCALE] begin\n");

    if (!win)
    {
        printf("[GUI SCALE] ERROR: window pointer is null\n");
        GuiScale s{1.0f,1.0f};
        return s;
    }

    printf("[GUI SCALE] window ptr: %p\n", (void*)win);

    int w,h;
    glfwGetWindowSize(win,&w,&h);

    printf("[GUI SCALE] window size: %d %d\n", w, h);

    printf("[GUI SCALE] base UI: %.0f %.0f\n",
           UI_BASE_WIDTH,
           UI_BASE_HEIGHT);

    GuiScale s;

    s.scaleX = (float)w / UI_BASE_WIDTH;
    s.scaleY = (float)h / UI_BASE_HEIGHT;

    printf("[GUI SCALE] computed scaleX=%f scaleY=%f\n",
           s.scaleX,
           s.scaleY);

    printf("[GUI SCALE] end\n");

    return s;
}