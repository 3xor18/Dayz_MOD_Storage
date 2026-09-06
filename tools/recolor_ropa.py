#!/usr/bin/env python3
# Recolorea las texturas _co vanilla a las dos paletas del set 3xor.
#
# Metodo: remapeo en HSV que CONSERVA la luminancia y la variacion relativa de tono.
#   - El tono de cada pixel se mide respecto del tono medio de ESA textura (media circular
#     pesada por saturacion), y esa desviacion se comprime alrededor del tono objetivo. Asi
#     las manchas del camo siguen siendo distintas entre si (no queda monocromo) y todas las
#     piezas del set caen en la misma familia de color aunque partan de bases distintas
#     (el plate es marron, los guantes oliva, la gorka gris-verde).
#   - La saturacion se escala y se TOPEA: es lo que separa un rosa sobrio de un rosa chicle.
#   - La luminancia (V) casi no se toca: ahi viven las costuras, correas, desgaste y sombras
#     pintadas. Tocarla es lo que hace que un retexturizado se vea de plastico.
#   - Los pixeles casi neutros (gomas negras de la bota, hebillas) se tinen mucho menos:
#     'sat' original baja -> el remapeo se atenua solo.
import os
import sys
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
BASE = os.path.dirname(os.path.abspath(__file__))
PNG = os.path.join(BASE, "png")
OUT = os.path.join(BASE, "out")


def rgb_to_hsv(a):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    mx = a.max(-1)
    mn = a.min(-1)
    d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-6
    rm = m & (mx == r)
    gm = m & (mx == g) & ~rm
    bm = m & (mx == b) & ~rm & ~gm
    h[rm] = ((g[rm] - b[rm]) / d[rm]) % 6.0
    h[gm] = (b[gm] - r[gm]) / d[gm] + 2.0
    h[bm] = (r[bm] - g[bm]) / d[bm] + 4.0
    h = h / 6.0
    s = np.zeros_like(mx)
    s[mx > 1e-6] = d[mx > 1e-6] / mx[mx > 1e-6]
    return h, s, mx


def hsv_to_rgb(h, s, v):
    i = np.floor(h * 6.0)
    f = h * 6.0 - i
    p = v * (1.0 - s)
    q = v * (1.0 - f * s)
    t = v * (1.0 - (1.0 - f) * s)
    i = (i % 6).astype(np.int32)
    r = np.select([i == 0, i == 1, i == 2, i == 3, i == 4, i == 5], [v, q, p, p, t, v])
    g = np.select([i == 0, i == 1, i == 2, i == 3, i == 4, i == 5], [t, v, v, q, p, p])
    b = np.select([i == 0, i == 1, i == 2, i == 3, i == 4, i == 5], [p, p, t, v, v, q])
    return np.stack([r, g, b], -1)


def hue_medio(h, s, v):
    """Tono dominante de la textura: media circular pesada por saturacion*luminancia
    (los pixeles negros del fondo del UV y las correas no deben decidir el tono)."""
    w = (s * v).astype(np.float64)
    if w.sum() < 1e-6:
        return 0.0
    ang = h.astype(np.float64) * 2.0 * np.pi
    return (np.arctan2((np.sin(ang) * w).sum(), (np.cos(ang) * w).sum()) / (2.0 * np.pi)) % 1.0


