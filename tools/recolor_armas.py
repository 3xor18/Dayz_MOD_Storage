#!/usr/bin/env python3
# Recolorea las texturas _co vanilla de las ARMAS a las paletas de color del set 3xor.
#
# Reusa el mismo remapeo HSV que la ropa (recolor_ropa.recolor): conserva el relieve, las
# marcas y el desgaste pintados, y solo cambia el tono/saturacion. El _nohq (relieve) y el
# _smdi (brillo) se heredan de vanilla, asi que salen gratis.
#
# DIFERENCIA CLAVE CON LA ROPA: las armas vanilla parten de un gris muy oscuro, casi negro.
# Con las paletas de ropa (que preservan la luminancia) un "rosa" quedaba un negro con un
# tinte que no se ve. Por eso todas las paletas de aca usan AUTO-NIVELADO (v_target) y una
# VENTANA de luminancia (v_lo/v_hi): primero llevan la textura a un gris medio y recien ahi
# aplican el color. Es lo mismo que hacen las paletas acromaticas de la ropa (urbano/nieve).
#
# Uso:  python tools/recolor_armas.py [rosa azul dorado]
import os
import sys

import numpy as np
from PIL import Image

from recolor_ropa import recolor

BASE = os.path.dirname(os.path.abspath(__file__))
PNG = os.path.join(BASE, "png_armas")
OUT = os.path.join(BASE, "out_armas")

# ROSA: el arma admite mas saturacion que la ropa (ahi 0.36 era el limite para que no
# pareciera disfraz). Un arma rosa se quiere ver rosa; igual el tope en 0.55 evita el fucsia.
ROSA = dict(hue=0.945, spread=0.30, spread_min=-0.045, spread_max=0.045,
            sat_scale=1.85, sat_add=0.34, sat_max=0.60, neutro=0.04,
            val_gamma=0.95, val_gain=1.0, val_lift=0.0, contraste=1.22,
            v_lo=0.10, v_hi=0.86, v_target=0.46)

# AZUL: tono 0.60 = ~216 grados (azul acero, no celeste ni violeta). Ventana un poco mas
# baja que el rosa porque el azul saturado se lee mas claro de lo que es.
AZUL = dict(hue=0.600, spread=0.26, spread_min=-0.040, spread_max=0.040,
            sat_scale=1.95, sat_add=0.36, sat_max=0.66, neutro=0.04,
            val_gamma=0.95, val_gain=1.0, val_lift=0.0, contraste=1.22,
            v_lo=0.08, v_hi=0.82, v_target=0.42)

# DORADO / PLATINADO: tono 0.122 = ~44 grados (oro, no amarillo limon ni naranja). Es la
# unica que sube fuerte la luminancia (v_target 0.62, techo 1.0): el oro es un metal CLARO,
# y si queda en el gris de las otras se lee como bronce sucio. El contraste alto mantiene
# separadas las zonas pulidas de las sombras, que es lo que da la sensacion de metal.
# El brillo metalico de verdad no sale de aca sino del .rvmat propio (ver gen_armas_config).
DORADO = dict(hue=0.122, spread=0.22, spread_min=-0.035, spread_max=0.035,
              sat_scale=1.95, sat_add=0.36, sat_max=0.70, neutro=0.04,
              val_gamma=0.82, val_gain=1.0, val_lift=0.0, contraste=1.32,
              v_lo=0.20, v_hi=0.98, v_target=0.58)

SETS = {"rosa": ROSA, "azul": AZUL, "dorado": DORADO}

# ---------------------------------------------------------------------------
#  CAMUFLAJE MILITAR: no es un cambio de tono, es un PATRON
# ---------------------------------------------------------------------------
# Las otras tres paletas remapean el tono y listo, porque el arma vanilla es gris parejo.
# El camo no se puede hacer asi: por definicion necesita MANCHAS, y un remapeo de tono
# nunca las va a inventar. Entonces:
#   1) se genera un patron woodland propio (manchas organicas, semilla fija = reproducible),
#   2) se saca la LUMINANCIA de la textura vanilla y se normaliza a 1.0 de media: eso es el
#      detalle mecanico puro -bordes, tornillos, rayones, sombras pintadas-, sin su color,
#   3) se multiplica el patron por ese detalle.
# El resultado es un arma PINTADA de camo: mantiene cada rayon y cada sombra del original,
# pero el color lo pone el patron. Multiplicar (y no mezclar) es la clave: un blend deja el
# camo lavado y plano, la multiplicacion conserva el contraste del relieve.
CAMO_PALETA = [
    (74, 80, 52),    # oliva medio (base)
    (46, 54, 36),    # verde oscuro
    (92, 82, 58),    # marron/tan
    (30, 33, 26),    # casi negro
    (60, 68, 46),    # oliva claro
]
CAMO_SEED = 20260908


