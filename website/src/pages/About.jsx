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

            <ChaosBox seed="FinancialPrinciples" as="section" className="aboutSection">
              <GrossHeading>Financial Principles</GrossHeading>

              <p>
                <strong>Money (doubloons.. bux.. Gold...) exists to keep MiMITA alive, growing, and accessible.</strong>
              </p>

              <p>
                It pays for the real-world things required to build something:
                <br></br>
                <br></br>
                • Servers
                <br></br>
                • Development tools , subscriptions to providers 
                <br></br>
                • Contributors (Could be you!!!!!)
                <br></br>
                • Infrastructure (anything from electricity, housing, food, mediicne, etc )
                <br></br>
                • The people who spend their time making MiMITA better!!!!!!!!
              </p>

              <p>
                Building MiMITA should also allow the people creating it to have a
                <strong> stable, healthy life.</strong>
              </p>

              <p>
                Creating something great requires:
                <br></br>
                <br></br>
                <strong>Time and Energy and some Resources</strong>
              </p>

              <p>
                A sustainable project lets people focus on creating instead of constantly
                worrying about survival.
              </p>

              <p>
               
                  Money is not!!!!!!!!!!!! the mission.
                  <br></br>
                  Money is the tool that helps the mission continue.
              </p>


              <GrossHeading>No Exploitation</GrossHeading>

              <p>
                <strong>
                  MiMITA will never optimize for making money at the expense of people.
                </strong>
              </p>

               <p>
                <strong>
                  MiMITA WILL NEVER  EVER  EVER EVER EVER EVER EVER EVER OPTIMIZE FOR MKAING MONEY AT EXPENSE OF PEOPL!!!!!!!!!!!
                </strong>
              </p>

              <p>
                No pay-to-win.
                <br></br>
                <br></br>
                No gambling mechanics. (UNLESS explained in clear, understandable detail, about addiction potential, real life consequences, etc.)
                <br></br>
                <br></br>
                No loot boxes designed around unhealthy spending.
                <br></br>
                <br></br>
                No annoying advertisements. and hwo even watches these. Adblock is everywerhe bro. 
                <br></br>
                <br></br>
                No fake buttons. like the fake X at the top right of mobile games DUDE I UGH
                <br></br>
                <br></br>
                No intentionally making things worse just to sell the solution. Setting walkspeed to 8, and selling a walkspeed 16 gamepass... no no no. Not good.
              </p>

              <p>
                If a decision makes MiMITA more profitable but makes the experience worse
                for players:
              </p>

              <p>
                <strong>
                  The priority is: the players.
                </strong>
              </p>


              <GrossHeading>Free!!!!!!!!!!!!!!!!! Core Experience</GrossHeading>

              <p>
                The core MiMITA experience should ALWAYS ALWS ALWAYS!!!!!!!!!!!!!1 be available to everyone.
              </p>

              <p>
                Playing, and
                <br></br>
                Practicing, and
                <br></br>
                Improving, and
                <br></br>
                Competing, and
                <br></br>
                Creating, and
                <br></br>
                Having FUN!!!!!!1
              </p>

              <p>
                These should never require spending money.
              </p>

              <p>
                Someone who spends <strong>$0</strong> on MiMITA should still receive the
                complete gameplay experience.
              </p>

              <p>
                Supporters can receive:
                <br></br>
                <br></br>
                • Cosmetics
                <br></br>
                • Special perks/features
                <br></br>
                • Customization
                <br></br>
                • Ways to show support
              </p>

              <p>
                But never:
                <br></br>
                <br></br>
                <strong>Gameplay advantages.</strong>
              </p>

              <p>
              
                  Money should add possibilities.
                  <br></br>
                  It should never NEVER EVER EVER EVR  remove fun that was intentionally taken away.
              
              </p>


              <GrossHeading>Creators Should Create</GrossHeading>

              <p>
                MiMITA is not meant to be a closed ecosystem where people only consume
                what already exists.
              </p>

              <p>
                It should be a tool that helps people create their own things.
              </p>

              <p>
                MiMITA should be like a hammer:
                <br></br>
                <br></br>
                A tool that helps you build your own ideas.
                <br></br>
                <br></br>
                Not a place where you are trapped inside someone else's system.
                <br></br>
                <br></br>
                I am not selling u a $19/mo hammer subscription. I am not making u read hammer terms of service with the 9725th page clause saying "btw um actually everything this hammer is used to build is the company's lol trolled."
              </p>

              <p>
                Long term, creators should be able to:
                <br></br>
                <br></br>
                • Build things
                <br></br>
                • Share things
                <br></br>
                • Receive support!!!!!!!!!!!!!!!!!
                <br></br>
                • Sell creations
                <br></br>
                • Earn from the value they create
                <br></br>
                • I WANT EVERY Human being to be able to wake up at 2:37 P.M. on a Monday with ABSOLUTELY nothing to do. Like the goal for that day is eat cereal and watch cartoons. Maybe draw or hang out with friends. The fundamentals like food water shelter healthcare medicine should be 100000000000000% taken care of.
              </p>


              <GrossHeading>Transparency</GrossHeading>

              <p>
                MiMITA should be open and understandable.
              </p>

              <p>
                Decisions should not happen behind a mysterious wall where people have no
                idea why something changed.
                "The company has deemed your conduct unreasonable" none of this business language. AND BTW Can we talk about that for a second bc like why. 
                  <br></br>
                <br></br>
                Like whats the point. Can u not instead say "because u didn't respond to customer emails fast enough, even after 3 warnings, we are firing you". 
                  <br></br>
                <br></br>
                I think honesty would help a lot lot more and so i am going to lead by example. 
                  <br></br>
                <br></br>
                AND U CAN QUOTE ME ON THIS If u ever catch me slipping.
              </p>

              <p>
                When possible:
                <br></br>
                <br></br>
                Development decisions.
                <br></br>
                Priorities.
                <br></br>
                Financial information.
                <br></br>
                <br></br>
                Should be visible.
              </p>

              <p>
                People should understand how , why, when , where, who, which, what MiMITA is being built.
              </p>


              <GrossHeading>The Bigger Goal</GrossHeading>

              <p>
                The dream is that someone, especially a young person, who feels like they
                are "just a kid," discovers MiMITA and realizes:
              </p>

              <p>
                <strong>
                  "Some random guy made this."
                  <br></br>
                  "And he sucked at making things for a long time."
                  <br></br>
                  "That means I can make things too."
                </strong>
              </p>

              <p>
                MiMITA should make people feel:
                <br></br>
                <br></br>
                Powerful
                <br></br>
                Creative
                <br></br>
                Capable
                <br></br>
                In control of their own lives
                <br></br>
                Not stuck, and not forced to do a path in life u don't want to
                <br></br>
              </p>

              <p>
                Not just like players inside a game.
              </p>

              <p>
                Like creators who can build their own worlds. Like a one person army.
              </p>

              <p>
                <strong>
                  The ultimate success is not just people playing MiMITA.
                  <br></br>
                  It is people using MiMITA to discover what they can create. 
                  <br></br>
                  Seeing how much they can push themselves. 
                  <br></br>
                  Seeing how they can start at the bottom and become a top ranked player. 
                  <br></br>
                  Seeing how far past their most ambitiious dreams they can go. 
                  <br></br>
                  Seeing how , yes, they really can do it, and always have been able to do it, regardless of upbringing, circumstances, failures, shortcomings, disabilities, mental health issues, past mistakes, anxieties, negative people, and anything else that makes them  think they are not full of epicness already.  because they are goated and aweosme.
                  <br></br>
                  <br></br>
                  because YOU are goated and awesome Vro.
                </strong>
              </p>
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
