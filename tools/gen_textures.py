#!/usr/bin/env python3
# Genera la textura camo "3xor" del barril (PNG 2048x2048, RGBA).
# - exor_barrel_500_co.png -> letras BLANCAS
# Procedural (no usa assets del juego). Despues se convierte a .paa con
# ImageToPAA (ver build.ps1). Seed fija para que el resultado sea reproducible.
import os
import random
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SIZE = 2048
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")

# Paleta camo woodland
BASE = (62, 70, 48)
PALETTE = [
    (38, 48, 32),    # verde oscuro
    (88, 78, 52),    # marron/tan
    (28, 30, 24),    # casi negro
    (74, 86, 58),    # verde oliva claro
]

WHITE = (242, 242, 242, 255)
RED = (205, 32, 32, 255)


def blob(draw, cx, cy, r, color, rnd):
    """Mancha organica: union de elipses superpuestas alrededor de un centro."""
    for _ in range(rnd.randint(4, 7)):
        dx = rnd.randint(-r, r)
        dy = rnd.randint(-r, r)
        rw = rnd.randint(int(r * 0.5), int(r * 1.3))
        rh = rnd.randint(int(r * 0.4), int(r * 1.1))
        draw.ellipse([cx + dx - rw, cy + dy - rh, cx + dx + rw, cy + dy + rh], fill=color)


def make_camo(seed):
    rnd = random.Random(seed)
    img = Image.new("RGB", (SIZE, SIZE), BASE)
    draw = ImageDraw.Draw(img)

    # Manchas grandes y medianas (con wrap horizontal para que la union
    # del cilindro del barril no corte las manchas de golpe)
    for _ in range(260):
        color = PALETTE[rnd.randrange(len(PALETTE))]
        cx = rnd.randint(0, SIZE)
        cy = rnd.randint(0, SIZE)
        r = rnd.randint(50, 170)
        blob(draw, cx, cy, r, color, rnd)
        if cx < r * 2:
            blob(draw, cx + SIZE, cy, r, color, rnd)
        if cx > SIZE - r * 2:
            blob(draw, cx - SIZE, cy, r, color, rnd)

    img = img.filter(ImageFilter.GaussianBlur(3))

    # Vetas verticales sutiles (aspecto de chapa)
    shade = Image.new("L", (SIZE, SIZE), 0)
    sdraw = ImageDraw.Draw(shade)
    for _ in range(400):
        x = rnd.randint(0, SIZE - 1)
        w = rnd.randint(2, 10)
        alpha = rnd.randint(6, 18)
        sdraw.rectangle([x, 0, x + w, SIZE], fill=alpha)
    img = Image.composite(Image.new("RGB", (SIZE, SIZE), (0, 0, 0)), img, shade)

    return img


def find_font(size):
    for name in ("impact.ttf", "arialbd.ttf", "arial.ttf"):
        path = os.path.join("C:\\Windows\\Fonts", name)
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def stamp_text(img, color):
    """Estampa '3xor' en grilla por toda la textura: como no conocemos el UV
    exacto del modelo, el tileo garantiza que el logo caiga visible en el
    cuerpo del barril. Se ajusta tras la primera prueba in-game."""
    draw = ImageDraw.Draw(img, "RGBA")
    font = find_font(170)
    cols = 3
    rows = 4
    for cidx in range(cols):
        for ridx in range(rows):
            x = int(SIZE * (cidx + 0.5) / cols)
            y = int(SIZE * (ridx + 0.5) / rows)
            draw.text((x, y), "3xor", font=font, fill=color, anchor="mm",
                      stroke_width=6, stroke_fill=(15, 15, 15, 255))
    return img


def make_discord():
    """Icono estilo Discord: cuadrado redondeado blurple + mascota blanca (aprox)."""
    ss = 4
    out = 512
    w = out * ss
    img = Image.new("RGBA", (w, w), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    blurple = (88, 101, 242, 255)
    white = (255, 255, 255, 255)

    def s(v):
        return int(v * ss)

    # fondo redondeado (cuadrado redondeado tipo app icon)
    pad = s(20)
    d.rounded_rectangle([pad, pad, w - pad, w - pad], radius=s(116), fill=blurple)
    # cuerpo de la mascota (capsula ancha)
    d.rounded_rectangle([s(150), s(188), s(362), s(350)], radius=s(96), fill=white)
    # patas inferiores
    d.ellipse([s(150), s(286), s(240), s(378)], fill=white)
    d.ellipse([s(272), s(286), s(362), s(378)], fill=white)
    # muesca entre las patas
    d.ellipse([s(236), s(338), s(276), s(404)], fill=blurple)
    # ojos
    d.ellipse([s(205), s(226), s(244), s(298)], fill=blurple)
    d.ellipse([s(268), s(226), s(307), s(298)], fill=blurple)

    return img.resize((out, out), Image.LANCZOS)


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    base = make_camo(seed=3018)

    for name, color in (("exor_barrel_500_co.png", WHITE),):
        img = stamp_text(base.copy(), color).convert("RGBA")
        out = os.path.abspath(os.path.join(OUT_DIR, name))
        img.save(out)
        print("OK", out)

    disc_out = os.path.abspath(os.path.join(OUT_DIR, "exor_discord_ca.png"))
    make_discord().save(disc_out)
    print("OK", disc_out)


if __name__ == "__main__":
    main()
