#include "analytics/analytics-uploader.h"

#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace {

std::wstring widen(const std::string& text)
{
    if (text.empty())
        return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring out((size_t)count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(), count);
    return out;
}

bool postBody(const std::string& endpoint, const std::string& body)
{
    std::wstring url = widen(endpoint);
    URL_COMPONENTSW parts{};
    wchar_t host[256]{};
    wchar_t path[2048]{};
    wchar_t extra[2048]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = (DWORD)(sizeof(extra) / sizeof(extra[0]));

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        std::printf("[ANALYTICS] invalid endpoint: %s\n", endpoint.c_str());
        return false;
    }

    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    std::wstring hostName(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring requestPath(parts.lpszUrlPath, parts.dwUrlPathLength);
    requestPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (requestPath.empty())
        requestPath = L"/";

    HINTERNET session = WinHttpOpen(
        L"MimitaAnalytics/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session)
        return false;

    HINTERNET connect = WinHttpConnect(session, hostName.c_str(), parts.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connect,
        L"POST",
        requestPath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(
        request,
        headers,
        (DWORD)-1L,
        (LPVOID)body.data(),
        (DWORD)body.size(),
        (DWORD)body.size(),
        0);
    if (ok)
        ok = WinHttpReceiveResponse(request, nullptr);

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (ok) {
        WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX);
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok && status >= 200 && status < 300;
}

}

void AnalyticsUploader::enqueue(const nlohmann::json& event)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mQueue.size() >= 256)
        mQueue.erase(mQueue.begin());
    mQueue.push_back(event);
}

void AnalyticsUploader::update(float dt, const std::string& endpoint)
{
    mFlushTimer += dt;
    if (mFlushTimer < 10.0f)
        return;
    mFlushTimer = 0.0f;
    flush(endpoint, false);
}

void AnalyticsUploader::flush(const std::string& endpoint, bool blocking)
{
    std::vector<nlohmann::json> batch;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mQueue.empty())
            return;
        batch.swap(mQueue);
    }

    auto send = [endpoint, batch = std::move(batch)]() {
        nlohmann::json body;
        body["events"] = batch;
        const bool ok = AnalyticsUploader::postJson(endpoint, body);
        if (!ok)
            std::printf("[ANALYTICS] upload failed events=%zu\n", batch.size());
    };

    if (blocking)
        send();
    else
        std::thread(std::move(send)).detach();
}

bool AnalyticsUploader::postJson(const std::string& endpoint, const nlohmann::json& body)
{
    if (endpoint.empty())
        return false;
    return postBody(endpoint, body.dump());
}
