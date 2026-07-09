import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"
import GrossHeading from "../components/GrossHeading"
import StickerLayer from "../components/StickerLayer"
import PixelBox from "../components/PixelBox"
import ProceduralOffset from "../components/ProceduralOffset"
import RandomRotation from "../components/RandomRotation"
import ChaosBox from "../components/ChaosBox"

export default function About() {
  return (
    <Layout>
      <div className="aboutPage">
        <StickerLayer count={7} />

        <div className="aboutPageHeader">
          <video autoPlay loop muted playsInline preload="auto" className="aboutVideo">
            <source src="/untitled-loop-small.mp4" type="video/mp4" />
          </video>
          <ChaosBox seed="AboutTitle">
            <RandomRotation maxDeg={1}>
              <h1 className="aboutPageTitle">ABOUT MiMITA</h1>
            </RandomRotation>
          </ChaosBox>
        </div>

        <div className="aboutPageContent">
          <ChaosBox seed="AboutSection1" as="section" className="aboutSection">
            <GrossHeading>What is Mimita?</GrossHeading>
            <p>
              Mimita is an experimental movement-based game engine focused on
              emergence, editability, and making things that just feel fun.
              It is not a traditional game....... it is a playground for movement,
              combat, creation, and expression.
            </p>
            <p>
              The engine is open-source, community-driven, and designed to be
              extended, modified, and reshaped by anyone who wants to build
              something with it.
            </p>
          </ChaosBox>

          <ChaosBox seed="AboutMovementBox">
            <PixelBox style={{ margin: "1.5rem 0", padding: "1rem" }}>
              <section className="aboutSection" style={{ borderBottom: "none", margin: 0, padding: 0 }}>
              <GrossHeading>Movement &gt; Aim</GrossHeading>
              <p>
                Mimita is built around a core philosophy:::::;;; movement matters more than aim.
              </p>
              <p>
                Instead of rewarding who can click the most accurately BC I SUCK AT AIMING

                <br></br>
                Mimita
                rewards who can move most creatively.
                Full-body collision physics, (arms and weapon collide with world + push u around)
                momentum-based mechanics, and emergent movement tech (Slopes  ahve never been so fun)
                create a
                combat system where positioning, timing, and flow matter more
                than precision aim.
              </p>
              <p>
                This philosophy extends to all systems.!!!?/!!!....... from combat to
                level design to game modes. Movement should feel expressive,
                responsive, and satisfying on its own.
              </p>
            </section>
          </PixelBox>
          </ChaosBox>

          <ChaosBox seed="AboutDonations" as="section" className="aboutSection">
            <GrossHeading>Donations Only&mdash; NO Pay-to-Win Chuddery here</GrossHeading>
            <p>
              Mimita is supported entirely by donations. There are no
              microtransactions, no battle passes, no loot boxes, and no
              pay-to-win mechanics.

              <br></br>
              like i might add those later but i lowk dont even  want to  especially loot boxes
              <br></br>
              cuz its  real life money i dont want to start addictions like that
            </p>
            <p>
              Supporter perks are cosmetic only;;;; custom colors, badges, and
              future cosmetics that let you show support without affecting
              gameplay. Everything gameplay-relevant is available to everyone ALL the time WITHOUT exception Always . Alwasy!!
            </p>
            <ProceduralOffset max={2}>
              <p>
                Financial contributions go toward server infrastructure,
                development tools, and keeping the project open and accessible.
                <br></br>
                Also  it hepls me to idk
                do more fun stuff
                like imagine there was a ctual money ppl could make from doing stuff here
                that would be so sick
                so idk need more thought  - 6 28 2026
              </p>
            </ProceduralOffset>
          </ChaosBox>

          <ChaosBox seed="AboutFAQ" as="section" className="aboutSection">
            <GrossHeading>Frequently Asked Questions</GrossHeading>
            <div className="aboutFaq">
              <PixelBox style={{ marginBottom: "0.75rem", padding: "0.75rem 1rem" }}>
                <div className="aboutFaqItem" style={{ border: "none", padding: 0, background: "none" }}>
                  <strong>Is Mimita free?</strong>
                  <p>Yes. !!!!!!!!!!!! The game and engine are completely free. Its all on github u can downlaod it clone it fork it whatever. No purchases required. I encourage u yes go clone it and mod it do whatever </p>
                </div>
              </PixelBox>
              <PixelBox style={{ marginBottom: "0.75rem", padding: "0.75rem 1rem" }}>
                <div className="aboutFaqItem" style={{ border: "none", padding: 0, background: "none" }}>
                  <strong>Can I contribute to development?</strong>
                  <p>Yes. T!!!!!!!!!!!he source code is on GitHub. Pull requests, bug reports, and ideas are welcome. And suggestions in mimita discord server its attttt https://discord.com/invite/sY8QHbfG9D</p>
                </div>
              </PixelBox>
              <PixelBox style={{ marginBottom: "0.75rem", padding: "0.75rem 1rem" }}>
                <div className="aboutFaqItem" style={{ border: "none", padding: 0, background: "none" }}>
                  <strong>What platforms are supported?</strong>
                  <p>Currently Windows 64-bit. Other platforms added in the future. DUDE i need this on my phone. Dude i need this on  my tv DUDE I NEED THIS ON PS5 ANX XBOX. I want to do that stuff SO  BADD THAT WOLD BE SO SICK</p>
                </div>
              </PixelBox>
              <PixelBox style={{ marginBottom: "0.75rem", padding: "0.75rem 1rem" }}>
                <div className="aboutFaqItem" style={{ border: "none", padding: 0, background: "none" }}>
                  <strong>Is there a single-player mode?</strong>
                  <p>YES!!!!! In coming time there should be madness combat modes
                    <br></br>
                    clear out this whole facility of grunts and then get the final boss of that level
                    <br></br>
                    new level = new boss new mehcanics etc fun stuff

                    <br></br>
                    ideally its a multiplayer PvP experience. NPC modes and practice tools are planned.</p>
                </div>
              </PixelBox>
              <PixelBox style={{ padding: "0.75rem 1rem" }}>
                <div className="aboutFaqItem" style={{ border: "none", padding: 0, background: "none" }}>
                  <strong>How do I report a bug?</strong>
                  <p>Use the feedback form below or open an issue on GitHub. Or on discord u can @MiMITA in any channel and say Hey bro this  thing is broken </p>
                </div>
              </PixelBox>
            </div>
              <p>also heres the mimita plan doc 7 9 2026 this is working but i do change docs alot hehe https://docs.google.com/document/d/1uTxOkciwKHtSzBm4rKxxMqMshNGttK9TOk8ju3uBu-E/edit?usp=sharing </p>

          </ChaosBox>
        </div>
      </div>

      <div className="wackySeparator" />

      <FeedbackBox pageName="about" />
    </Layout>
  )
}
