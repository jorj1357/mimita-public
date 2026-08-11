import "../App.css"

import Layout from "../components/Layout"

export default function Uninstall() {
  return (
    <Layout>

      <div className="termsPage">

        <h1 className="termsTitle">
          UNINSTALLING MIMITA
        </h1>

        <div className="termsContent">

          <p>
            MiMITA is a portable application.
            It does not install anything into the Windows registry
            or Program Files.
            Everything lives under
            <code> %LOCALAPPDATA%\MiMITA </code>.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "1.5rem" }}>1. EXIT MIMITA</h2>

          <p>
            Close the game window,
            and right-click the MiMITA tray icon and choose Exit.
            Make sure no
            <code> mimita.exe </code>
            or
            <code> MimitaLauncher.exe </code>
            process is running in Task Manager.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "1.5rem" }}>2. REMOVE SHORTCUTS (OPTIONAL)</h2>

          <p>
            Delete the MiMITA shortcuts
            from the Start Menu
            and your desktop
            if you created them during installation.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "1.5rem" }}>3. DELETE THE MIMITA FOLDER</h2>

          <p>
            In File Explorer, type
            <code> %LOCALAPPDATA%\MiMITA </code>
            into the address bar and delete the folder.
            It contains the launcher,
            installed game versions,
            your settings, logs, and saved replays.
            Back these up first if you want to keep them.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "1.5rem" }}>4. VERIFY REMOVAL</h2>

          <p>
            Confirm the
            <code> %LOCALAPPDATA%\MiMITA </code>
            folder no longer exists.
            No uninstaller entry is registered in Settings &gt; Apps.
          </p>

          <p>
            Deleting the folder does not delete your MiMITA website account —
            manage that at mimita.fun.
            Analytics deletion can be requested separately
            from the in-game settings menu.
          </p>

          <p>
            The source version of this guide is maintained in the repository:
            docs/uninstall.md
          </p>

        </div>

      </div>

    </Layout>
  )
}
