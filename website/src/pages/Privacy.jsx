import "../App.css"

import Layout from "../components/Layout"

export default function Privacy() {
  return (
    <Layout>

      <div className="termsPage">

        <h1 className="termsTitle">
          MiMITA PRIVACY POLICY
        </h1>

        <div className="termsContent">

          <p>
            Effective date: August 11, 2026.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "1.5rem" }}>THE GAME</h2>

          <p>
            MiMITA collects anonymous gameplay analytics
            only after you answer the first-launch popup.
            Analytics can be disabled permanently
            from that popup,
            the in-game settings menu,
            or the terminal command
            <code> analytics_disable </code>.
          </p>

          <p>
            When enabled, we record aggregate, non-identifying information such as
            session length,
            onboarding,
            maps,
            weapons,
            movement,
            feature use,
            crashes,
            disconnects,
            and retention.
            Events are uploaded in batches over HTTPS to mimita.fun.
          </p>

          <p>
            We do not collect passwords,
            chat contents,
            auth tokens,
            names,
            or sensitive free-form information
            through gameplay analytics.
          </p>

          <p>
            You can request deletion of analytics data
            from the in-game settings menu
            ("Request Data Deletion")
            or with the terminal command
            <code> analytics_request_delete </code>.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "2rem" }}>THE LAUNCHER</h2>

          <p>
            The launcher downloads and verifies game updates
            from the public GitHub repository
            using SHA-256 checksums.
            It does not upload personal information.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "2rem" }}>THE WEBSITE</h2>

          <p>
            The website may use infrastructure services
            such as hosting,
            email,
            newsletters,
            payments,
            and optional analytics tools.
          </p>

          <h2 style={{ color: "#a020ff", marginTop: "2rem" }}>NETWORK TRANSFERS</h2>

          <p>
            MiMITA will not transfer information to other networked systems
            unless specifically requested by the user
            or the person installing or operating it.
            All network activity is user-initiated:
            playing online,
            signing in,
            checking for updates,
            and (after explicit consent) submitting analytics.
          </p>

          <p>
            Using MiMITA may involve third parties
            including GitHub,
            SignPath.io / SignPath Foundation,
            and the MiMITA website API and ICE coordinator,
            each with their own privacy policies.
          </p>

          <p>
            The source version of this policy is maintained in the repository:
            docs/privacy-policy.md
          </p>

        </div>

      </div>

    </Layout>
  )
}