def recolor(img, p):
    a = np.asarray(img.convert("RGB"), dtype=np.float32) / 255.0
    h, s, v = rgb_to_hsv(a)
    h0 = hue_medio(h, s, v)

    # Desviacion de tono respecto del tono dominante, por el camino corto del circulo.
    # 'spread' decide cuanta variacion entre manchas sobrevive (0 = monocromo, se pierde el
    # camo) y 'spread_max' RECORTA esa desviacion para que ninguna mancha se escape de la
    # familia de color: sin ese tope, el arido tiraba manchas salmon y el rosa derivaba a lila.
    # El recorte es ASIMETRICO: cada familia de color tiene un lado del circulo al que NO
    # puede asomarse. En el arido, desviarse hacia el rojo da manchas salmon (se veian, y
    # arruinaban el camo); hacia el amarillo-verde da oliva, que es justo lo que hace que
    # funcione en terreno con pasto. Por eso el margen hacia cada lado es distinto.
    dh = (h - h0 + 0.5) % 1.0 - 0.5
    dh = np.clip(dh * p["spread"], p["spread_min"], p["spread_max"])
    hn = (p["hue"] + dh) % 1.0

    # saturacion: escalar, topear, y no dejar que lo casi-neutro se tina de golpe
    sn = np.clip(s * p["sat_scale"] + p["sat_add"], 0.0, 1.0)
    sn = np.minimum(sn, p["sat_max"])
    sn = np.where(s < p["neutro"], s * p["sat_scale"] * 0.35, sn)

    # AUTO-NIVELADO opcional: lleva la luminancia MEDIA de esta textura a 'v_target' con
    # una gamma calculada, ignorando el fondo negro del UV. Hace falta en las paletas
    # acromaticas: el plate y los cascos vanilla ya vienen oscuros de fabrica, asi que sin
    # esto un "urbano" les queda casi negro mientras la campera queda gris medio, y el set
    # se ve despareado. Con esto todas las piezas parten del mismo gris y solo entonces la
    # ventana decide el rango final. En rosa y arido va apagado: el tono ya une el set.
    vv = np.clip(v, 0.0, 1.0)
    if p["v_target"] > 0.0:
        util = vv[vv > 0.02]
        if util.size > 0:
            med = float(np.clip(util.mean(), 0.02, 0.98))
            vv = np.power(vv, np.log(p["v_target"]) / np.log(med))

    # Luminancia: gamma suave + ganancia + un contraste leve alrededor del medio. El
    # contraste es lo que devuelve profundidad: al desaturar, las manchas del camo se
    # acercan entre si y el conjunto se lee lavado.
    vn = np.power(vv, p["val_gamma"]) * p["val_gain"] + p["val_lift"]
    vn = np.clip(0.5 + (vn - 0.5) * p["contraste"], 0.0, 1.0)

    # VENTANA de luminancia: comprime todo el rango a [v_lo, v_hi]. En las paletas de color
    # el tono hace el trabajo, pero nieve y negro son casi acromaticas -ahi lo unico que
    # define el camuflaje es DONDE cae el rango de grises-. Sin esta ventana, "nieve" con
    # solo subir el brillo se quema a blanco plano y se pierde el patron; con ella, el blanco
    # nunca llega a 1.0 y las manchas siguen separadas.
    vn = p["v_lo"] + vn * (p["v_hi"] - p["v_lo"])
    vn = np.where(v < 0.02, v, vn)   # negro del fondo del UV: intacto

    out = hsv_to_rgb(hn, sn, vn)
    return Image.fromarray((np.clip(out, 0, 1) * 255.0 + 0.5).astype(np.uint8), "RGB")


# --------------------------------------------------------------------------
#  PALETAS
# --------------------------------------------------------------------------
# ROSA SOBRIO: rosa viejo / malva. La clave es el tope de saturacion bajo (0.34): a partir
# de ahi entra en territorio fucsia, que es exactamente lo que se ve mal en la referencia.
# Tono 0.945 = ~340 grados (rosa-vino, no magenta), y 'spread' bajo para que no derive a lila.
# Rango de tono resultante: 322-358 grados. Nunca cruza a lila (que empieza a ~300) ni a
# rojo puro, que son los dos lados por donde un rosa se vuelve disfraz.
ROSA = dict(hue=0.945, spread=0.34, spread_min=-0.050, spread_max=0.050,
            sat_scale=1.10, sat_add=0.05, sat_max=0.36, neutro=0.10,
            val_gamma=0.96, val_gain=1.05, val_lift=0.01, contraste=1.14,
            v_lo=0.00, v_hi=1.00, v_target=0.0)

