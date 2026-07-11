import { useState, useEffect } from "react"
import { useParams, Link } from "react-router-dom"
import Markdown from "react-markdown"
import remarkGfm from "remark-gfm"

import rehypeRaw from "rehype-raw"

import Layout from "../components/Layout"

function renderContent(md) {
  if (!md) return ""
  let result = md.replace(/\[rainbow\](.*?)\[\/rainbow\]/gs,
    '<span class="rainbow-text">$1</span>')
  const codeBlocks = []
  result = result.replace(/```[\s\S]*?```/g, m => {
    codeBlocks.push(m)
    return `\x00CB${codeBlocks.length - 1}\x00`
  })
  result = result.replace(/\n/g, '<br>\n')
  result = result.replace(/\x00CB(\d+)\x00/g, (_, i) => codeBlocks[parseInt(i)])
  return result
}

export default function ArticlePage() {

  const { slug } = useParams()
  const [article, setArticle] = useState(null)
  const [loading, setLoading] = useState(true)
  const [notFound, setNotFound] = useState(false)

  useEffect(() => {

    // Import markdown content via the virtual module
    async function load() {

      try {

        const res = await fetch("/articles.generated.json")
        if (!res.ok) throw new Error("Failed to load")

        const all = await res.json()
        const found = all.find(a => a.slug === slug)

        if (!found) {
          setNotFound(true)
          setLoading(false)
          return
        }

        // Content is included directly in the JSON
        setArticle(found)

        setLoading(false)

      } catch (err) {

        console.error("Article load error:", err)
        setNotFound(true)
        setLoading(false)

      }

    }

    load()

  }, [slug])

  if (loading) {
    return (
      <Layout>
        <div className="articlePage">
          <p className="aboutSmall">loading...</p>
        </div>
      </Layout>
    )
  }

  if (notFound || !article) {
    return (
      <Layout>
        <div className="articlePage"
          style={{
            minHeight: "70vh",
            display: "flex",
            flexDirection: "column",
            justifyContent: "center",
            alignItems: "center",
            textAlign: "center",
            gap: "20px"
          }}
        >
          <h1 className="socialsTitle">
            article not found
          </h1>
          <p className="aboutSmall">
            no article with that slug exists.
          </p>
          <Link to="/articles" className="bigLink">
            browse articles
          </Link>
          <Link to="/" className="bigLink">
            go home
          </Link>
        </div>
      </Layout>
    )
  }

  return (
    <Layout>

      <article className="articlePage">

        <Link to="/articles" className="articleBackLink">
          &larr; back to articles
        </Link>

        <header className="articleHeader">

          <h1 className="articlePageTitle">
            {article.title}
          </h1>

          <div className="articleMeta">

            {article.date && (
              <span className="articleMetaDate">
                {article.date}
              </span>
            )}

            {article.author && (
              <span className="articleMetaAuthor">
                {article.author}
              </span>
            )}

          </div>

          {article.tags && article.tags.length > 0 && (
            <div className="articleMetaTags">
              {article.tags.map(tag => (
                <span key={tag} className="articleTag">
                  {tag}
                </span>
              ))}
            </div>
          )}

        </header>

        <div className="articleContent">
          <Markdown remarkPlugins={[remarkGfm]} rehypePlugins={[rehypeRaw]}>
            {renderContent(article.content)}
          </Markdown>
        </div>

      </article>

    </Layout>
  )
}
