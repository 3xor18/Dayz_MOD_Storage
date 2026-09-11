#!/usr/bin/env python3
# Textura CAMUFLADA del cofre de loot (modulo cofres_loot). Reutiliza el baul marino
# vanilla (SeaChest, hiddenSelections = {"camoGround"}) y le pinta el mismo patron
# woodland que usan las armas camo del mod, asi el cofre se ve del mismo set.
#
#   python tools/recolor_cofre.py
#
# Es re-ejecutable: si DayZ actualiza la textura del baul, se vuelve a correr.
# Ver el playbook de retextura: solo se cambia el _co; el relieve (_nohq) y el brillo
# (_smdi) se heredan del material vanilla del p3d y salen gratis.
import os
import subprocess
import sys

from PIL import Image

from recolor_armas import camuflaje

BASE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(BASE)
TMP = os.path.join(BASE, "paa_cofre_vanilla")
DEST = os.path.join(REPO, "src", "ExorStorage", "data", "cofres")

DAYZ = r"D:\SteamLibrary\steamapps\common\DayZ\Addons"
IMG2PAA = r"D:\SteamLibrary\steamapps\common\DayZ Tools\Bin\ImageToPAA\ImageToPAA.exe"
EXTRACT = os.path.join(BASE, "extract_pbo.py")

PBO = "gear_camping.pbo"
TEX = "sea_chest_co.paa"
SALIDA = "exor_cofre_camo_co.paa"
SEED = 20260911   # semilla propia: el cofre no tiene por que repetir las manchas del arma


def img2paa(src, dst):
    r = subprocess.run([IMG2PAA, src, dst], capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(dst):
        raise RuntimeError("ImageToPAA fallo: %s -> %s\n%s" % (src, dst, r.stdout + r.stderr))


def main():
    os.makedirs(TMP, exist_ok=True)
    os.makedirs(DEST, exist_ok=True)

    crudo = os.path.join(TMP, "_pbo")
    subprocess.run([sys.executable, EXTRACT, os.path.join(DAYZ, PBO), crudo, TEX],
                   check=True, stdout=subprocess.DEVNULL)
    hallado = None
    for raiz, _, files in os.walk(crudo):
        if TEX in files:
            hallado = os.path.join(raiz, TEX)
            break
    if not hallado:
        raise SystemExit("no se encontro %s dentro de %s" % (TEX, PBO))

    png_van = os.path.join(TMP, "sea_chest_co.png")
    img2paa(hallado, png_van)
    van = Image.open(png_van)
    print("vanilla:", van.size)

    png_camo = os.path.join(TMP, "sea_chest_camo_co.png")
    camuflaje(van, seed=SEED).save(png_camo)

    dst = os.path.join(DEST, SALIDA)
    img2paa(png_camo, dst)
    print("listo:", dst, os.path.getsize(dst), "bytes")


if __name__ == "__main__":
    main()
