#!/usr/bin/env python3
# Genera el bloque de ARMAS DE COLOR del config.cpp y actualiza CfgPatches.
# Re-ejecutable: reescribe SOLO lo que hay entre los marcadores.
#
#   python tools/gen_armas_config.py
#
# COMO FUNCIONA UNA VARIANTE DE COLOR (igual que la ropa, ver el playbook de retexturas):
# se HEREDA de la clase vanilla y se le pisa `hiddenSelectionsTextures[]`. Nunca se reabre
# la clase original, asi que el arma vanilla queda intacta.
#
# DOS FAMILIAS DE PIEZA, porque vanilla mismo las trata distinto:
#   - TEXTURA: la pieza tiene un _co pintado (los cuerpos de las armas, la madera del AK).
#     Ahi se apunta a nuestro .paa recoloreado.
#   - COLOR PLANO: vanilla resuelve varias culatas y guardamanos con una textura procedural
#     de un solo color -asi hace el verde: #(argb,8,8,3)color(0.35,0.36,0.28,1.0,CO)-. En
#     esas piezas el detalle lo pone el normal map, no el _co, asi que se copia el metodo:
#     una linea con el color y listo, sin gastar un .paa.
#
# ⚠️ DOS TRAMPAS QUE COSTARON DOS VUELTAS (no volver a pisarlas):
#   1) NO abrir `class CfgWeapons` ni `class CfgVehicles` propios: config.cpp YA tiene los
#      dos, y el parser de DayZ corta el arranque con "Member already defined" si una clase
#      de nivel superior aparece dos veces en el MISMO archivo. Por eso las clases nuevas se
#      inyectan DENTRO de los bloques que ya existen, entre marcadores.
#   2) NO buscar "el ultimo };" del archivo para colgarse ahi: config.cpp termina con un
#      bloque CfgNonAIVehicles COMENTADO con /* */, y ese "};" de adentro del comentario se
#      llevaba las 68 clases adentro del comentario. Resultado: no existian, /arma_color no
#      spawneaba nada, no salian en VPP, y no habia UN SOLO error ni al empaquetar ni al
#      arrancar. Los marcadores evitan las dos cosas.
#
# LA LAR (FAL) NO SE PUEDE: es la unica que no declara `hiddenSelections`, o sea que su
# textura esta cerrada dentro del modelo y no hay gancho de config donde meter otra. Para
# colorearla habria que tocar el p3d, que es otro trabajo (y redistribuir el modelo).
import io
import os
import re

BASE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(BASE)
CFG = os.path.join(REPO, "src", "ExorStorage", "config.cpp")

INI_W = "\t// ---------------- ARMAS_INI_W (generado por tools/gen_armas_config.py) ----------------"
FIN_W = "\t// ---------------- ARMAS_FIN_W ----------------"
INI_V = "\t// ---------------- ARMAS_INI_V (generado por tools/gen_armas_config.py) ----------------"
FIN_V = "\t// ---------------- ARMAS_FIN_V ----------------"

RUTA = "ExorStorage\\data\\armas"

# color -> (sufijo de clase, nombre visible, color plano RGB para las piezas sin textura)
COLORES = [
    ("rosa", "Rosa", "0.62,0.20,0.34"),
    ("azul", "Azul", "0.14,0.26,0.52"),
    ("dorado", "Dorado", "0.74,0.56,0.16"),
    ("camo", "Camo", "0.29,0.31,0.20"),
]

# ARMAS (van en CfgWeapons): clase vanilla, nombre visible, [texturas de hiddenSelections].
# La lista de texturas respeta el ORDEN de hiddenSelections[] de la clase vanilla: si se
# cambia el orden, el color cae en la pieza equivocada.
ARMAS = [
    ("M4A1", "M4-A1", ["m4_body"]),
    ("AKM", "KA-M", ["akm"]),
    ("Aug", "AUR AX", ["aug_stock", "aug_rail", "aug_barrel"]),
    ("M14", "DMR", ["m14_metal", "m14_synth"]),
    # el 3er hiddenSelection del VS-89 ("carryhandle") va vacio en vanilla: se respeta
    ("SV98", "VS-89", ["sv98_wood", "sv98_metal", ""]),
]

# ATTACHMENTS (van en CfgVehicles). tipo: "tex" o "plano"
ATTACHS = [
    ("M4_OEBttstck", "Culata M4 OE", "plano", None),
    ("M4_MPBttstck", "Culata M4 MP", "plano", None),
    ("M4_CQBBttstck", "Culata M4 CQB", "plano", None),
    ("M4_RISHndgrd", "Guardamano M4 RIS", "plano", None),
    ("M4_MPHndgrd", "Guardamano M4 MP", "plano", None),
    ("M4_PlasticHndgrd", "Guardamano M4 plastico", "tex", "m4_handguard"),
    ("AK_WoodBttstck", "Culata KA-M madera", "tex", "akm_wood"),
    ("AK_WoodHndgrd", "Guardamano KA-M madera", "tex", "akm_wood"),
    ("AK_FoldingBttstck", "Culata KA-M plegable", "tex", "akm_acc"),
    ("AK_RailHndgrd", "Guardamano KA-M riel", "tex", "akm_acc"),
    ("AK_PlasticBttstck", "Culata KA-M plastica", "plano", None),
    ("AK_PlasticHndgrd", "Guardamano KA-M plastico", "plano", None),
]

