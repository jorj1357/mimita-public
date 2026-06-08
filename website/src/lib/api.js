export async function apiRequest(path, options = {}) {
    const response = await fetch(path, {
        credentials: "include",
        ...options,
        headers: {
            "Content-Type": "application/json",
            ...options.headers
        }
    })

    const data = await response.json().catch(() => ({
        success: false,
        message: "invalid server response"
    }))

    if (!response.ok) {
        throw new Error(data.message || "request failed")
    }

    return data
}
