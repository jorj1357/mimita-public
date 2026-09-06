import { useState, useEffect } from "react"
import { Link } from "react-router-dom"

import Layout from "../components/Layout"

export default function ArticlesIndex() {

  const [articles, setArticles] = useState([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState(null)

  useEffect(() => {

    fetch("/articles.generated.json")
      .then(res => {

        if (!res.ok) throw new Error("Failed to load articles")
        return res.json()

      })
      .then(data => {

        setArticles(data)
        setLoading(false)

      })
      .catch(err => {

        setError(err.message)
        setLoading(false)

      })

  }, [])

  if (loading) {
    return (
      <Layout>
        <div className="articlesPage">
          <p className="aboutSmall">loading articles...</p>
        </div>
      </Layout>
    )
  }

  if (error) {
    return (
      <Layout>
        <div className="articlesPage">
          <h1 className="articlesTitle">articles</h1>
          <p className="aboutSmall" style={{ color: "#ff6666" }}>
            could not load articles: {error}
          </p>
        </div>
      </Layout>
    )
  }

  if (articles.length === 0) {
    return (
      <Layout>
        <div className="articlesPage">
          <h1 className="articlesTitle">articles</h1>
          <p className="aboutSmall">no articles yet. check back soon.</p>
        </div>
      </Layout>
    )
  }

  return (
    <Layout>

      <div className="articlesPage">

        <h1 className="articlesTitle">articles</h1>

        <p className="aboutSmall" style={{ marginBottom: "32px" }}>
          Have You  ever played R  MiMITA
        </p>

        <div className="articlesList">

          {articles.map(article => (

            <Link
              key={article.slug}
              to={`/articles/${article.slug}`}
              className="articleCard"
            >

              <h2 className="articleCardTitle">
                {article.title}
              </h2>

              {article.description && (
                <p className="articleCardDesc">
                  {article.description}
                </p>
              )}

              <div className="articleCardMeta">

                {article.date && (
                  <span className="articleCardDate">
                    {article.date}
                  </span>
                )}

                {article.author && (
                  <span className="articleCardAuthor">
                    {article.author}
                  </span>
                )}

              </div>

              {article.tags && article.tags.length > 0 && (
                <div className="articleCardTags">
                  {article.tags.map(tag => (
                    <span key={tag} className="articleTag">
                      {tag}
                    </span>
                  ))}
                </div>
              )}

            </Link>

          ))}

        </div>

      </div>

    </Layout>
  )
}
