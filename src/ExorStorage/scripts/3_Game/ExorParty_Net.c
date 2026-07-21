// ============================================================================
// 3xor_Vanilla_Optimization - Party: IDs de RPC + utilidades (Fase B)
// IDs altos para no chocar con los ERPCs vanilla ni con otros mods.
// Direccion (S=server, C=cliente):
//   ROSTER_SYNC  S -> C   roster del grupo (JSON) para el menu/HUD
//   INVITE       S -> C   "te invitaron a un party"
//   ACCEPT       C -> S   acepto la invitacion
//   DECLINE      C -> S   rechazo la invitacion
//   LEAVE        C -> S   salir del party
//   KICK         C -> S   (lider) sacar a un miembro por steamid
// El inicio de la invitacion es server-side (accion en el mastil), no necesita RPC.
// ============================================================================
class ExorRPC
{
	static const int ROSTER_SYNC = 49216;
	static const int INVITE      = 49217;
	static const int ACCEPT      = 49218;
	static const int DECLINE     = 49219;
	static const int LEAVE         = 49220;
	static const int KICK          = 49221;
	static const int TERRITORY_SYNC = 49222;	// S -> C: zonas de territorio cercanas (preview de construccion)
	static const int FLAG_STATE     = 49223;	// S -> C: estado de bandera (Fase D) [reservado]
	static const int MEMBER_SYNC    = 49224;	// S -> C: posicion+vida de los miembros (HUD/distancia)
	static const int MARKER_SYNC    = 49225;	// S -> C: marcas del party
	static const int SPAWN_OPEN     = 49226;	// S -> C: abrir pantalla de seleccion de spawn (+lista JSON)
	static const int SPAWN_PICK     = 49227;	// C -> S: el jugador eligio un punto (indice; -1 = base)
	static const int MARKER_ADD     = 49228;	// C -> S: poner marca en una posicion del mundo (x,y,z)
	static const int MARKER_CLEAR   = 49229;	// C -> S: limpiar mis marcas
	static const int CONFIG_SYNC    = 49230;	// S -> C: config relevante al cliente (toggles party/mapa/items)
	static const int KILLFEED       = 49231;	// S -> C: evento de killfeed (muerte PvP / suicidio) a TODOS los clientes
	static const int SCORE_REQ      = 49232;	// C -> S: pedir el leaderboard (al abrir el panel de server info)
	static const int SCORE_DATA     = 49233;	// S -> C: leaderboard (JSON) para la tab Score
	static const int CHAT_SEND      = 49234;	// C -> S: el jugador manda un mensaje de chat (+canal)
	static const int CHAT_MSG       = 49235;	// S -> C: mensaje de chat para mostrar (JSON)
	static const int VIP_STATUS     = 49236;	// S -> C: si el jugador local es VIP (para features VIP del cliente, ej. distancia en marcas)
	static const int SERVERINFO_SYNC = 49237;	// S -> C: texto del panel Server Info (RPC propio: el bundle CONFIG_SYNC se pasaba de tamaño)
	static const int AUTORUN_SET    = 49238;	// C -> S: estado del auto-run del jugador (para que el server tambien simule y no haya rubber-band)
	static const int POP_REQ        = 49239;	// C -> S: pedir la cantidad de jugadores conectados (al abrir el mapa, refrescado)
	static const int POP_COUNT      = 49240;	// S -> C: cantidad de jugadores conectados (para el contador del mapa)
	static const int KOTH_SYNC      = 49241;	// S -> C: estado de la marca del KOTH en el mapa (mostrar/ocultar + pos + color + label)
	static const int PARKING_OPEN   = 49242;	// S -> C: abrir/refrescar el menu de parking (JSON: autos almacenados + reales cerca) [chunked]
	static const int PARKING_VIRT   = 49243;	// C -> S: virtualizar (guardar) el auto real seleccionado (Param2 netId low/high)
	static const int PARKING_SPAWN  = 49244;	// C -> S: desvirtualizar (sacar) el auto almacenado seleccionado (Param1 id)
	static const int LOCK_MODAL_OPEN   = 49245;	// S -> C: abrir el modal de clave del locker (Param1 int mode: 0=meter clave, 1=setear/cambiar)
	static const int LOCK_MODAL_SUBMIT = 49246;	// C -> S: el jugador confirmo el modal (Param2 int mode, string clave)

	// Rango reservado del mod. Se usa para decidir si un RPC es NUESTRO y por lo tanto NO
	// hay que pasarlo a super.OnRPC() (la cadena de los otros mods). Al agregar un RPC
	// nuevo, mantenerlo DENTRO de este rango o ampliar el tope.
	static const int RANGE_MIN = 49216;
	static const int RANGE_MAX = 49299;	// margen para RPCs futuros sin tocar el chequeo
}

// Cache cliente: cantidad de jugadores conectados (la setea el server por POP_COUNT).
// El cliente solo conoce a los players de su burbuja de red, asi que el total real lo
// tiene que mandar el server. Lo lee el contador del mapa.
class ExorPopClient
{
	static int s_Count;
}

