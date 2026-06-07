#!/usr/bin/env python3
# Extrae archivos especificos de un PBO de DayZ (solo entradas sin comprimir, method 0).
# Uso: python extract_pbo.py <ruta.pbo> <dir_salida> <filtro1> [filtro2 ...]
import struct, sys, os

def read_cstr(f):
    out = bytearray()
    while True:
        b = f.read(1)
        if not b or b == b"\x00":
            return out.decode("ascii", "replace")
        out += b

def extract(path, outdir, filtros):
    entries = []
    with open(path, "rb") as f:
        while True:
            name = read_cstr(f)
            method, osize, res, ts, dsize = struct.unpack("<5I", f.read(20))
            if name == "" and method == 0x56657273:
                while True:
                    k = read_cstr(f)
                    if k == "":
                        break
                    read_cstr(f)
                continue
            if name == "":
                break
            entries.append((name, method, dsize))
        # bloque de datos: arranca aca, en orden de cabeceras
        for name, method, dsize in entries:
            data = f.read(dsize)
            if any(fl.lower() in name.lower() for fl in filtros):
                if method not in (0, 0x56657273):
                    print(f"SKIP (comprimido, method={method:#x}): {name}")
                    continue
                dest = os.path.join(outdir, name.replace("\\", os.sep))
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                with open(dest, "wb") as o:
                    o.write(data)
                print(f"OK  {name}  ({dsize} bytes)")

if __name__ == "__main__":
    extract(sys.argv[1], sys.argv[2], sys.argv[3:])
