// ============================================================================
// 3xor_Vanilla_Optimization - Constantes del mod
// ============================================================================
class ExorStorageConstants
{
	static const string MOD_NAME = "3xor_Vanilla_Optimization";
	static const string MOD_VERSION = "2.16.5";
	// Sello de build: SUBIRLO EN CADA EMPAQUE, aunque no cambie MOD_VERSION. Sirve para
	// saber desde el RPT que PBO esta corriendo el server (el de version sola no alcanza:
	// se desplego 2.9.1 con MOD_VERSION todavia en "2.8.0" y los logs pre/post deploy
	// salieron identicos -> imposible confirmar si el deploy habia entrado).
	static const string MOD_BUILD = "2026-09-06-v2165-mangal";
	static const string LOG = "[3xorVO]";
	// DEBUG temporal del ciclo de vida del barril (setear/levantar/abrir/cerrar/item
	// in-out/virtualizar/restaurar/load/save/shutdown). Poner en false (o borrar las
	// llamadas ExorDbg) cuando se termine de diagnosticar el dupe del piso.
	static const bool DEBUG_BARRELS = false;

	// Config en el profile del server (se crea sola con defaults al primer arranque)
	static const string CONFIG_DIR = "$profile:3xorVanillaOptimization";
	// settings.json monolitico viejo: solo se usa como ORIGEN de migracion
	static const string CONFIG_PATH = "$profile:3xorVanillaOptimization\\settings.json";
	static const string CONFIG_MIGRATED = "$profile:3xorVanillaOptimization\\settings.json.migrated";

	// Config modular: 1 JSON por modulo (Fase A)
	static const string CFG_STORAGE   = "$profile:3xorVanillaOptimization\\storage.json";
	static const string CFG_VEHICULOS = "$profile:3xorVanillaOptimization\\vehiculos.json";
	static const string CFG_MUNICION  = "$profile:3xorVanillaOptimization\\municion.json";
	static const string CFG_PARTY     = "$profile:3xorVanillaOptimization\\party.json";
	static const string CFG_SPAWNS    = "$profile:3xorVanillaOptimization\\spawns.json";
	static const string CFG_MAPA      = "$profile:3xorVanillaOptimization\\mapa.json";
	static const string CFG_ITEMS     = "$profile:3xorVanillaOptimization\\items.json";
	static const string CFG_VIP       = "$profile:3xorVanillaOptimization\\vip.json";
	static const string CFG_KILLFEED  = "$profile:3xorVanillaOptimization\\killfeed.json";
	static const string CFG_SERVERINFO = "$profile:3xorVanillaOptimization\\serverinfo.json";
	static const string CFG_CHAT       = "$profile:3xorVanillaOptimization\\chat.json";
	static const string CFG_REPARACION = "$profile:3xorVanillaOptimization\\reparacion.json";
	static const string CFG_BODYCADAVER = "$profile:3xorVanillaOptimization\\bodycadaver.json";
	static const string CFG_AUTORUN   = "$profile:3xorVanillaOptimization\\autorun.json";
	static const string CFG_KOTH      = "$profile:3xorVanillaOptimization\\koth.json";
	static const string CFG_NOBUILD   = "$profile:3xorVanillaOptimization\\nobuild.json";
	static const string CFG_COFRE     = "$profile:3xorVanillaOptimization\\evento_apertura_cofre.json";
	static const string CFG_MENSAJES  = "$profile:3xorVanillaOptimization\\mensajes.json";
	static const string CFG_AUTOS     = "$profile:3xorVanillaOptimization\\codelock_autos.json";
	// candado de autos: 1 JSON de estado por auto (keyed por su id estable), como los lockers.
	// NUNCA se toca el stream de persistencia del auto -> retro-compatible y no corrompe nada.
	static const string CARLOCK_DIR   = "$profile:3xorVanillaOptimization\\CarLocks";