ADDONS_NUEVOS = ["DZ_Weapons_Firearms_M4", "DZ_Weapons_Firearms_AKM", "DZ_Weapons_Firearms_aug",
                 "DZ_Weapons_Firearms_M14", "DZ_Weapons_Firearms_SV98", "DZ_Weapons_Supports"]


def tex(color, corto):
    if corto == "":
        return '""'
    return '"%s\\exor_%s_%s_co.paa"' % (RUTA, color, corto)


def bloque_armas():
    """Contenido para adentro del CfgWeapons que ya existe."""
    L = [INI_W, ""]
    L.append("\t// Variantes de color de las armas. Se HEREDA de la clase vanilla y solo se pisa")
    L.append("\t// la textura: mismo modelo, mismo danio, mismas balas, mismos cargadores.")
    L.append("\t// Generado por tools/gen_armas_config.py - no editar a mano.")
    for cls, _, _ in ARMAS:
        L.append("\tclass %s;\t// externa (vanilla): forward declaration, NO la modifica" % cls)
    for color, suf, _ in COLORES:
        L.append("")
        L.append("\t// ---- armas %s ----" % suf)
        for cls, nombre, texturas in ARMAS:
            L.append("\tclass Exor_%s_%s: %s" % (cls, suf, cls))
            L.append("\t{")
            L.append("\t\tscope = 2;")
            L.append('\t\tdisplayName = "%s %s";' % (nombre, suf))
            L.append('\t\tdescriptionShort = "%s del set %s de 3xor. Igual que la vanilla, solo cambia el color.";' % (nombre, suf))
            L.append("\t\thiddenSelectionsTextures[] = {")
            L.append(",\n".join("\t\t\t" + tex(color, t) for t in texturas))
            L.append("\t\t};")
            L.append("\t};")
    L.append("")
    L.append(FIN_W)
    return "\n".join(L)


def bloque_attach():
    """Contenido para adentro del CfgVehicles que ya existe."""
    L = [INI_V, ""]
    L.append("\t// Culatas y guardamanos de color. Generado por tools/gen_armas_config.py.")
    for cls, _, _, _ in ATTACHS:
        L.append("\tclass %s;\t// externa (DZ_Weapons_Supports)" % cls)
    for color, suf, plano in COLORES:
        L.append("")
        L.append("\t// ---- culatas y guardamanos %s ----" % suf)
        for cls, nombre, tipo, corto in ATTACHS:
            L.append("\tclass Exor_%s_%s: %s" % (cls, suf, cls))
            L.append("\t{")
            L.append("\t\tscope = 2;")
            L.append('\t\tdisplayName = "%s %s";' % (nombre, suf))
            L.append('\t\tdescriptionShort = "%s del set %s de 3xor.";' % (nombre, suf))
            if tipo == "plano":
                L.append("\t\t// color plano, igual que la variante verde de vanilla: el relieve lo pone el normal map")
                L.append('\t\thiddenSelectionsTextures[] = {"#(argb,8,8,3)color(%s,1.0,CO)"};' % plano)
            else:
                L.append("\t\thiddenSelectionsTextures[] = {%s};" % tex(color, corto))
            L.append("\t};")
    L.append("")
    L.append(FIN_V)
    return "\n".join(L)


def reemplazar(s, ini, fin, nuevo):
    if ini not in s or fin not in s:
        raise SystemExit("FALTAN los marcadores en config.cpp:\n  %s\n  %s" % (ini, fin))
    i, j = s.index(ini), s.index(fin) + len(fin)
    return s[:i] + nuevo + s[j:]


def lista_arrays(s, nombre, agregar):
    m = re.search(r'(\t\t%s\[\] = \{)(.*?)(\};)' % nombre, s, re.S)
    actuales = [x.strip().strip('"') for x in m.group(2).split(",") if x.strip()]
    for a in agregar:
        if a not in actuales:
            actuales.append(a)
    return s[:m.start(2)] + ", ".join('"%s"' % x for x in actuales) + s[m.end(2):]


def main():
    s = io.open(CFG, encoding="utf-8").read()
    s = reemplazar(s, INI_W, FIN_W, bloque_armas())
    s = reemplazar(s, INI_V, FIN_V, bloque_attach())

    armas, units = [], []
    for _, suf, _ in COLORES:
        armas += ["Exor_%s_%s" % (c, suf) for c, _, _ in ARMAS]
        units += ["Exor_%s_%s" % (c, suf) for c, _, _, _ in ATTACHS]

    s = lista_arrays(s, "units", units)
    s = lista_arrays(s, "weapons", armas)
    # sin los addons padre las clases no encuentran su base y el juego las descarta EN SILENCIO
    s = lista_arrays(s, "requiredAddons", ADDONS_NUEVOS)

    io.open(CFG, "w", encoding="utf-8", newline="\n").write(s)
    print("armas:", len(armas), "| attachments:", len(units), "| total clases:", len(armas) + len(units))


if __name__ == "__main__":
    main()
