#!/usr/bin/env python3
# Pipeline completo de las armas de color: vanilla .paa -> .png -> recoloreado -> .paa
# dentro del mod. Es re-ejecutable: si el juego actualiza una textura, se vuelve a correr.
#
#   python tools/armas_pipeline.py            (todo)
#   python tools/armas_pipeline.py extraer    (solo bajar los .paa del juego)
#   python tools/armas_pipeline.py png        (solo .paa -> .png)
#   python tools/armas_pipeline.py recolor    (solo recolorear)
#   python tools/armas_pipeline.py paa        (solo .png -> .paa del mod)
#   python tools/armas_pipeline.py preview    (hoja de contactos para mirar los colores)
#
# ImageToPAA convierte en LOS DOS SENTIDOS: dandole un .paa de entrada y un .png de salida
# hace la inversa. No hace falta TexView ni decodificar DXT a mano.
import os
import subprocess
import sys

from PIL import Image

BASE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(BASE)
VAN = os.path.join(BASE, "paa_armas_vanilla")
PNG = os.path.join(BASE, "png_armas")
OUT = os.path.join(BASE, "out_armas")
DEST = os.path.join(REPO, "src", "ExorStorage", "data", "armas")

DAYZ = r"D:\SteamLibrary\steamapps\common\DayZ\Addons"
IMG2PAA = r"D:\SteamLibrary\steamapps\common\DayZ Tools\Bin\ImageToPAA\ImageToPAA.exe"
EXTRACT = os.path.join(BASE, "extract_pbo.py")

# nombre corto -> (pbo del juego, nombre del archivo dentro del pbo)
FUENTES = {
    "m4_body": ("weapons_firearms.pbo", "m4_body_co.paa"),
    "akm": ("weapons_firearms.pbo", "akm_co.paa"),
    "aug_stock": ("weapons_firearms.pbo", "aug_stock_co.paa"),
    "aug_rail": ("weapons_firearms.pbo", "rail_co.paa"),
    "aug_barrel": ("weapons_firearms.pbo", "aug_barrel_co.paa"),
    "akm_wood": ("weapon_attachments_textures.pbo", "akm_wood_co.paa"),
    "akm_acc": ("weapon_attachments_textures.pbo", "akm_accessories_black_co.paa"),
    "m4_handguard": ("weapon_attachments_textures.pbo", "handguard_co.paa"),
    "m14_metal": ("weapons_firearms.pbo", "m14_metal_co.paa"),
    "m14_synth": ("weapons_firearms.pbo", "m14_synth_co.paa"),
    "sv98_wood": ("weapons_firearms.pbo", "sv98_wood_co.paa"),
    "sv98_metal": ("weapons_firearms.pbo", "sv98_metal_co.paa"),
}

COLORES = ["rosa", "azul", "dorado", "camo"]


def paso_extraer():
    os.makedirs(VAN, exist_ok=True)
    porpbo = {}
    for corto, (pbo, arch) in FUENTES.items():
        porpbo.setdefault(pbo, []).append((corto, arch))
    for pbo, lista in porpbo.items():
        tmp = os.path.join(VAN, "_tmp")
        cmd = [sys.executable, EXTRACT, os.path.join(DAYZ, pbo), tmp] + [a for _, a in lista]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        for corto, arch in lista:
            hallado = None
            for raiz, _, files in os.walk(tmp):
                if arch in files:
                    hallado = os.path.join(raiz, arch)
                    break
            if not hallado:
                print("NO se encontro", arch, "en", pbo)
                continue
            dst = os.path.join(VAN, corto + ".paa")
            with open(hallado, "rb") as f, open(dst, "wb") as g:
                g.write(f.read())
            print("extraido", corto, os.path.getsize(dst), "bytes")


def _img2paa(src, dst):
    r = subprocess.run([IMG2PAA, src, dst], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(dst):
        raise RuntimeError("ImageToPAA fallo: %s -> %s\n%s" % (src, dst, r.stdout + r.stderr))


def paso_png():
    os.makedirs(PNG, exist_ok=True)
    for corto in FUENTES:
        src = os.path.join(VAN, corto + ".paa")
        dst = os.path.join(PNG, corto + ".png")
        _img2paa(src, dst)
        print("png", corto, Image.open(dst).size)


def paso_recolor():
    subprocess.run([sys.executable, os.path.join(BASE, "recolor_armas.py")], check=True)


def paso_paa():
    os.makedirs(DEST, exist_ok=True)
    total = 0
    for color in COLORES:
        for corto in FUENTES:
            src = os.path.join(OUT, color, corto + ".png")
            if not os.path.exists(src):
                print("FALTA", src)
                continue
            dst = os.path.join(DEST, "exor_%s_%s_co.paa" % (color, corto))
            _img2paa(src, dst)
            total += os.path.getsize(dst)
            print("paa", os.path.basename(dst), os.path.getsize(dst), "bytes")
    print("TOTAL en el PBO por las armas:", round(total / 1048576, 1), "MB")


def paso_preview():
    """Hoja de contactos: una fila por textura, una columna por color (+ el vanilla).
    Sirve para mirar los colores SIN construir el PBO ni abrir el juego."""
    cols = ["vanilla"] + COLORES
    filas = list(FUENTES.keys())
    cel = 260
    img = Image.new("RGB", (cel * len(cols), cel * len(filas)), (18, 18, 22))
    for fy, corto in enumerate(filas):
        for fx, color in enumerate(cols):
            p = os.path.join(PNG, corto + ".png") if color == "vanilla" \
                else os.path.join(OUT, color, corto + ".png")
            if not os.path.exists(p):
                continue
            im = Image.open(p).convert("RGB")
            im.thumbnail((cel - 8, cel - 8))
            img.paste(im, (fx * cel + 4, fy * cel + 4))
    dst = os.path.join(BASE, "preview_armas.png")
    img.save(dst)
    print("preview:", dst, img.size, '| filas:', filas, '| columnas:', cols)


PASOS = {"extraer": paso_extraer, "png": paso_png, "recolor": paso_recolor,
         "paa": paso_paa, "preview": paso_preview}

if __name__ == "__main__":
    quiero = sys.argv[1:] or ["extraer", "png", "recolor", "paa", "preview"]
    for p in quiero:
        print("\n===== paso:", p, "=====")
        PASOS[p]()
