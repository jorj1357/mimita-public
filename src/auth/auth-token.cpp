#include "auth/auth-token.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <wincred.h>

#pragma comment(lib, "credui.lib")

namespace {

const char* TOKEN_PATH = "config/auth-token.json";
const char* CRED_TARGET = "MimitaAuthSession";
const char* CRED_TYPE_DESC = "Mimita Session Token";

bool storeCredentialManager(const std::string& token)
{
    bool result = false;
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    wchar_t targetName[] = L"MimitaAuthSession";
    cred.TargetName = targetName;
    cred.CredentialBlobSize = (DWORD)token.size();
    cred.CredentialBlob = (BYTE*)token.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    wchar_t userName[] = L"MimitaUser";
    cred.UserName = userName;
    if (CredWriteW(&cred, 0))
    {
        printf("[AUTH] session token stored in Credential Manager\n");
        result = true;
    }
    else
    {
        printf("[AUTH] Credential Manager write failed (error=%lu), falling back to file\n", GetLastError());
    }
    return result;
}

std::string loadCredentialManager()
{
    PCREDENTIALW cred = nullptr;
    if (CredReadW(L"MimitaAuthSession", CRED_TYPE_GENERIC, 0, &cred))
    {
        std::string token((const char*)cred->CredentialBlob, cred->CredentialBlobSize);
        CredFree(cred);
        return token;
    }
    return {};
}

void clearCredentialManager()
{
    if (CredDeleteW(L"MimitaAuthSession", CRED_TYPE_GENERIC, 0))
        printf("[AUTH] session token removed from Credential Manager\n");
}

}

bool storeSessionToken(const std::string& token)
{
    if (storeCredentialManager(token))
        return true;

    std::filesystem::create_directories("config");
    std::ofstream out(TOKEN_PATH, std::ios::trunc);
    if (!out)
    {
        printf("[AUTH] failed to write session token\n");
        return false;
    }
    out << "{\"session_token\":\"" << token << "\"}\n";
    printf("[AUTH] session token stored to file\n");
    return out.good();
}

std::string loadSessionToken()
{
    std::string token = loadCredentialManager();
    if (!token.empty())
        return token;

    std::ifstream in(TOKEN_PATH);
    if (!in)
        return {};

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    auto pos = content.find("\"session_token\":\"");
    if (pos == std::string::npos)
        return {};

    pos += 17;
    auto end = content.find('"', pos);
    if (end == std::string::npos)
        return {};

    return content.substr(pos, end - pos);
}

void clearSessionToken()
{
    clearCredentialManager();
    std::filesystem::remove(TOKEN_PATH);
    printf("[AUTH] session token cleared\n");
}
