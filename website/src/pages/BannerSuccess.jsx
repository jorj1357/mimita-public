import { Link } from "react-router-dom"
import Layout from "../components/Layout"
import "../styles/banner.css"

export default function BannerSuccess() {
    return (
        <Layout>
            <div className="bannerCreator">
                <h1 className="bannerCreatorTitle">payment complete</h1>
                <p>
                    your banner is on the way. if a banner is already showing, yours will go into the
                    queue and appear when the current slot frees up. your purchased time only starts
                    counting when your banner actually shows.
                </p>
                <p>
                    <Link to="/" style={{ color: "#40e0d0" }}>back to home</Link>
                    {" · "}
                    <Link to="/banner/create" style={{ color: "#40e0d0" }}>make another banner</Link>
                </p>
            </div>
        </Layout>
    )
}