	// Datos del sistema party (Fase B+): grupos persistidos
	static const string GROUPS_DIR = "$profile:3xorVanillaOptimization\\groups";

	// Stats persistentes para el Score del panel de server info
	static const string STATS_FILE = "$profile:3xorVanillaOptimization\\score_board.json";

	// Marcas personales del mapa (PIN) ??? guardadas en el CLIENTE ($profile del cliente),
	// privadas de cada jugador, no van al server. Las maneja ExorMapPins (5_Mission).
	static const string MAP_PINS_FILE = "$profile:3xorVanillaOptimization\\my_map_pins.json";

	// Estado persistente de VIP (fecha de ingreso + usos de equipamiento por ciclo)
	static const string VIP_STATE_FILE = "$profile:3xorVanillaOptimization\\vip_usos_consumidos.json";

	// Datos de virtualizacion (contenido de barriles)
	static const string STORAGE_DIR = "$profile:3xorVanillaOptimization\\storage";

	// REGISTRO de muebles colocados (id/tipo/pos/orient). Un JSON por mueble. Se escribe al
	// colocar/cargar y se BORRA solo al empaquetar (accion del player). Sirve al self-heal:
	// un mueble en el registro que NO esta spawneado = lo despawneo el motor -> recrearlo.
	static const string MUEBLES_REG_DIR = "$profile:3xorVanillaOptimization\\muebles_reg";

	// Datos de virtualizacion del contenido de bolsas de cadaver
	static const string BODYBAG_DIR = "$profile:3xorVanillaOptimization\\bodybags";

	// Un JSON forense por tumba (muerto/pos/fecha/items/looteadores). Se limpia por retencion.
	static const string TUMBAS_DIR = "$profile:3xorVanillaOptimization\\tumbas";

	// Log de auditoria del server (1 archivo por dia: audit_YYYY-MM-DD.txt, auto-purga)
	static const string AUDITLOG_DIR = "$profile:3xorVanillaOptimization\\ServerAuditLog";
	// Garage: vehiculos virtualizados. Un JSON por auto, con el groupId embebido para saber
	// de que clan es (asi el menu del mastil solo muestra los TUYOS).
	static const string VEHICLES_DIR = "$profile:3xorVanillaOptimization\\vehiculos_virt";

	// Ledger anti-farmeo de kills: por par killer->victima (steamid), timestamps de los
	// kills recientes. Persiste para sobrevivir los reinicios cada 4h (si no, la ventana
	// se resetearia en cada arranque). Lo maneja ExorKillFarm (4_World).
	static const string KILLFARM_FILE = "$profile:3xorVanillaOptimization\\killfarm.json";

	// Duracion de las acciones (segundos)
	static const float PACK_SECONDS = 3;
	static const float DEPLOY_SECONDS = 3;

	// Cada cuanto corre el chequeo de DORMIR vehiculos (ms). Lento porque hace
	// chequeos de distancia a jugadores (mas caro).
	static const int TICK_MS = 30000;
	// Cada cuanto se chequea si hay que despertar vehiculos (ms)
	static const int WAKE_TICK_MS = 5000;
	// Tick RAPIDO solo para barriles/bodybags (auto-cierre 10s, virtualizar 30s). Es
	// barato (solo compara timestamps), por eso puede correr seguido.
	static const int BARREL_TICK_MS = 5000;
	// Maximo de barriles que se virtualizan por tick (anti-pico: si muchos quedan
	// idle a la vez, ej. tras un raid, se reparten en varios ticks).
	static const int MAX_VIRT_PER_TICK = 15;
	// Maximo de snapshots EN VIVO (escritura del JSON a disco) por tick. El guardado
	// es I/O sincrona en el hilo del juego; si muchos barriles activos cambian a la vez
	// (raid), encadenar las escrituras genera hitch -> se reparten en varios ticks. El
	// que no entra escribe el proximo tick (el flag dirty persiste, no se pierde nada).
	static const int MAX_SNAPSHOT_PER_TICK = 12;
	// Maximo de barriles que RECONCILIAN (limpian stale + scan del piso) por tick. El
	// reconcile es lo CARO del arranque tras un crash (GetObjectsAtPosition 12m por barril
	// que quedo sin virtualizar). Repartirlo evita un hitch al cargar bases con muchos
	// barriles. Los untouched reconcilian de a poco; el que un player abra reconcilia ya.
	static const int MAX_RECONCILE_PER_TICK = 3;	// bajado 5->3: el floor scan (12m) es lo mas caro;
	// repartirlo en mas ticks suaviza el hitch post-arranque (~125ms->~75ms). Loot-safe: el que un
	// player abra reconcilia YA (ExorRestoreIfNeeded); los untouched se ponen al dia de a poco.

