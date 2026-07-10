import { useEffect, useState } from "react"

export default function DelayedRender({ children, delayMs = 200, placeholder = null }) {
  const [show, setShow] = useState(false)

  useEffect(() => {
    const id = setTimeout(() => setShow(true), delayMs)
    return () => clearTimeout(id)
  }, [delayMs])

  return show ? children : placeholder
}
