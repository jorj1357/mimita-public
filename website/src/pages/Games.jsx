import { Link } from "react-router-dom"
import Layout from "../components/Layout"

const GAMES = [
    {
        id: "aim-test-v1",
        title: "aim-test-v1",
        subtitle: "How good is your aim...../?????? MOBILE AND PC WORKS",
        path: "/games/aim-test-v1",
    },
]

export default function Games() {
    return (
        <Layout>
            <div className="gamesPage">
                <div className="gamesInner">
                    <h1 className="gamesTitle">Games</h1>
                    <p className="gamesSubtitle">Freaky slenderman: Collect my pagesssssahhhhh👅👅👅
                        Freaky  mimita: Play my gamesssssahhhhhhh 👅👅👅👅👅</p>

                    <div className="gamesGrid">
                        {GAMES.map((game) => (
                            <Link key={game.id} to={game.path} className="gameCard">
                                <div className="gameCardThumb">
                                    <div className="gameCardThumbPlaceholder">
                                        <span className="gameCardThumbIcon">🎯</span>
                                    </div>
                                </div>
                                <div className="gameCardBody">
                                    <h2 className="gameCardTitle">{game.title}</h2>
                                    <p className="gameCardSubtitle">{game.subtitle}</p>
                                </div>
                            </Link>
                        ))}
                    </div>
                </div>
            </div>
        </Layout>
    )
}
