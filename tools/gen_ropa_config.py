#!/usr/bin/env python3
# Genera el bloque de CfgVehicles de los sets de ropa 3xor y lo inserta en config.cpp
# entre los marcadores >>> / <<<. Es idempotente: se puede volver a correr para agregar
# un color o una pieza sin tocar nada mas del config a mano.
#
# Uso:  python tools/gen_ropa_config.py
import io
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(REPO, "src", "ExorStorage", "config.cpp")
TYPES = os.path.join(REPO, "types_3xor_ropa.xml")

INICIO = "\t// >>> SETS DE ROPA 3xor (generado por tools/gen_ropa_config.py) >>>"
FIN = "\t// <<< SETS DE ROPA 3xor <<<"

# (sufijo de clase, nombre para mostrar)
SETS = [
    ("Rosa",   "Rosa"),
    ("Arido",  "Arido"),
    ("Urbano", "Urbano"),
    ("Nieve",  "Nieve"),
    ("Negro",  "Negro"),
]

# nombre de clase | padre vanilla | addon del padre | nombre visible | texturas (piso, puesto)
PIEZAS = [
    ("GorkaJacket",         "GorkaEJacket_ColorBase",   "DZ_Characters_Tops",     "Camisa Gorka",        "jacket_ground", "jacket_worn"),
    ("GorkaPants",          "GorkaPants_ColorBase",     "DZ_Characters_Pants",    "Pantalon Gorka",      "pants_ground",  "pants_worn"),
    ("BallisticHelmet",     "BallisticHelmet_ColorBase","DZ_Characters_Headgear", "Casco balistico",     "helmet",        "helmet"),
    ("Mich2001Helmet",      "Mich2001Helmet",           "DZ_Characters_Headgear", "Casco MICH 2001",     "mich",          "mich"),
    ("GorkaHelmet",         "GorkaHelmet",              "DZ_Characters_Headgear", "Casco Gorka",         "gorkahelmet",   "gorkahelmet"),
    ("BalaclavaMask",       "BalaclavaMask_ColorBase",  "DZ_Characters_Masks",    "Balaclava",           "balaclava",     "balaclava"),
    ("CombatBoots",         "CombatBoots_ColorBase",    "DZ_Characters_Shoes",    "Botas de combate",    "boots",         "boots"),
    ("TacticalGloves",      "TacticalGloves_ColorBase", "DZ_Characters_Gloves",   "Guantes tacticos",    "gloves",        "gloves"),
    ("PressVest",           "PressVest_ColorBase",      "DZ_Characters_Vests",    "Chaleco de prensa",   "press",         "press"),
    ("PlateCarrierVest",    "PlateCarrierVest",         "DZ_Characters_Vests",    "Chaleco balistico",   "plate",         "plate"),
    ("PlateCarrierHolster", "PlateCarrierHolster",      "DZ_Characters_Vests",    "Pistolera de chaleco","plate",         "plate"),
    ("PlateCarrierPouches", "PlateCarrierPouches",      "DZ_Gear_Containers",     "Bolsillos de chaleco","plate",         "plate"),
    ("TortillaBag",         "TortillaBag",              "DZ_Characters_Backpacks","Mochila tactica",     "tortilla",      "tortilla"),
]

# Piezas cuyo PADRE no declara hiddenSelections (las declara cada variante vanilla): hay que
# declararlas en la clase nueva o la textura no se aplica a nada.
SEL_PROPIA = {
    "PlateCarrierVest":    ["camoGround", "camoMale", "camoFemale"],
    "PlateCarrierHolster": ["camoGround"],
    "PlateCarrierPouches": ["camoGround"],
    "TortillaBag":         ["camoGround", "camoMale", "camoFemale"],
}

# Extras por pieza (van despues de las texturas, dentro de la clase)
EXTRA = {
    # La mochila es lo unico que cambia MECANICA y no solo color: 10x12 = 120 slots y dos
    # slots de arma. El '+=' conserva los attachments de vanilla (Chemlight, WalkieTalkie,
    # Backpack_1) en vez de pisarlos, que es lo que pasa con '=' en un array de config.
    # OJO: el p3d de la mochila no trae proxy de arma, asi que las armas colgadas se
    # LLEVAN pero no se VEN sobre la espalda. Para que se vean haria falta editar el modelo.
    "TortillaBag": [
        'itemsCargoSize[] = {10, 12};',
        'attachments[] += {"Shoulder", "Melee"};',
    ],
}

