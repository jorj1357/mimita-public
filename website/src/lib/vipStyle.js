// 08 06 2026, 16 45
/* purpose
* Shares VIP name-style labels and normalization used by the name-style editor and VIP pages.
* Keeps style constants in one place so the editor component stays a pure component file.
* DOES NOT render UI, contact Stripe, or mutate account state.
*/

export const STYLE_LABELS = {
    vip_turquoise: "VIP turquoise",
    rainbow: "Rainbow",
    solid: "Solid color",
    animated_rainbow: "Animated rainbow",
    per_letter: "Per-letter"
}

export function normalizeStyle(style) {
    return {
        version: 1,
        kind: style?.kind || "vip_turquoise",
        solid_color: style?.solid_color || "#40e0d0",
        colors: Array.isArray(style?.colors) && style.colors.length
            ? style.colors
            : ["#ff0044", "#ffcc00", "#00ff66", "#00ccff", "#9944ff"],
        rainbow_speed: Number(style?.rainbow_speed || 1),
        rainbow_direction: style?.rainbow_direction || "ltr",
        animation: style?.animation || "cycle"
    }
}