# ARIDO: base arena/khaki con oliva y marron en los medios. Tono 0.105 = ~38 grados.
# 'spread' mas alto que en el rosa: en camuflaje real de desierto la variacion entre manchas
# es lo que rompe la silueta; un arena plano se lee como una mancha lisa a distancia.
# Rango de tono resultante: 27-65 grados = tierra -> arena -> khaki -> oliva. El margen
# hacia el rojo es menos de la mitad que hacia el verde justamente para matar el salmon.
ARIDO = dict(hue=0.105, spread=0.55, spread_min=-0.030, spread_max=0.075,
             sat_scale=0.98, sat_add=0.04, sat_max=0.40, neutro=0.10,
             val_gamma=0.90, val_gain=1.11, val_lift=0.02, contraste=1.08,
             v_lo=0.00, v_hi=1.00, v_target=0.0)

# Las tres de abajo son CASI ACROMATICAS: la saturacion se aplasta y el camuflaje lo hace
# entero el rango de grises. El poquito de tono frio que queda (~215 grados) no se ve como
# azul; evita que el gris salga amarillento, que es como se ve "sucio" en pantalla.
#
# URBANO: gris medio, con el rango bien abierto para que convivan hormigon claro, sombra y
# negro de las aberturas. Es el que mas contraste necesita: en ciudad lo que rompe la
# silueta son los saltos duros, no las transiciones suaves.
URBANO = dict(hue=0.60, spread=0.20, spread_min=-0.040, spread_max=0.040,
              sat_scale=0.20, sat_add=0.0, sat_max=0.10, neutro=0.06,
              val_gamma=0.98, val_gain=1.0, val_lift=0.0, contraste=1.30,
              v_lo=0.10, v_hi=0.82, v_target=0.46)

# NIEVE: blanco sucio con sombras gris-azuladas. El techo en 0.97 y no en 1.0 es a proposito:
# apenas el blanco satura, el patron se quema y queda un mameluco liso.
NIEVE = dict(hue=0.58, spread=0.18, spread_min=-0.035, spread_max=0.035,
             sat_scale=0.22, sat_add=0.0, sat_max=0.09, neutro=0.06,
             val_gamma=0.80, val_gain=1.0, val_lift=0.0, contraste=1.18,
             v_lo=0.58, v_hi=0.97, v_target=0.50)

# NEGRO: no es negro plano. El piso en 0.02 y el techo en 0.30 dejan ver la silueta y las
# costuras; con todo a 0 la prenda se ve como un agujero recortado en la pantalla.
NEGRO = dict(hue=0.62, spread=0.18, spread_min=-0.035, spread_max=0.035,
             sat_scale=0.25, sat_add=0.0, sat_max=0.10, neutro=0.06,
             val_gamma=1.15, val_gain=1.0, val_lift=0.0, contraste=1.22,
             v_lo=0.03, v_hi=0.36, v_target=0.48)

SETS = {"rosa": ROSA, "arido": ARIDO, "urbano": URBANO, "nieve": NIEVE, "negro": NEGRO}

TEXTURAS = ["jacket_ground", "jacket_worn", "pants_ground", "pants_worn",
            "plate", "helmet", "boots", "gloves",
            "mich", "gorkahelmet", "press", "tortilla", "balaclava"]


def main():
    solo = sys.argv[1:] or list(SETS.keys())
    os.makedirs(OUT, exist_ok=True)
    for nombre in solo:
        p = SETS[nombre]
        d = os.path.join(OUT, nombre)
        os.makedirs(d, exist_ok=True)
        for t in TEXTURAS:
            src = os.path.join(PNG, t + ".png")
            im = Image.open(src)
            recolor(im, p).save(os.path.join(d, t + ".png"))
            print(nombre, t, "ok")


if __name__ == "__main__":
    main()
