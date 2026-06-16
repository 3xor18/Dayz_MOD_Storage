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
}

// Mensaje de chat: el server lo serializa y lo manda a los destinatarios (todos =
// global, o dentro del radio = zona). dur/max van adentro para no sincronizar config.
class ExorChatMsg
{
	string name;
	string text;
	int channel;   // 0 = global, 1 = zona
	int dur;       // segundos que dura la linea
	int max;       // maximo de lineas simultaneas
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
	string killer;   // nombre del que mato (vacio si suicidio)
	string victim;   // nombre de la victima
	string weapon;   // nombre del arma
	int dist;        // distancia en metros
	bool suicide;    // true = "se ha suicidado"
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
