import "../App.css"
import Layout from "../components/Layout"
import PixelBox from "../components/PixelBox"

const faqs = [
  {
    q: "What is MiMITA?",
    a: "MiMITA is a fast-paced movement shooter built on a custom C++17 OpenGL engine. It focuses on full-body collision, precise physics, and deep movement mechanics — wall climbing, dashing, sliding, and more. Every body part collides with the world: head, torso, arms, legs, and even your equipped weapon."
  },
  {
    q: "How do I play?",
    a: "Download the game from the Download page, install, and launch. Use WASD to move, space to jump, shift to dash, and mouse to aim and shoot. The game features multiple modes including duel, bomb tag, and sandbox. You can also spawn NPCs to practice movement and aim."
  },
  {
    q: "How do I contribute?",
    a: "MiMITA is open source. You can contribute on GitHub, join the Discord to discuss development, or donate via PayPal to support the project. All contributions — code, art, sound, feedback — are welcome."
  },
  {
    q: "What is the main vision?",
    a: "MiMITA aims to be the best-feeling movement shooter with no shortcuts. Full-body collision, physics-driven gameplay, no capsules — real body parts collide. The engine is built from scratch for performance, targeting low-end hardware while delivering high-skill gameplay. The philosophy: performance first, simplicity, readability, debuggability, extensibility. Also 7 11 2026 i want to make creation of ANYTHING simple as think it = its made. And u can post it here or make ur own site etc. more power to common people is the goal basically, but it starts with making MiMTIA PvP game super duper awesome"
  },
  {
    q: "How do I link my account?",
    a: "Open the game, go to settings, and you will see a linking option. It will give you a 6-digit code. Go to the Link Account page on this website, enter the code, and your game account will be linked to your website account."
  },
  {
    q: "What are the password principles?",
    a: "Use a strong, unique password. MiMITA stores passwords using secure hashing. Never share your password with anyone. If you suspect your account has been compromised, change your password immediately in your account settings."
  }
]

export default function FAQ() {
  return (
    <Layout>
      <div className="aboutPage">
        <div className="aboutContent">
          <h1 className="aboutTitle">FAQ</h1>

          {faqs.map((faq, i) => (
            <PixelBox key={i} style={{ marginBottom: "1rem" }}>
              <h3 style={{ color: "#a020ff", marginBottom: "0.5rem" }}>{faq.q}</h3>
              <p style={{ color: "rgba(255,255,255,0.8)", lineHeight: 1.6, whiteSpace: "pre-line" }}>{faq.a}</p>
            </PixelBox>
          ))}
        </div>
      </div>
    </Layout>
  )
}
