import { useEffect, useState } from "react"
import { useParams, Link } from "react-router-dom"
import Layout from "../components/Layout"
import Avatar from "../components/Avatar"
import Username from "../components/Username"
import { apiRequest } from "../lib/api"

export default function Forum() {
    const [categories, setCategories] = useState([])
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        apiRequest("/api/forum/categories")
            .then(data => {
                if (data?.success) setCategories(data.categories || [])
            })
            .catch(() => {})
            .finally(() => setLoading(false))
    }, [])

    return (
        <Layout>
            <section className="forumPage">
                <div className="forumHeader">
                    <h1 className="pageHeading">Forum</h1>
                    <p className="forumSubtitle">Community discussion</p>
                </div>
                {loading ? (
                    <p>Loading categories...</p>
                ) : (
                    <div className="forumCategories">
                        {categories.map(cat => (
                            <Link
                                key={cat.id}
                                to={`/forum/${cat.slug}`}
                                className="forumCategoryCard"
                            >
                                <h2 className="forumCategoryName">{cat.name}</h2>
                                <p className="forumCategoryDesc">{cat.description}</p>
                                <span className="forumCategoryCount">
                                    {cat.thread_count} {cat.thread_count === 1 ? "thread" : "threads"}
                                </span>
                            </Link>
                        ))}
                    </div>
                )}
            </section>
        </Layout>
    )
}
