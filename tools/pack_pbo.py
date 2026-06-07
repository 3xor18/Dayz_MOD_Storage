#!/usr/bin/env python3
# Empacador de PBO minimo para mods de DayZ (sin DayZ Tools).
# Crea un PBO valido sin compresion (configs, scripts y .paa se guardan tal cual).
# Uso: python pack_pbo.py [--src DIR] [--prefix PREFIX] [--out RUTA.pbo]
import argparse
import hashlib
import os
import struct

EXCLUDE_EXT = {".png", ".psd", ".md", ".xcf"}


def pack(src_root, prefix, out_path):
    # Recolectar archivos (rutas relativas con backslash, como exige el PBO)
    files = []
    for dirpath, _, filenames in os.walk(src_root):
        for fn in filenames:
            if os.path.splitext(fn)[1].lower() in EXCLUDE_EXT:
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, src_root).replace("/", "\\")
            files.append((rel, full))
    files.sort(key=lambda x: x[0].lower())

    buf = bytearray()

    # --- Entrada de producto (header) con la propiedad prefix ---
    buf += b"\x00"                        # nombre vacio
    buf += struct.pack("<I", 0x56657273)  # packing method "Vers"
    buf += struct.pack("<I", 0) * 4       # original/reserved/timestamp/data size
    buf += b"prefix\x00" + prefix.encode("ascii") + b"\x00"
    buf += b"\x00"

    # --- Cabeceras de cada archivo ---
    datas = []
    for rel, full in files:
        with open(full, "rb") as f:
            data = f.read()
        datas.append(data)
        buf += rel.encode("ascii") + b"\x00"
        buf += struct.pack("<I", 0)          # packing method 0 = stored
        buf += struct.pack("<I", len(data))  # original size
        buf += struct.pack("<I", 0)          # reserved
        buf += struct.pack("<I", 0)          # timestamp
        buf += struct.pack("<I", len(data))  # data size

    # --- Entrada terminadora ---
    buf += b"\x00" + struct.pack("<I", 0) * 5

    # --- Bloques de datos ---
    for data in datas:
        buf += data

    # --- Checksum SHA1: 0x00 + sha1(contenido previo) ---
    sha = hashlib.sha1(bytes(buf)).digest()
    buf += b"\x00" + sha

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(bytes(buf))

    print("PBO creado:", out_path)
    print("Tamano:", len(buf), "bytes |", len(files), "archivos")
    for rel, _ in files:
        print("  +", rel)


if __name__ == "__main__":
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.join(repo, "src", "ExorStorage"))
    ap.add_argument("--prefix", default="ExorStorage")
    ap.add_argument("--out", default=os.path.join(repo, "dist", "@3xor_Vanilla_Optimization", "addons", "ExorStorage.pbo"))
    args = ap.parse_args()
    pack(args.src, args.prefix, args.out)
