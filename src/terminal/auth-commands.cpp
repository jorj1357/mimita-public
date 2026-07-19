// 07 19 2026, 12 00
/* purpose
* Register terminal-based sign in and sign out commands.
* Drive username/email lookup, password submission, and account bootstrap loading.
* Update the local player name after account linking succeeds.
* DOES NOT save raw passwords or bypass backend authentication.
* DOES NOT render login menus or password fields.
* DOES NOT handle website reset-password or delete-account flows.
*/

#include "terminal/auth-commands.h"

#include "auth/auth-controller.h"
#include "auth/auth-system.h"
#include "auth/auth-token.h"
#include "devtools/terminal.h"
#include "entities/player.h"
#include "website/api-client.h"

#include <string>
#include <vector>

extern Player* gpPlayer;

namespace {

std::string gPendingIdentifier;
std::string gPendingUsername;
bool gAwaitingPassword = false;
bool gAwaitingSignoutConfirm = false;

std::string joinArgs(const std::vector<std::string>& args)
{
    std::string out;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i > 0) out += " ";
        out += args[i];
    }
    return out;
}

void clearPendingSignin()
{
    gPendingIdentifier.clear();
    gPendingUsername.clear();
    gAwaitingPassword = false;
}

void doSigninPassword(const std::string& password)
{
    Terminal& term = Terminal::instance();
    if (!gAwaitingPassword || gPendingIdentifier.empty())
    {
        term.addLog("[AUTH] Use: signin <username-or-email> first");
        return;
    }
    if (password.empty())
    {
        term.addLog("[AUTH] Use: signinpw <password>");
        return;
    }

    term.addLog("[AUTH] Checking password...");
    const std::string deviceId = loadOrCreateDeviceId();
    const std::string deviceName = getDeviceName();
    GameLoginResult login = gameLogin(gPendingIdentifier, password, true,
                                      deviceId, deviceName, "windows", "0.1.0");
    if (!login.ok)
    {
        term.addLog("[AUTH] Password incorrect or sign in failed: " + login.errorMessage);
        return;
    }

    GameBootstrap bootstrap = getGameBootstrap(login.accessToken);
    if (!bootstrap.valid)
    {
        term.addLog("[AUTH] Signed in, but account data failed to load.");
        return;
    }

    AuthController::instance().updateFromLoginResult(
        login.accountId,
        login.username,
        login.accessToken,
        login.refreshToken,
        true);
    AuthController::instance().runtime().state = AuthState::SignedIn;
    AuthSystem::instance().applyBootstrap(login.accessToken, bootstrap);
    if (gpPlayer)
        gpPlayer->username = AuthSystem::instance().displayName();

    term.addLog("[AUTH] Signed in as " + bootstrap.user.username);
    term.addLog("[AUTH] This exe is now linked to account id " + std::to_string(bootstrap.user.id));
    term.addLog("[AUTH] MMR: " + std::to_string(bootstrap.stats.currentMmr));
    clearPendingSignin();
}

void confirmSignout(bool yes)
{
    Terminal& term = Terminal::instance();
    if (!gAwaitingSignoutConfirm)
        return;

    gAwaitingSignoutConfirm = false;
    if (!yes)
    {
        term.addLog("[AUTH] Sign out cancelled.");
        return;
    }

    AuthController::instance().signOut();
    AuthSystem::instance().logout();
    if (gpPlayer)
        gpPlayer->username = AuthSystem::instance().displayName();
    clearPendingSignin();
    term.addLog("[AUTH] Signed out.");
}

}

void registerAuthCommands()
{
    Terminal::instance().registerCommand({
        "signin", "Begin terminal account sign in", "signin <username-or-email>",
        [](const std::vector<std::string>& args) {
            Terminal& term = Terminal::instance();
            if (args.empty())
            {
                term.addLog("[AUTH] Usage: signin <username-or-email>");
                return;
            }

            const std::string identifier = joinArgs(args);
            term.addLog("[AUTH] Looking up account...");
            GameAccountLookupResult lookup = gameLookupAccount(identifier);
            if (!lookup.ok)
            {
                term.addLog("[AUTH] Account lookup failed: " + lookup.errorMessage);
                return;
            }
            if (!lookup.exists)
            {
                term.addLog("[AUTH] User does not exist.");
                term.addLog("[AUTH] Create/reset/delete accounts on mimita.fun.");
                clearPendingSignin();
                return;
            }

            gPendingIdentifier = identifier;
            gPendingUsername = lookup.username;
            gAwaitingPassword = true;
            gAwaitingSignoutConfirm = false;
            term.addLog("[AUTH] Found user: " + lookup.username);
            term.addLog("[AUTH] Enter password with: signinpw <password>");
        },
        "07 19 2026",
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "signinpw", "Submit password for pending sign in", "signinpw <password>",
        [](const std::vector<std::string>& args) {
            doSigninPassword(joinArgs(args));
        },
        "07 19 2026",
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "signout", "Sign out of the linked account", "signout",
        [](const std::vector<std::string>&) {
            Terminal& term = Terminal::instance();
            gAwaitingSignoutConfirm = true;
            term.addLog("[AUTH] Are you sure? Type 1 for yes, 2 for no.");
        },
        "07 19 2026",
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "1", "Confirm pending terminal prompt", "1",
        [](const std::vector<std::string>&) {
            confirmSignout(true);
        },
        "07 19 2026",
        CommandCategory::Player
    });

    Terminal::instance().registerCommand({
        "2", "Cancel pending terminal prompt", "2",
        [](const std::vector<std::string>&) {
            confirmSignout(false);
        },
        "07 19 2026",
        CommandCategory::Player
    });
}
