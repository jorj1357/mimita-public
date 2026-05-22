import "../App.css"

import Layout from "../components/Layout"

export default function About() {
  return (
    <Layout>

      <div className="aboutPage">

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
                and making things that feel genuinely fun.
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
                MiMITA should become a place people genuinely want to:
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
                people create here because they genuinely want to,
                not because they are forced to survive.
            </p>

            <p>
                make something genuinely fun first
                <br></br>
                then keep building outward from there.
            </p>

            </div>
        </div>

    </Layout>
  )
}