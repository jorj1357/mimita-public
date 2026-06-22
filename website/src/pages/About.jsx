import "../App.css"

import Layout from "../components/Layout"
import FeedbackBox from "../components/FeedbackBox"

export default function About() {
  return (
    <Layout>

      <div className="aboutPage">

        <video
        autoPlay
        loop
        muted
        playsInline
        preload="auto"
        className="aboutVideo"
        >

        <source
            src="/untitled-loop-small.mp4"
            type="video/mp4"
        />

        </video>

        <h1 className="aboutPageTitle">
          ABOUT MiMITA
        </h1>


            <div className="aboutPageContent">

            <p>
                <span className="bold">MiMITA v1</span>
                <br></br>
                is an experimental game engine focused on
                <span className="highlight"> movement</span>,
                emergence,
                editability,
                and making things that just feel fun.
            </p>
                <br></br>
                
                                <br></br>
                                
                                <br></br>

            <p>
                The goal is to create gameplay that is:
                <br></br>
                responsive,
                competitive,
                expressive,
                physical,
                and satisfying.

                {/* <br></br>
                Simultaneously, keep the engine open-source and updated so others can use/adapt it to their needs */}
            </p>
                <br></br>
                
                                <br></br>

            <p>
                MiMITA is inspired by games where simple systems create
                deep emergent gameplay and weird mechanics naturally.
                <br></br>

                <br></br>
                Super Smash Brothers Melee is a great example of what is intended:
                <br></br>
                                <br></br>

                Wavedashing was never intended to be a mechanic, 
                                <br></br>

                but because of a short development time (~13 months!)
                                <br></br>

                and multiple systems interacting together,
                                <br></br>
                                <br></br>

                wavedashing was discovered, mastered, and became part of the appeal of SSBM.

                                <br></br>
                                <br></br>
                                <br></br>
                                <br></br>
                                <br></br>

                {/* and that's what MiMITA aims to do  */}
            </p>

            <p>
                Long-term,
                MiMITA should become a place people want to:
                <br></br>
                play,
                <br></br>
                create,
                <br></br>
                experiment,
                <br></br>
                and hang out in.
                
                                <br></br>
                                <br></br>

                Play should seamlessly flow into creation,
                and creation should flow seamlessly into play. Press a button and instantly go into editor mode.
  <br></br>
                                <br></br>

                MiMITA should also not stop at just the game engine.
                  <br></br>
                                <br></br>

                Music, sound effects, hardware, software, 2d art, 3d art, movies, shows, shorts...
                                
                                    <br></br>
                                                    <br></br>
                    I believe these should all flow together as well
                    <br></br>
                                <br></br>

                Play a game, auto clip a moment, add images/video overlays, auto upload/post in the background, continue playing

            </p>

            <p>
                The goal is to make creating things feel as fun and easy
                as possible:
                <br></br>
                games,
                <br></br>
                worlds,
                <br></br>
                software,
                <br></br>
                tools,
                <br></br>
                ideas,
                <br></br>
                experiments,
                <br></br>
                anything.
            </p>


                                <br></br>
                                <br></br>
            <p>
                And ideally,
                people create here because they want to,
                not because they are forced to survive.
                                  <br></br>
                                <br></br>

                Relates to UBI and economics (mimitabux/mimitacoin/etc)
            </p>



                                <br></br>
                                <br></br>
            <p>
                5 25 2026 - 
                hi
            </p>

            </div>
        </div>

        <FeedbackBox pageName="about" />

    </Layout>
  )
}