CABECERA = '''\t// ==================================================================
\t//  SETS DE ROPA 3xor (retexturizados)
\t// ------------------------------------------------------------------
\t//  Cinco colores x trece piezas. Son ITEMS NUEVOS: cada clase HEREDA de la base vanilla
\t//  y solo cambia 'hiddenSelectionsTextures'. Las bases se declaran sin cuerpo (forward
\t//  declaration), que NO modifica la clase vanilla: los items originales del juego quedan
\t//  intactos, igual que los modelos, que se reusan tal cual. No hay ni un 'modded class'.
\t//
\t//  Las texturas son el _co (color) vanilla recoloreado en HSV conservando la luminancia
\t//  -o sea costuras, correas, sombras y desgaste-. El _nohq (relieve) y el _smdi (brillo)
\t//  NO se tocan: siguen siendo los de vanilla, heredados. Ver tools/recolor_ropa.py, que
\t//  documenta cada paleta y por que esta donde esta.
\t//
\t//  ESTE BLOQUE ES GENERADO. Para agregar un color o una pieza, editar las tablas de
\t//  tools/gen_ropa_config.py y correrlo; reescribe entre los marcadores >>> y <<<.
\t//  Para volver atras: borrar el bloque entero, sus entradas en CfgPatches.units, la
\t//  carpeta data\\ropa y las lineas de types.xml. Nada mas depende de esto.
\t// ==================================================================
'''


def tex_path(color, nombre):
    return '"ExorStorage\\\\data\\\\ropa\\\\exor_%s_%s_co.paa"' % (color.lower(), nombre)


def generar_clases():
    out = [INICIO, CABECERA]

    # forward declarations (sin cuerpo -> no tocan la clase vanilla), sin repetir
    vistos = []
    out.append('\tclass Clothing;\t\t\t\t// externa (DZ_Data)')
    for cls, padre, addon, _, _, _ in PIEZAS:
        if padre in vistos:
            continue
        vistos.append(padre)
        out.append('\tclass %s;\t// externa (%s)' % (padre, addon))
    out.append("")

    for suf, visible in SETS:
        out.append("\t// ---------------- SET %s ----------------" % visible.upper())
        out.append("")
        for cls, padre, addon, nombre, t_piso, t_puesto in PIEZAS:
            cuerpo = []
            cuerpo.append("\t\tscope = 2;")
            cuerpo.append('\t\tdisplayName = "%s %s";' % (nombre, visible))
            cuerpo.append('\t\tdescriptionShort = "%s del set %s de 3xor.";' % (nombre, visible))
            if cls in SEL_PROPIA:
                sels = ", ".join('"%s"' % s for s in SEL_PROPIA[cls])
                cuerpo.append("\t\thiddenSelections[] = {%s};" % sels)
            cuerpo.append("\t\thiddenSelectionsTextures[] = {")
            cuerpo.append("\t\t\t%s," % tex_path(suf, t_piso))
            cuerpo.append("\t\t\t%s," % tex_path(suf, t_puesto))
            cuerpo.append("\t\t\t%s" % tex_path(suf, t_puesto))
            cuerpo.append("\t\t};")
            for e in EXTRA.get(cls, []):
                cuerpo.append("\t\t" + e)
            out.append("\tclass Exor_%s_%s: %s" % (cls, suf, padre))
            out.append("\t{")
            out.extend(cuerpo)
            out.append("\t};")
            out.append("")
    out.append(FIN)
    return "\n".join(out)


def nombres_clases():
    return ["Exor_%s_%s" % (cls, suf) for suf, _ in SETS for cls, _, _, _, _, _ in PIEZAS]


def escribir_config():
    d = open(CONFIG, "rb").read()
    crlf = d.count(b"\r\n") > 0
    def enc(t):
        t = t.replace("\r\n", "\n")
        return (t.replace("\n", "\r\n") if crlf else t).encode("utf-8")

    bloque = generar_clases()
    ini, fin = enc(INICIO), enc(FIN)
    if d.count(ini) == 1 and d.count(fin) == 1:
        a = d.index(ini)
        b = d.index(fin) + len(fin)
        d = d[:a] + enc(bloque) + d[b:]
    else:
        raise SystemExit("No encontre los marcadores en config.cpp; insertarlos a mano una vez.")
    open(CONFIG, "wb").write(d)
    print("config.cpp: %d clases" % len(nombres_clases()))


def escribir_types():
    # nominal=0 + min=0: el CE no los mete en el loot de mundo (salen por cofre/KOTH/admin),
    # pero la entrada hace falta igual para que respeten el lifetime.
    ent = """  <type name="%s">
    <nominal>0</nominal>
    <lifetime>3888000</lifetime>
    <restock>0</restock>
    <min>0</min>
    <quantmin>-1</quantmin>
    <quantmax>-1</quantmax>
    <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0" />
  </type>
"""
    txt = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<types>\n'
           + "".join(ent % n for n in nombres_clases()) + "</types>\n")
    io.open(TYPES, "w", encoding="utf-8", newline="\n").write(txt)
    print("types_3xor_ropa.xml: %d entradas" % len(nombres_clases()))


if __name__ == "__main__":
    escribir_config()
    escribir_types()
    print("\nCfgPatches.units[] debe listar:")
    print(", ".join('"%s"' % n for n in nombres_clases()))