// ============================================================================
// Transferencia de strings GRANDES server -> cliente en TROZOS (chunking).
// DayZ revienta la VM del cliente ("String CORRUPTED - FIX OnStoreLoad()") al LEER
// un string de RPC que pasa ~2KB. El panel Server Info (reglas/territorio/etc.) y
// otros JSON largos lo pasaban -> excepcion que rompe el HUD (incl. la grilla de
// storage de los barriles si salta con el inventario abierto). Solucion: partir el
// string en trozos chicos (Param3<idx,total,data>) y reensamblar en el cliente.
// Sin perder texto (a diferencia del recorte por bytes de Score/Territorio).
// ============================================================================
class ExorNetChunk
{
	// Tamano de cada trozo en CHARS. Conservador: el texto en español tiene tildes
	// (2 bytes c/u en UTF-8), asi que 800 chars ~ 1600 bytes peor caso, bajo el ~2KB.
	static const int CHUNK_SIZE = 800;

	// SERVER: parte 'data' y lo manda por 'rpcType' como Param3<int idx, int total, string trozo>.
	// 'target' es el objeto sobre el que se llama RPCSingleParam (el player). Envio GARANTIZADO
	// (ordenado) para que los trozos lleguen completos y en orden al reensamblar.
	static void Send(Object target, PlayerIdentity id, int rpcType, string data)
	{
		if (!target)
			return;
		int len = data.Length();
		int total = len / CHUNK_SIZE;
		if (len % CHUNK_SIZE != 0)
			total = total + 1;
		if (total == 0)
			total = 1;	// string vacio -> 1 trozo vacio (el cliente igual lo aplica)
		int i;
		for (i = 0; i < total; i++)
		{
			int start = i * CHUNK_SIZE;
			int n = CHUNK_SIZE;
			if (start + n > len)
				n = len - start;
			string piece = "";
			if (n > 0)
				piece = data.Substring(start, n);
			target.RPCSingleParam(rpcType, new Param3<int, int, string>(i, total, piece), true, id);
		}
	}
}

// CLIENTE: reensambla los trozos por tipo de RPC. Asume entrega ordenada (RPC
// garantizado): el trozo idx==0 reinicia el buffer; al juntar 'total' trozos
// devuelve el string completo (si no, devuelve "" = aun incompleto).
class ExorBigStringRx
{
	static ref map<int, string> s_Buf;   // rpcType -> texto acumulado
	static ref map<int, int>    s_Need;  // rpcType -> trozos esperados
	static ref map<int, int>    s_Have;  // rpcType -> trozos recibidos

	static string Feed(int rpcType, ParamsReadContext ctx)
	{
		Param3<int, int, string> p = new Param3<int, int, string>(0, 0, "");
		if (!ctx.Read(p))
			return "";
		if (!s_Buf)
		{
			s_Buf = new map<int, string>;
			s_Need = new map<int, int>;
			s_Have = new map<int, int>;
		}
		int idx = p.param1;
		int total = p.param2;
		string piece = p.param3;
		if (total <= 0)
			return "";
		if (idx == 0)
		{
			s_Buf.Set(rpcType, "");
			s_Need.Set(rpcType, total);
			s_Have.Set(rpcType, 0);
		}
		string cur = "";
		s_Buf.Find(rpcType, cur);
		cur = cur + piece;
		s_Buf.Set(rpcType, cur);
		int have = 0;
		s_Have.Find(rpcType, have);
		have = have + 1;
		s_Have.Set(rpcType, have);
		int need = 0;
		s_Need.Find(rpcType, need);
		if (need > 0 && have >= need)
			return cur;	// completo
		return "";
	}
}

// ============================================================================
// KOTH: marca del mapa sincronizada a TODOS los clientes (S -> C por KOTH_SYNC).
// El mapa de este mod (ExorMapMenu) la dibuja con el color del koth. show=false
// = ocultar (koth despawneado/limpiado). Es 1 solo koth activo a la vez.
// ============================================================================
class ExorKothMarkerDTO
{
	bool show;
	int id;        // id del koth (indice del color): permite varias marcas a la vez
	float x;
	float y;
	float z;
	int argb;      // color de la marca (= color del koth: amarillo/verde/morado)
	string label;  // texto de la marca ("Koth", "Koth activo", "Koth completado"...)
}

// Cache cliente de las marcas del koth (por id). El server las setea por KOTH_SYNC;
// las lee ExorMapMenu. Varios koth pueden estar activos a la vez -> varias marcas.
class ExorKothClient
{
	static ref map<int, ref ExorKothMarkerDTO> s_Markers;

	static void Apply(ExorKothMarkerDTO m)
	{
		if (!m)
			return;
		if (!s_Markers)
			s_Markers = new map<int, ref ExorKothMarkerDTO>;
		if (m.show)
			s_Markers.Set(m.id, m);
		else
			s_Markers.Remove(m.id);
	}
}

// Estado VIP del jugador LOCAL en el cliente (lo setea el server por RPC VIP_STATUS).
// Lo usan features VIP client-side (ej. mostrar la distancia en las marcas del party).
class ExorVipClient
{
	static bool s_IsVip;
}