def _manchas(w, h, seed):
    """Patron woodland del tamanio pedido. El radio va en proporcion al lado mas corto para
    que la escala de las manchas se vea igual en una textura de 2048 y en una de 512."""
    import random as _r
    from PIL import ImageDraw, ImageFilter
    rnd = _r.Random(seed)
    lado = min(w, h)
    img = Image.new("RGB", (w, h), CAMO_PALETA[0])
    d = ImageDraw.Draw(img)
    cantidad = int((w * h) / (lado * lado) * 90) + 90
    for _ in range(cantidad):
        color = CAMO_PALETA[rnd.randrange(1, len(CAMO_PALETA))]
        cx = rnd.randint(0, w)
        cy = rnd.randint(0, h)
        r = rnd.randint(int(lado * 0.035), int(lado * 0.11))
        for _ in range(rnd.randint(4, 7)):
            dx = rnd.randint(-r, r)
            dy = rnd.randint(-r, r)
            rw = rnd.randint(int(r * 0.5), int(r * 1.3))
            rh = rnd.randint(int(r * 0.4), int(r * 1.1))
            d.ellipse([cx + dx - rw, cy + dy - rh, cx + dx + rw, cy + dy + rh], fill=color)
    # un desenfoque MINIMO: suaviza el borde de pixel puro sin llegar a difuminar la mancha
    return img.filter(ImageFilter.GaussianBlur(radius=max(1.0, lado / 900.0)))


def camuflaje(img, seed=CAMO_SEED):
    a = np.asarray(img.convert("RGB"), dtype=np.float32) / 255.0
    w, h = img.size
    patron = np.asarray(_manchas(w, h, seed), dtype=np.float32) / 255.0

    # luminancia perceptual del vanilla, normalizada a media 1.0 = "cuanto mas claro u
    # oscuro que el promedio es este pixel". El fondo negro del UV (v ~ 0) se deja intacto.
    lum = (0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2])
    util = lum[lum > 0.02]
    med = float(np.clip(util.mean(), 0.02, 0.98)) if util.size else 0.5
    det = np.clip(lum / med, 0.35, 2.10)[..., None]

    out = np.clip(patron * det, 0.0, 1.0)
    out = np.where((lum < 0.02)[..., None], a, out)   # fondo del UV: intacto
    return Image.fromarray((out * 255.0 + 0.5).astype(np.uint8), "RGB")

# nombre corto -> archivo vanilla del que sale (solo para documentar de donde viene)
TEXTURAS = {
    "m4_body": "m4_body_co.paa",              # cuerpo del M4A1
    "akm": "akm_co.paa",                      # cuerpo del AKM
    "akm_wood": "akm_wood_co.paa",            # culata y guardamano de madera del AK
    "akm_acc": "akm_accessories_black_co.paa",  # culata plegable / riel del AK
    "m4_handguard": "handguard_co.paa",       # guardamano plastico del M4
    "m14_metal": "m14_metal_co.paa",          # DMR (M14): metal
    "m14_synth": "m14_synth_co.paa",          # DMR (M14): culata sintetica
    "sv98_wood": "sv98_wood_co.paa",          # VS-89 (SV98): madera
    "sv98_metal": "sv98_metal_co.paa",        # VS-89 (SV98): metal
    "aug_stock": "aug_stock_co.paa",          # cuerpo del AUR
    "aug_rail": "rail_co.paa",                # riel del AUR
    "aug_barrel": "aug_barrel_co.paa",        # canio del AUR
}


def main():
    solo = sys.argv[1:] or list(SETS.keys()) + ["camo"]
    os.makedirs(OUT, exist_ok=True)
    for nombre in solo:
        p = SETS.get(nombre)
        d = os.path.join(OUT, nombre)
        os.makedirs(d, exist_ok=True)
        for t in TEXTURAS:
            src = os.path.join(PNG, t + ".png")
            if not os.path.exists(src):
                print("FALTA", src, "(corre antes armas_pipeline.py paso 1-2)")
                continue
            im = Image.open(src)
            if nombre == "camo":
                camuflaje(im).save(os.path.join(d, t + ".png"))
            else:
                recolor(im, p).save(os.path.join(d, t + ".png"))
            print(nombre, t, "ok")


if __name__ == "__main__":
    main()
