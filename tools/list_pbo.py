#!/usr/bin/env python3
# Lista las entradas de un PBO de DayZ (cabeceras solamente, no extrae).
# Uso: python list_pbo.py <ruta.pbo> [filtro]
import struct, sys

def read_cstr(f):
    out = bytearray()
    while True:
        b = f.read(1)
        if not b or b == b"\x00":
            return out.decode("ascii", "replace")
        out += b

def list_pbo(path, filtro=""):
    entries = []
    prefix = ""
    with open(path, "rb") as f:
        while True:
            name = read_cstr(f)
            method, osize, res, ts, dsize = struct.unpack("<5I", f.read(20))
            if name == "" and method == 0x56657273:  # header "Vers" con propiedades
                while True:
                    k = read_cstr(f)
                    if k == "":
                        break
                    v = read_cstr(f)
                    if k.lower() == "prefix":
                        prefix = v
                continue
            if name == "":  # terminador
                break
            entries.append((name, dsize))
    print("prefix:", prefix)
    for n, s in entries:
        if filtro.lower() in n.lower():
            print(f"{s:>12}  {n}")

if __name__ == "__main__":
    list_pbo(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "")