// Mensaje de chat: el server lo serializa y lo manda a los destinatarios (todos =
// global, o dentro del radio = zona). dur/max van adentro para no sincronizar config.
class ExorChatMsg
{
	string name;
	string text;
	int channel;   // 0 = global, 1 = zona
	int dur;       // segundos que dura la linea
	int max;       // maximo de lineas simultaneas EN PANTALLA
	int kind;      // 0 = jugador (nombre azul + texto blanco), 1 = mensaje del SERVER (verde, sin remitente)
	int chars;     // caracteres por linea antes de partir (config: max_caracteres_por_linea)
	int maxlin;    // lineas maximas por ESTE mensaje (config: max_lineas_por_mensaje)
}

// Cola estatica cliente: OnRPC (4_World) encola; la UI (5_Mission) la drena por frame.
class ExorChatQueue
{
	static ref array<ref ExorChatMsg> s_Pending;
	static void Enqueue(ExorChatMsg m)
	{
		if (!s_Pending)
			s_Pending = new array<ref ExorChatMsg>;
		s_Pending.Insert(m);
	}
}

// Estado del canal de chat en el CLIENTE (se togglea con la tecla ".").
class ExorChat
{
	static int s_Channel = 0;   // 0 = global, 1 = zona

	static void Toggle()
	{
		if (s_Channel == 0)
			s_Channel = 1;
		else
			s_Channel = 0;
	}

	static string ChannelName()
	{
		if (s_Channel == 1)
			return "ZONA";
		return "GLOBAL";
	}
}

// DTO del killfeed: el server lo serializa a JSON y lo manda a todos los clientes.
// Incluye duracion/max para que el cliente no necesite sincronizar config aparte.
class ExorKfDTO
{
	string killer;   // nombre del que mato (vacio si suicidio o muerte ambiental)
	string victim;   // nombre de la victima
	string weapon;   // nombre del arma
	int dist;        // distancia en metros
	bool suicide;    // true = "se ha suicidado"
	bool generic;    // true = "X murio" a secas (enfermedad/hambre/sed/sangrado sin fuente)
	string cause;    // muerte AMBIENTAL (mina/claymore/gas/explosivo improvisado/caida): "X murio por <cause>".
	                 // vacio = no ambiental (usar el flujo PvP/suicidio/generico normal)
	string msg;      // ALERTA generica (KOTH): si != "" se muestra este texto tal cual (en msgargb) en vez de una muerte
	int msgargb;     // color ARGB del texto de la alerta generica
	int dur;         // segundos que dura la linea
	int max;         // maximo de lineas simultaneas
}

// Cola estatica de killfeed: el OnRPC (4_World) encola aca; la UI (5_Mission, que
// se compila despues) la drena cada frame. Asi no rompemos el orden de modulos.
class ExorKillfeedQueue
{
	static ref array<ref ExorKfDTO> s_Pending;

	static void Enqueue(ExorKfDTO d)
	{
		if (!s_Pending)
			s_Pending = new array<ref ExorKfDTO>;
		s_Pending.Insert(d);
	}
}

// ID del menu scripteado de seleccion de spawn (alto para no chocar con vanilla)
class ExorMenuIDs
{
	static const int SPAWN = 47210;
	static const int PARTY = 47211;
	static const int MAP   = 47212;
	static const int SERVERINFO = 47213;
	static const int PARKING = 47214;
	static const int LOCKKEY = 47215;
}

// Una fila del leaderboard (stats por jugador). Server la guarda/persiste; se manda
// al cliente como array JSON para la tab Score. Visible en 3_Game (server+cliente).
class ExorStatRow
{
	string steamid;
	string name;
	int kills;
	int deaths;
	int suicides;
	int max_dist;     // distancia (m) del kill PvP mas lejano
	string max_weapon;// arma de ese kill mas lejano
}

class ExorStatsFile
{
	ref array<ref ExorStatRow> rows;

	void ExorStatsFile()
	{
		rows = new array<ref ExorStatRow>;
	}
}

// Cache cliente del leaderboard recibido (lo lee el menu de server info / tab Score).
class ExorScoreClient
{
	static ref ExorStatsFile s_Data;
}

// Utilidad de tiempo real del server para el auto-kick por inactividad.
// Numero de dia monotono (diferencias de dias suficientes para "X dias sin login").
// Aproximado en bordes de mes (usa 31 d/mes), lo que solo retrasa un poco el kick.
class ExorTimeUtil
{
	static int TodayNumber()
	{
		int y, m, d;
		GetYearMonthDay(y, m, d);
		return DayNumber(y, m, d);
	}

	static int DayNumber(int y, int m, int d)
	{
		return (y * 372) + (m * 31) + d;
	}

	// Numero de minuto monotono (reloj real del server). Persistente entre
	// reinicios y con granularidad de minutos (para ventanas tipo "1 min").
	// 1440 min/dia; aprox en bordes de mes (31 d), suficiente para diferencias.
	static int NowMinutes()
	{
		int y, mo, d;
		GetYearMonthDay(y, mo, d);
		int h, mi, s;
		GetHourMinuteSecond(h, mi, s);
		return (DayNumber(y, mo, d) * 1440) + (h * 60) + mi;
	}
}
