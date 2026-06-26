import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function About() {
  return (
    <Layout>
      <div className="aboutPage">
        <div className="aboutPageHeader">
          <video autoPlay loop muted playsInline preload="auto" className="aboutVideo">
            <source src="/untitled-loop-small.mp4" type="video/mp4" />
          </video>
          <h1 className="aboutPageTitle">ABOUT MiMITA</h1>
        </div>

        <div className="aboutPageContent">
          <section className="aboutSection">
            <h2>What is Mimita?</h2>
            <p>
              Mimita is an experimental movement-based game engine focused on
              emergence, editability, and making things that just feel fun.
              It is not a traditional game — it is a playground for movement,
              combat, creation, and expression.
            </p>
            <p>
              The engine is open-source, community-driven, and designed to be
              extended, modified, and reshaped by anyone who wants to build
              something with it.
            </p>
          </section>

          <section className="aboutSection">
            <h2>Movement &gt; Aim</h2>
            <p>
              Mimita is built around a core philosophy: movement matters more than aim.
            </p>
            <p>
              Instead of rewarding who can click the most accurately, Mimita
              rewards who can move most creatively. Full-body collision physics,
              momentum-based mechanics, and emergent movement tech create a
              combat system where positioning, timing, and flow matter more
              than precision aim.
            </p>
            <p>
              This philosophy extends to all systems — from combat to
              level design to game modes. Movement should feel expressive,
              responsive, and satisfying on its own.
            </p>
          </section>

          <section className="aboutSection">
            <h2>Donations Only — No Pay-to-Win</h2>
            <p>
              Mimita is supported entirely by donations. There are no
              microtransactions, no battle passes, no loot boxes, and no
              pay-to-win mechanics.
            </p>
            <p>
              Supporter perks are cosmetic only — custom colors, badges, and
              future cosmetics that let you show support without affecting
              gameplay. Everything gameplay-relevant is available to everyone.
            </p>
            <p>
              Financial contributions go toward server infrastructure,
              development tools, and keeping the project open and accessible.
            </p>
          </section>

          <section className="aboutSection">
            <h2>Frequently Asked Questions</h2>
            <div className="aboutFaq">
              <div className="aboutFaqItem">
                <strong>Is Mimita free?</strong>
                <p>Yes. The game and engine are completely free. No purchases required.</p>
              </div>
              <div className="aboutFaqItem">
                <strong>Can I contribute to development?</strong>
                <p>Yes. The source code is on GitHub. Pull requests, bug reports, and ideas are welcome.</p>
              </div>
              <div className="aboutFaqItem">
                <strong>What platforms are supported?</strong>
                <p>Currently Windows 64-bit. Other platforms may be added in the future.</p>
              </div>
              <div className="aboutFaqItem">
                <strong>Is there a single-player mode?</strong>
                <p>Mimita is primarily a multiplayer PvP experience. NPC modes and practice tools are planned.</p>
              </div>
              <div className="aboutFaqItem">
                <strong>How do I report a bug?</strong>
                <p>Use the feedback form below or open an issue on GitHub.</p>
              </div>
            </div>
          </section>
        </div>
      </div>

      <FeedbackBox pageName="about" />
    </Layout>
  )
}
