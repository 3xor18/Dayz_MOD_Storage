#!/usr/bin/env python3
# Genera types_3xor_armas.xml con las armas y attachments de color.
#
# DISENIO (decidido el 8-sep-2026): las armas de color aparecen SOLO EN LOS HELI CRASH,
# nunca en el loot de los edificios. Eso se logra con dos cosas a la vez:
#
#   1) ACA: nominal 0 y min 0. Lo que decide si un item se reparte por el mundo es el
#      NOMINAL, no los tags. CORREGIDO el 8-sep: primero se puso nominal 5 "sin
#      category/usage/value para que no fuera a ningun edificio" y resulto ser al reves:
#      usage y value son FILTROS, y un tipo con nominal > 0 y sin ninguno de los dos no
#      queda restringido a nada -> el CE lo puede poner en CUALQUIER punto de loot del
#      mapa, zonas de spawn incluidas (aparecieron un M14 dorado y un AUG en la costa).
#      Con nominal 0 la unica fuente queda siendo el cargo del wreck, que se crea junto
#      con el objeto y no consulta el nominal. La entrada sigue haciendo falta igual:
#      es la que le da el LIFETIME al arma tirada en el piso.
#
#   2) En cfgspawnabletypes.xml de la mision: un <type> para Wreck_Mi8_Crashed y otro para
#      Wreck_UH1Y con las armas como <cargo>. Ese es el unico camino que GARANTIZA que
#      aparezcan ahi; el tier del mapa solo no alcanza, porque los helis caen en cualquier
#      zona. Ver el script de deploy en la sesion / el playbook de retexturas.
#
# Si algun dia se las quiere TAMBIEN en el mapa: subir el nominal Y darles category +
# usage + value, para que caigan donde uno quiere y no en todos lados.
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
    <!-- nominal 0 = el CE no la reparte por el mapa. Sale solo del cargo de los wrecks
         del heli crash (cfgspawnabletypes.xml). NO subir el nominal: eso es lo que las
         hacia aparecer en las zonas de spawn. -->
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