	// ------------------------------------------------------------------------
	// PRESUPUESTO ADAPTATIVO (escalabilidad)
	// ------------------------------------------------------------------------
	// Los cupos de arriba son el TECHO. El manager los escala hacia abajo solo cuando el
	// server viene sufriendo, y los devuelve cuando se recupera. Asi el mismo PBO sirve
	// para 10 jugadores (cupo full, se pone al dia rapido) y para 60 (cupo minimo, prioriza
	// el frame). Nada de esto toca la seguridad del loot: lo que no entra en un tick se
	// hace en el siguiente (los flags dirty/needs-reconcile persisten).
	//
	// Contexto: barriles y muebles tenian presupuestos SEPARADOS, o sea el trabajo maximo
	// por tick era el doble (15+15 virt, 12+12 snapshot, 3+3 reconcile). Ahora comparten
	// un pool unico repartido con cursor rotativo -> ni se starvean entre si ni duplican
	// el pico de CPU/IO.
	static const float ADAPT_FRAME_OK_MS   = 40.0;	// peor frame por debajo de esto = server holgado -> subir cupo
	static const float ADAPT_FRAME_BAD_MS  = 90.0;	// peor frame por encima de esto = server sufriendo -> bajar cupo
	static const float ADAPT_MIN_FACTOR    = 0.25;	// piso: nunca menos del 25% del cupo (si no, nunca se pone al dia)
	static const float ADAPT_STEP_DOWN     = 0.50;	// al sufrir, cortar el cupo a la mitad (reaccion rapida)
	static const float ADAPT_STEP_UP       = 0.10;	// al recuperarse, devolver de a poco (evita oscilar)

	// Umbral para el log de diagnostico del tick. Si un BarrelTick tarda mas que esto, se
	// escribe UNA linea con el desglose por bloque (grep "3xorVO TICK-LENTO"). En operacion
	// normal el tick tarda pocos ms -> no escribe nada.
	static const int TICK_WARN_MS = 25;

	// PESO de una operacion de MUEBLE en el presupuesto por tick. Confirmado por el log
	// TICK-LENTO en produccion: una op de mueble (virtualizar/snapshot de un locker con
	// armas+ropa+cargo anidado = 50-100+ entidades a recrear) cuesta ~7-12ms, contra ~0.5ms
	// de un barril. El presupuesto contaba las dos igual, asi que un tick podia apilar 3-4
	// muebles = 40ms de golpe (los slow ticks observados). Con este peso, una op de mueble
	// consume WEIGHT del cupo -> un tick nunca apila varios muebles caros (el resto espera al
	// proximo, el cursor rotativo garantiza que a todos les toca). NO se aplica al reconcile
	// (cupo chico y critico para el loot post-crash). Ajustar si los slow ticks persisten.
	// 4 -> 6 (27-jul): con peso 4 un tick podia apilar 3 snapshots + 3 virtualizados de
	// mueble; los TICK-LENTO del server real muestran justo eso (202 ms en un tick con 3 ops
	// de mueble, o sea ~67 ms por op). Ademas ahora se guarda mas seguido -abrir el mueble
	// marca sucio, que es lo que arregla la perdida de municion en contenedores anidados- asi
	// que conviene repartir esas escrituras en mas ticks en vez de apilarlas. Con peso 6 el
	// tope por tick baja a 2 snapshots + 2 virtualizados. No se pierde trabajo: el cursor
	// rotativo garantiza que al que no le toco este tick le toca en el siguiente (5 s).
	static const int MUEBLE_BUDGET_WEIGHT = 6;

