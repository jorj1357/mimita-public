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
            only AFTER you answer the first-launch popup.
            Analytics can be disabled permanently(!!!!!)
            from that popup,
            the in-game settings menu,
            or the terminal command
            <code> analytics_disable </code>.
          </p>

          <p>
            When enabled, mimita.exe records aggregate, non-identifying(!!!!!) information such as
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
            the reason i even do this is bc it helps figure out 
            what bugs are ruining ur guys' experience, 
            like asynchronously. So u dont have to 
            seek me out + send bug reports + time zone mismatch and stuff. I can just fix it. Better yet YOU can fix it bc its open source heh.
          </p>

          <p>
            We do not collect passwords,
            chat contents,
            auth tokens,
            names,
            or sensitive free-form information
            through gameplay analytics.
            CUZ LIKE WHO Cares like we dont 
            need that
            the only thing i can think of  being an exception is 
            like if something genuine illegal happened and need chat or other logs 
            but even that we cant provide BC WE DONT TRACK it. Ugh...
          </p>

          <p>
            You can request deletion of analytics data
            from the in-game settings menu
            ("Request Data Deletion")
            or with the terminal command
            <code> analytics_request_delete </code>.
            
            8 12 2026 i don't like how this says request i rather it just say u can straight up delete it
            i really dont like surveillance and 99999 page privacy policies that say Um actually we r gonna 
            loko thru ur entire phone

            LIKE DO YALL Rmember when 
            tiktok
            it would say "TikTok accessed your clipboard."
            for no reason
            like waht do u need clipboard for bro
            i do NOT wanna do that Hellllllllll naw
            idc about data or Muh Advertising or Muh Profit  But but but data is so helpful for advertising!!!!
            advertising purpose = more revenue ?
            mimita purpose = more fun 
            if smth is making it less fun, i.e. 
            im getting my entire everything snooped and tracked 
            and sold so i can get profited off of 
            then no i dont wanna do that 
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

            google analytics i use , bc actual analytics tracking we have isnt super good yet
            prob remind me at hello@mimita.fun like 
            1. put a How To de-google your life guide here 
            and 2. if there ever is a open source analytics provider, use that one instead 
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
            and the MiMITA website API and ICE coordinator (virtual private server!!!! thank u VPS),
            each with their own privacy policies.
          </p>

          {/* <p>
            The source version of this policy is maintained in the repository:
            docs/privacy-policy.md
            8 12 2026 it 
          </p> */}

        </div>

      </div>

    </Layout>
  )
}
