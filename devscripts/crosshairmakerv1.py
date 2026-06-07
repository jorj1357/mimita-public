from PIL import Image, ImageDraw

SIZE = 100

img = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
draw = ImageDraw.Draw(img)

center = SIZE // 2

# bar_length = 40
bar_length = 15
bar_width = 2
gap = 8

# white, ready to shoot
# color = (255, 255, 255, 255)

# red, just shot, cant shoot
# color = (255, 0, 0, 255)

# blue, reloading
color = (0, 0, 255, 255)

# top
draw.rectangle([
    center - bar_width // 2,
    center - gap - bar_length,
    center + bar_width // 2,
    center - gap
], fill=color)

# bottom
draw.rectangle([
    center - bar_width // 2,
    center + gap,
    center + bar_width // 2,
    center + gap + bar_length
], fill=color)

# left
draw.rectangle([
    center - gap - bar_length,
    center - bar_width // 2,
    center - gap,
    center + bar_width // 2
], fill=color)

# right
draw.rectangle([
    center + gap,
    center - bar_width // 2,
    center + gap + bar_length,
    center + bar_width // 2
], fill=color)

img.save("crosshair.png")

print("saved crosshair.png")