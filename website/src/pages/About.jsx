import "../App.css"

import Layout from "../components/Layout"

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

            <p>
                The goal is to create gameplay that is:
                <br></br>
                responsive,
                competitive,
                expressive,
                physical,
                and satisfying.
            </p>

            <p>
                MiMITA is inspired by games where simple systems create
                deep emergent gameplay and weird mechanics naturally.
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

            <p>
                Ideally,
                people create here because they want to,
                not because they are forced to survive.
            </p>

            </div>
        </div>

    </Layout>
  )
}