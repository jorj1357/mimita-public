import fs from "fs"
import path from "path"
import matter from "gray-matter"

const ARTICLES_DIR = path.resolve(__dirname, "..", "content", "articles")
const VIRTUAL_MODULE_ID = "virtual:articles-data"
const RESOLVED_VIRTUAL_ID = "\0" + VIRTUAL_MODULE_ID

export default function vitePluginArticles() {

  const articlesMap = new Map()

  function loadArticles() {
    articlesMap.clear()

    if (!fs.existsSync(ARTICLES_DIR)) {
      return
    }

    const files = fs.readdirSync(ARTICLES_DIR)

    for (const file of files) {

      if (!file.endsWith(".md")) continue

      const filePath = path.join(ARTICLES_DIR, file)
      const slug = file.slice(0, -3)
      const raw = fs.readFileSync(filePath, "utf-8")
      const { data, content } = matter(raw)

      const title = data.title || slugToTitle(slug)
      const description = data.description || extractFirstParagraph(content) || ""
      const date = data.date || getFileDate(filePath)
      const author = data.author || ""
      const tags = data.tags || []
      const published = data.published !== false

      articlesMap.set(slug, {
        slug,
        title,
        description,
        date,
        author,
        tags,
        content,
        published,
        url: "/articles/" + slug,
      })
    }
  }

  function generateJson() {
    const articles = Array.from(articlesMap.values())
      .filter(a => a.published)
      .sort((a, b) => b.date.localeCompare(a.date))
      .map(({ published, ...rest }) => rest)

    const jsonPath = path.resolve("public/articles.generated.json")
    const dir = path.dirname(jsonPath)
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true })
    fs.writeFileSync(jsonPath, JSON.stringify(articles, null, 2))
  }

  return {
    name: "vite-plugin-articles",

    configResolved() {
      loadArticles()
    },

    buildStart() {
      loadArticles()
      generateJson()
    },

    configureServer(server) {
      loadArticles()

      // Watch for article changes
      if (fs.existsSync(ARTICLES_DIR)) {
        server.watcher.add(ARTICLES_DIR)
      }

      server.watcher.on("change", (changedPath) => {
        if (changedPath.startsWith(ARTICLES_DIR)) {
          loadArticles()
          generateJson()
          const mod = server.moduleGraph.getModuleById(RESOLVED_VIRTUAL_ID)
          if (mod) server.reloadModule(mod)
        }
      })

      // API endpoint for article data (includes content)
      server.middlewares.use((req, res, next) => {
        if (req.url === "/articles.json" || req.url === "/articles.generated.json") {
          res.setHeader("Content-Type", "application/json")
          const published = Array.from(articlesMap.values())
            .filter(a => a.published)
            .sort((a, b) => b.date.localeCompare(a.date))
            .map(({ published, ...rest }) => rest)
          res.end(JSON.stringify(published, null, 2))
          return
        }

        next()
      })
    },

    resolveId(id) {
      if (id === VIRTUAL_MODULE_ID) return RESOLVED_VIRTUAL_ID
    },

    load(id) {
      if (id !== RESOLVED_VIRTUAL_ID) return

      const articles = Array.from(articlesMap.entries())
        .map(([slug, data]) => ({
          slug: data.slug,
          title: data.title,
          description: data.description,
          date: data.date,
          author: data.author,
          tags: data.tags,
          content: data.content,
          published: data.published,
          url: data.url,
        }))

      return `export const articles = ${JSON.stringify(articles, null, 2)}`
    },
  }
}

function slugToTitle(slug) {
  return slug
    .split("-")
    .map(w => w.charAt(0).toUpperCase() + w.slice(1))
    .join(" ")
}

function extractFirstParagraph(md) {
  const match = md.match(/^(?!\s*#)(.+)$/m)
  return match ? match[1].trim().replace(/[#*`\[\]]/g, "").slice(0, 200) : ""
}

function getFileDate(filePath) {
  try {
    const stat = fs.statSync(filePath)
    const d = stat.mtime
    return d.toISOString().split("T")[0]
  } catch {
    return ""
  }
}