	// Maximo de BOLSAS DE CADAVER que hacen el chequeo CARO (proximidad -> virtualizar/
	// restaurar) por tick. Medido en produccion 22-jul (test con admin spawneando sets: 564
	// muertes en 5h, 252 tumbas vivas a la vez): el bloque de bolsas se comio 9.2 de los 10.5
	// segundos de ticks lentos, con un pico de 656ms en UN tick (barriles: 1ms de esos 656).
	// Causa: el loop recorria TODAS las bolsas en TODOS los ticks y cada una escaneaba a los
	// ~50 players con Cast+Distance -> 250x50 = 12.500 casts por tick. El FPS caia de 610
	// (0-49 bolsas) a 112 (250+) con la MISMA cantidad de players.
	// Con cursor rotativo + este cupo, 250 bolsas se recorren enteras en ~7 ticks (35s), que
	// es de sobra para un camino que ya es el BACKUP: el restore de verdad es sincrono al
	// abrir la tumba (Open()), que no pasa por aca. El TTL sigue corriendo para TODAS las
	// bolsas en TODOS los ticks (es aritmetica de enteros, no cuesta nada) -> ninguna tumba
	// se queda de mas por este reparto.
	static const int MAX_BAGS_PER_TICK = 40;

	// Maximo de tumbas que de verdad VIRTUALIZAN O RESTAURAN en un tick. El cupo de arriba
	// limita cuantas se REVISAN (comparar distancias es barato); esto limita el trabajo caro.
	// MEDIDO en el banco de pruebas con una tumba de 5 prendas llenas + 45 items sueltos:
	// virtualizar ~4 ms, RESTAURAR ~12 ms. Parece poco de a una, pero el cupo de revision
	// permitia 40 en el MISMO frame, y una oleada de muertes -el final de un raid- es
	// justamente cuando todas cumplen la condicion a la vez: medio segundo de server clavado,
	// en el bloque que ya habia sido el peor (pico historico de 656 ms con 250 tumbas).
	// Loot-safe: la que no entra se hace en el proximo tick (5 s) y el cursor rotativo le
	// garantiza el turno; y abrir la tumba la restaura EN EL ACTO sin pasar por este cupo.
	static const int MAX_BAG_OPS_PER_TICK = 4;

	// Debounce del guardado EN VIVO mientras el contenedor esta ABIERTO. Cada snapshot
	// reserializa el contenedor ENTERO y reescribe su archivo (los JSON de produccion van
	// de 14 KB de mediana a 128 KB el mayor), asi que guardar cada 5s mientras alguien
	// acomoda su base es I/O sincrona repetida sobre los mismos bytes.
	// Loot-safe: Close() y ExorVirtualize() fuerzan el volcado sin esperar, y el flag dirty
	// persiste -> lo unico que cambia es CUANDO se escribe, no SI se escribe.
	static const int SNAP_DEBOUNCE_MS = 15000;

	// Entradas de NIVEL SUPERIOR que restaura un contenedor por frame. Restaurar es lo mas
	// caro que hace el mod (crear y reubicar entidades, recursivo) y antes corria entero en
	// un solo frame: un locker de 500 slots clavaba el server segundos. Con lotes el trabajo
	// se reparte y el peor frame queda acotado.
	// 12 es el punto medio medido: un locker lleno tarda ~40 frames (menos de un segundo de
	// reloj) en llenarse del todo, que es imperceptible porque el inventario del mueble esta
	// bloqueado hasta que termina.
	static const int EXOR_RESTORE_LOTE = 12;
}
