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
}

// ID del menu scripteado de seleccion de spawn (alto para no chocar con vanilla)
class ExorMenuIDs
{
	static const int SPAWN = 47210;
	static const int PARTY = 47211;
	static const int MAP   = 47212;
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
