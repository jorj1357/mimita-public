// No longer generates articles JSON — the Express server handles that.
// Keeping this as a minimal stub in case anything imports virtual:articles-data.
const VIRTUAL_MODULE_ID = "virtual:articles-data"
const RESOLVED_VIRTUAL_ID = "\0" + VIRTUAL_MODULE_ID

export default function vitePluginArticles() {
  return {
    name: "vite-plugin-articles",

    resolveId(id) {
      if (id === VIRTUAL_MODULE_ID) return RESOLVED_VIRTUAL_ID
    },

    load(id) {
      if (id !== RESOLVED_VIRTUAL_ID) return
      return `export const articles = []`
    },
  }
}
