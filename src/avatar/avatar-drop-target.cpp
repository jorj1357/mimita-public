#include "avatar-editor.h"
#include "avatar.h"
#include "avatar-drop-target.h"

#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <Windows.h>
#include <shellapi.h>
#include <ole2.h>
#include <shlobj.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// Global hover state visible to the UI
std::string gDropHoverPath;
bool gDropHoverActive = false;

// ─── IDropTarget implementation ─────────────────────────────────────
class AvatarDropTarget : public IDropTarget {
    LONG mRefCount = 1;
public:
    HWND mHwnd = nullptr;

private:

public:
    AvatarDropTarget(HWND hwnd) : mHwnd(hwnd) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return InterlockedIncrement(&mRefCount);
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&mRefCount);
        if (r == 0) delete this;
        return r;
    }

    // IDropTarget
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState,
                                         POINTL pt, DWORD* pdwEffect) override
    {
        (void)grfKeyState;
        (void)pt;
        // Check if it's a file drop
        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        if (pDataObj->QueryGetData(&fmt) == S_OK) {
            *pdwEffect = DROPEFFECT_COPY;

            // Extract the first file path for hover display
            STGMEDIUM med = {};
            if (pDataObj->GetData(&fmt, &med) == S_OK) {
                HDROP hDrop = (HDROP)med.hGlobal;
                wchar_t pathBuf[MAX_PATH];
                if (DragQueryFileW(hDrop, 0, pathBuf, MAX_PATH) > 0) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, pathBuf, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        gDropHoverPath.resize(len);
                        WideCharToMultiByte(CP_UTF8, 0, pathBuf, -1, &gDropHoverPath[0], len, nullptr, nullptr);
                        gDropHoverPath.resize(len - 1); // remove null terminator
                    }
                }
                ReleaseStgMedium(&med);
            }
            gDropHoverActive = true;
            return S_OK;
        }
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        (void)grfKeyState;
        (void)pt;
        *pdwEffect = gDropHoverActive ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        gDropHoverActive = false;
        gDropHoverPath.clear();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState,
                                    POINTL pt, DWORD* pdwEffect) override
    {
        (void)grfKeyState;
        (void)pt;
        *pdwEffect = DROPEFFECT_NONE;
        gDropHoverActive = false;
        gDropHoverPath.clear();

        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM med = {};
        if (pDataObj->GetData(&fmt, &med) != S_OK)
            return S_OK;

        HDROP hDrop = (HDROP)med.hGlobal;
        UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

        std::vector<const char*> paths;
        std::vector<std::string> storage;
        for (UINT i = 0; i < fileCount; ++i) {
            wchar_t pathBuf[MAX_PATH];
            if (DragQueryFileW(hDrop, i, pathBuf, MAX_PATH) > 0) {
                int len = WideCharToMultiByte(CP_UTF8, 0, pathBuf, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    storage.emplace_back();
                    storage.back().resize(len);
                    WideCharToMultiByte(CP_UTF8, 0, pathBuf, -1, &storage.back()[0], len, nullptr, nullptr);
                    storage.back().resize(len - 1);
                    paths.push_back(storage.back().c_str());
                }
            }
        }

        ReleaseStgMedium(&med);

        // Route to the game's drop handler
        if (!paths.empty())
            avatarEditorHandleDrop((int)paths.size(), paths.data());

        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
    }
};

static AvatarDropTarget* gDropTarget = nullptr;

void initAvatarDropTarget(void* window)
{
    if (gDropTarget) return;
    HWND hwnd = glfwGetWin32Window((GLFWwindow*)window);
    if (!hwnd) {
        printf("[AVATAR DROP] Failed to get window handle\n");
        return;
    }
    OleInitialize(nullptr);
    gDropTarget = new AvatarDropTarget(hwnd);
    HRESULT hr = RegisterDragDrop(hwnd, gDropTarget);
    if (SUCCEEDED(hr))
        printf("[AVATAR DROP] Drag-drop registered (hover + drop)\n");
    else
        printf("[AVATAR DROP] RegisterDragDrop failed: 0x%08lx\n", hr);
}

void shutdownAvatarDropTarget()
{
    if (gDropTarget) {
        RevokeDragDrop(gDropTarget->mHwnd);
        gDropTarget->Release();
        gDropTarget = nullptr;
        OleUninitialize();
    }
}

const std::string& getDropHoverPath()
{
    return gDropHoverPath;
}

bool isDropHoverActive()
{
    return gDropHoverActive;
}
