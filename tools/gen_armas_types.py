#!/usr/bin/env python3
# Genera types_3xor_armas.xml con las armas y attachments de color.
#
# DISENIO (decidido el 8-sep-2026): las armas de color aparecen SOLO EN LOS HELI CRASH,
# nunca en el loot de los edificios. Eso se logra con dos cosas a la vez:
#
#   1) ACA: nominal > 0 pero SIN category/usage/value. Sin esos tres el CE no sabe en que
#      edificio ponerlas, asi que no las pone en NINGUNO. El nominal igual tiene que ser
#      > 0 para que el tipo este disponible en la economia cuando el wreck lo pida de
#      cargo, y la entrada es ademas la que le da el LIFETIME al arma tirada en el piso.
#      count_in_cargo="0" -> las que estan dentro del wreck no cuentan contra el nominal.
#
#   2) En cfgspawnabletypes.xml de la mision: un <type> para Wreck_Mi8_Crashed y otro para
#      Wreck_UH1Y con las armas como <cargo>. Ese es el unico camino que GARANTIZA que
#      aparezcan ahi; el tier del mapa solo no alcanza, porque los helis caen en cualquier
#      zona. Ver el script de deploy en la sesion / el playbook de retexturas.
#
# Si algun dia se las quiere TAMBIEN en el mapa, hay que agregarles category + usage +
# value (los tres, o el CE no las spawnea nunca por mas nominal que tengan).
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
    <nominal>5</nominal>
    <lifetime>%d</lifetime>
    <restock>0</restock>
    <min>0</min>
    <quantmin>-1</quantmin>
    <quantmax>-1</quantmax>
    <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0" />
    <!-- SIN category/usage/value a proposito: es lo que las mantiene fuera del loot de
         los edificios. Salen solo del cargo de los wrecks del heli crash. -->
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
