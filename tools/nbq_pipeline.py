#!/usr/bin/env python3
# Pipeline del set NBQ (trajes NBC) en los cinco colores del mod: vanilla .paa -> .png ->
# recoloreado -> .paa dentro del mod. Re-ejecutable: si DayZ actualiza una textura, se
# vuelve a correr.
#
#   python tools/nbq_pipeline.py
#
# ⭐ POR QUE SE PARTE DEL AMARILLO Y NO DEL GRIS
# Las paletas de ropa remapean el TONO respecto del tono dominante de la textura, y ademas
# dejan quieto lo casi-neutro (si la saturacion esta por debajo de 'neutro', no se tine).
# El NBC gris es justamente eso: casi sin tono. Partiendo del gris, rosa y arido devolvian
# la prenda gris igual que entro. El amarillo tiene tono de sobra, asi que el remapeo tiene
# de donde agarrarse; y para nieve, urbano y negro da lo mismo, porque esas tres aplastan
# la saturacion y el color lo hace el rango de grises.
#
# Igual que el resto de la ropa: solo se cambia el _co. El relieve (_nohq) y el brillo
# (_smdi) se heredan del material vanilla y salen gratis.
import os
import subprocess
import sys

from PIL import Image

from recolor_armas import camuflaje
from recolor_ropa import SETS, recolor

BASE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(BASE)
TMP = os.path.join(BASE, "paa_nbq_vanilla")
PNG = os.path.join(TMP, "png")
DEST = os.path.join(REPO, "src", "ExorStorage", "data", "ropa")

DAYZ = r"D:\SteamLibrary\steamapps\common\DayZ\Addons"
IMG2PAA = r"D:\SteamLibrary\steamapps\common\DayZ Tools\Bin\ImageToPAA\ImageToPAA.exe"
EXTRACT = os.path.join(BASE, "extract_pbo.py")

# nombre en el mod -> (pbo del juego, archivo dentro del pbo)
FUENTES = {
    "nbc_jacket_ground": ("characters_tops.pbo", "NBC_Jacket_g_yellow_co.paa"),
    "nbc_jacket_worn":   ("characters_tops.pbo", "NBC_Jacket_yellow_co.paa"),
    "nbc_pants":         ("characters_pants.pbo", "NBC_Pants_yellow_co.paa"),
    "nbc_hood":          ("characters_headgear.pbo", "NBC_Hood_yellow_co.paa"),
    "nbc_gloves":        ("characters_gloves.pbo", "NBC_Gloves_yellow_co.paa"),
    "nbc_boots":         ("characters_shoes.pbo", "NBC_Boots_yellow_co.paa"),
}


def img2paa(src, dst):
    r = subprocess.run([IMG2PAA, src, dst], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(dst):
        raise RuntimeError("ImageToPAA fallo: %s -> %s\n%s" % (src, dst, r.stdout + r.stderr))


def extraer():
    os.makedirs(PNG, exist_ok=True)
    porpbo = {}
    for corto, (pbo, arch) in FUENTES.items():
        porpbo.setdefault(pbo, []).append((corto, arch))
    for pbo, lista in porpbo.items():
        crudo = os.path.join(TMP, "_" + pbo.replace(".pbo", ""))
        cmd = [sys.executable, EXTRACT, os.path.join(DAYZ, pbo), crudo] + [a for _, a in lista]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        for corto, arch in lista:
            hallado = None
            for raiz, _, files in os.walk(crudo):
                if arch in files:
                    hallado = os.path.join(raiz, arch)
                    break
            if not hallado:
                raise SystemExit("no se encontro %s en %s" % (arch, pbo))
            dst = os.path.join(PNG, corto + ".png")
            img2paa(hallado, dst)
            print("vanilla", corto, Image.open(dst).size)


# ⭐ EL ARIDO VA CON PATRON, NO CON REMAPEO DE TONO.
# El resto de la ropa del mod parte de prendas que YA tienen camuflaje pintado (la gorka),
# asi que alcanza con remapear el tono: las manchas ya estaban. El traje NBC vanilla es
# LISO, de un solo color, asi que remapearle el tono devuelve un mameluco arena plano: se
# ve como un traje de pintor, no como ropa militar. Para el arido se genera un patron
# woodland propio y se multiplica por la luminancia del vanilla -el mismo metodo que las
# armas camo del mod-, que conserva pliegues, costuras y sombras y solo cambia el color.
# Los otros cuatro colores SI van lisos a proposito: nieve, negro y urbano son colores
# planos por definicion, y el rosa es un color, no un camuflaje.
CAMO = ["arido"]
CAMO_SEED = 20260912


def recolorear():
    total = 0
    os.makedirs(DEST, exist_ok=True)
    for color, paleta in SETS.items():
        for corto in FUENTES:
            im = Image.open(os.path.join(PNG, corto + ".png"))
            tmp_png = os.path.join(PNG, "%s_%s.png" % (color, corto))
            if color in CAMO:
                camuflaje(im, seed=CAMO_SEED).save(tmp_png)
            else:
                recolor(im, paleta).save(tmp_png)
            dst = os.path.join(DEST, "exor_%s_%s_co.paa" % (color, corto))
            img2paa(tmp_png, dst)
            total += os.path.getsize(dst)
            print("paa", os.path.basename(dst), os.path.getsize(dst), "bytes")
    print("TOTAL que suma el set NBQ al PBO:", round(total / 1048576, 1), "MB")


def preview():
    """Hoja de contactos: una fila por pieza, una columna por color (+ el vanilla).
    Sirve para mirar los colores sin construir el PBO ni abrir el juego."""
    cols = ["vanilla"] + list(SETS.keys())
    filas = list(FUENTES.keys())
    cel = 220
    img = Image.new("RGB", (cel * len(cols), cel * len(filas)), (18, 18, 22))
    for fy, corto in enumerate(filas):
        for fx, color in enumerate(cols):
            p = os.path.join(PNG, corto + ".png") if color == "vanilla" \
                else os.path.join(PNG, "%s_%s.png" % (color, corto))
            if not os.path.exists(p):
                continue
            im = Image.open(p).convert("RGB")
            im.thumbnail((cel - 8, cel - 8))
            img.paste(im, (fx * cel + 4, fy * cel + 4))
    dst = os.path.join(BASE, "preview_nbq.png")
    img.save(dst)
    print("preview:", dst, img.size)


if __name__ == "__main__":
    pasos = sys.argv[1:] or ["extraer", "recolorear", "preview"]
    for p in pasos:
        print("\n===== paso:", p, "=====")
        {"extraer": extraer, "recolorear": recolorear, "preview": preview}[p]()
