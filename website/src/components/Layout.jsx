import Header from "./Header"
import Footer from "./Footer"

export default function Layout({ children }) {
  return (
    <div className="app">

      <Header />

      <main className="pageContent">
        {children}
      </main>

      <Footer />

    </div>
  )
}