#!/usr/bin/env python3
# Genera types_3xor_armas.xml con las armas y attachments de color.
#
# Arranca con nominal=0 / min=0 A PROPOSITO: las armas NO entran al loot del mundo, salen
# solo por /arma_color, cofre o KOTH. La entrada hace falta igual, porque es la que le da
# el LIFETIME al item tirado en el piso; sin ella el CE le aplica el default y se puede
# limpiar antes de tiempo.
#
# Para convertirlas en loot de mundo NO alcanza con subir nominal: hacen falta ademas
# category + usage + value, si no el CE no sabe en que edificio ponerlas y no las spawnea
# nunca. Por eso los tres van escritos abajo, comentados y listos para descomentar.
# Ver la nota de types.xml en el playbook de retexturas.
import io
import os

BASE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(BASE)
OUT = os.path.join(REPO, "types_3xor_armas.xml")

COLORES = ["Rosa", "Azul", "Dorado", "Camo"]
ARMAS = ["M4A1", "AKM", "Aug", "M14", "SV98"]
ATTACHS = ["M4_OEBttstck", "M4_MPBttstck", "M4_CQBBttstck", "M4_RISHndgrd", "M4_MPHndgrd",
           "M4_PlasticHndgrd", "AK_WoodBttstck", "AK_WoodHndgrd", "AK_FoldingBttstck",
           "AK_RailHndgrd", "AK_PlasticBttstck", "AK_PlasticHndgrd"]


def entrada(nombre, lifetime):
    return """  <type name="%s">
    <nominal>0</nominal>
    <lifetime>%d</lifetime>
    <restock>0</restock>
    <min>0</min>
    <quantmin>-1</quantmin>
    <quantmax>-1</quantmax>
    <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0" />
    <!-- para que entren al loot del mundo: subir nominal/min y descomentar estas tres
         lineas (sin category+usage+value el CE nunca las spawnea, por mas nominal que tengan)
    <category name="weapons" />
    <usage name="Military" />
    <value name="Tier3" />
    <value name="Tier4" />
    -->
  </type>
""" % (nombre, lifetime)


def main():
    L = ['<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<types>\n']
    for c in COLORES:
        L.append("  <!-- ================= set %s ================= -->\n" % c)
        for a in ARMAS:
            L.append(entrada("Exor_%s_%s" % (a, c), 14400))    # armas: mismo lifetime que las vanilla
        for a in ATTACHS:
            L.append(entrada("Exor_%s_%s" % (a, c), 7200))     # attachments: la mitad, como vanilla
    L.append("</types>\n")
    io.open(OUT, "w", encoding="utf-8", newline="\n").write("".join(L))
    print(OUT, "->", len(COLORES) * (len(ARMAS) + len(ATTACHS)), "entradas")


if __name__ == "__main__":
    main()
