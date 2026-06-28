// ============================================================================
// 3xor_Vanilla_Optimization - Anti-cheat heuristico (SOLO server).
//
// Mide GEOMETRIA y RESULTADOS con los eventos del motor. NO lee memoria ni inputs
// del cliente (eso es BattlEye). Da INDICIOS contados para que el admin revise a
// mano; NUNCA banea solo (hay falsos positivos: lag/hitreg/desync, peeker's
// advantage, penetracion de balas, puertas/ventanas abiertas, drop balistico).
// Todo se escribe EN VIVO al log de auditoria (ExorRaidLog -> ServerAuditLog\).
//
// Diseno (rendimiento): todo gatillado por EVENTO (kill) o por una watchlist chica
// muestreada a ~1 Hz. El raycast (lo caro) solo se lanza cuando un chequeo barato
// de angulo ya salto -> casi nunca se lanza. NO hay escaneo por-frame N x N.
//
//   Feature 1 - detector por KILL (sobre TODOS menos exentos):
//     LOS bloqueada (wallhack) + angulo arma->victima (aimbot/"mira al cielo") +
//     distancia sospechosa por arma. Cuenta senales -> nivel BAJO/MEDIO/ALTO.
//   Feature 2 - WATCHLIST (solo SteamIDs vigilados, muestreo ~1 Hz):
//     apunta (arma en alto) a un oculto = ESP/prefire; corre derecho a un oculto.
// ============================================================================

// Estado runtime por jugador vigilado (no se persiste; vive en RAM mientras corre).
class ExorAcWatchState
{
	vector last_pos;                 // pos del tick anterior (para el vector de movimiento)
	bool has_last;
	int last_ms;                     // uptime ms del tick anterior (para medir dt REAL, no asumir TICK_MS)
	int vel_streak;                  // ticks SEGUIDOS por encima de watch_velocidad_max (exigir sostenido)
	int grace_until_ms;              // hasta este uptime ms se saltean vel/godmode/bajo-tierra (teleport/respawn/conexion)
	ref map<string, int> cooldown;   // clave "tipo:targetsid" -> uptime ms del ultimo log (anti-spam)

	// --- god mode (event-driven en EEHitBy) ---
	float last_health;               // vida en el ultimo impacto recibido
	bool hp_init;                    // ya hay baseline de vida
	int nodrop_hits;                 // impactos reales SEGUIDOS sin que bajara la vida
	int last_hit_ms;                 // uptime ms del ultimo impacto (para resetear si pasa mucho)

	// --- spinbot (tick) ---
	vector last_aim;                 // direccion de mira del tick anterior
	bool has_aim;
	int spin_streak;                 // ticks seguidos de giro imposible

	// --- seguimiento de oculto/lejano (tick) ---
	ref map<string, int> track_streak;   // targetsid -> ticks seguidos rastreando a ese objetivo

	void ExorAcWatchState()
	{
		cooldown = new map<string, int>;
		track_streak = new map<string, int>;
		has_last = false;
		hp_init = false;
		nodrop_hits = 0;
		has_aim = false;
		spin_streak = 0;
	}
}

// Snapshot de la geometria del ULTIMO impacto de un player a esta victima (se guarda en la
// victima, en EEHitBy). El detector por kill lo usa en vez de las posiciones POST-MUERTE
// (victima en ragdoll = ojos/pos basura -> falsos ANGULO/LOS). Capturado con AMBOS vivos en
// el momento real del disparo que mato -> menos falsos positivos + mejor deteccion de
// wallhack (LOS al disparar) y silent-aim (arma server-authoritative no apunta pero pega).
class ExorKillShot
{
	bool valid;
	string killer_sid;   // de quien fue el impacto (debe coincidir con el killer final)
	vector killer_eye;
	vector victim_eye;
	vector victim_pos;   // pos (pies) de la victima al impactar -> para LOS multi-rayo (cabeza/torso/piernas)
	vector aim_dir;      // direccion REAL del cañon del atacante al impactar (memory points del modelo)
	bool has_weapon;     // tenia arma en manos (el check de angulo solo aplica a kills con arma)
	int dist;
	int ms;              // uptime ms del impacto (para descartar snapshots viejos)
}

class ExorAnticheat
{
	static ref ExorAnticheat s_Instance;
	ref map<string, ref ExorAcWatchState> m_Watch;   // steamid vigilado -> estado

	static const int TICK_MS = 1000;   // muestreo de la watchlist (~1 Hz)

	void ExorAnticheat()
	{
		m_Watch = new map<string, ref ExorAcWatchState>;
	}

	static ExorAnticheat Get()
	{
		if (!s_Instance)
			s_Instance = new ExorAnticheat();
		return s_Instance;
	}

	// Arranca el tick de la watchlist. Se llama 1 vez en MissionServer.OnInit.
	static void Start()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().Tick, TICK_MS, true);
		Print(string.Format("%1 Anti-cheat iniciado (tick %2ms)", ExorStorageConstants.LOG, TICK_MS));
	}

	// ------------------------- helpers de config -------------------------
	static bool IsExento(string sid)
	{
		return GetExorConfig().anticheat.EsExento(sid);
	}

	// ------------------------- geometria -------------------------
	// Posicion de los "ojos" del jugador (hueso Head; fallback ~1.5m sobre el piso).
	static vector EyePos(PlayerBase p)
	{
		if (!p)
			return vector.Zero;
		int b = p.GetBoneIndexByName("Head");
		if (b >= 0)
			return p.GetBonePositionWS(b);
		vector pos = p.GetPosition();
		pos[1] = pos[1] + 1.5;
		return pos;
	}

	// Direccion del arma EN MANOS (server-authoritative). hasWeapon=false si no hay arma.
	// Usa la direccion REAL del cañon via los memory points del modelo (chamber "konec hlavne" ->
	// boca "usti hlavne"): es a donde apunta el arma de verdad, con pitch. NO se usa w.GetDirection()
	// porque devuelve el eje del MODELO (offset fijo ~117deg) -> inservible para medir angulos.
	// Fallback (arma sin esos memory points): la direccion del cuerpo (coarse pero fiable).
	static vector AimDir(PlayerBase p, out bool hasWeapon)
	{
		hasWeapon = false;
		if (!p || !p.GetHumanInventory())
			return vector.Zero;
		Weapon_Base w = Weapon_Base.Cast(p.GetHumanInventory().GetEntityInHands());
		if (!w)
			return vector.Zero;
		hasWeapon = true;

		vector konec = w.ModelToWorld(w.GetSelectionPositionMS("konec hlavne"));
		vector usti  = w.ModelToWorld(w.GetSelectionPositionMS("usti hlavne"));
		vector dir = usti - konec;
		if (dir.LengthSq() > 0.01)
		{
			dir.Normalize();
			return dir;
		}
		return p.GetDirection();   // fallback
	}

	// Angulo (grados) entre dos vectores. 0 = misma direccion, 180 = opuestas.
	static float AngleBetweenDeg(vector a, vector b)
	{
		a.Normalize();
		b.Normalize();
		float d = vector.Dot(a, b);
		d = Math.Clamp(d, -1.0, 1.0);
		return Math.Acos(d) * Math.RAD2DEG;
	}

	// Angulo HORIZONTAL (ignora la altura). Para "orientado hacia" un objetivo usando la
	// direccion del CUERPO (server-authoritative), que no tiene un pitch fiable. Devuelve 180
	// (= no orientado) si algun vector horizontal es casi nulo.
	static float AngleHorizDeg(vector a, vector b)
	{
		a[1] = 0;
		b[1] = 0;
		if (a.LengthSq() < 0.0001 || b.LengthSq() < 0.0001)
			return 180;
		return AngleBetweenDeg(a, b);
	}

	// Lanza un raycast 'from'->'to' y devuelve el primer objeto SOLIDO que bloquea la
	// vision (pared/edificio/roca/arbol/auto), o null si la linea esta despejada. Se
	// saltean ambos jugadores y cualquier ser vivo (no cuentan como obstaculo). Como
	// el segmento termina en la victima, cualquier solido golpeado esta ENTRE medio.
	static Object LosBlocker(vector from, vector to, PlayerBase a, PlayerBase b)
	{
		set<Object> hits = new set<Object>;
		vector hitPos, hitNormal;
		int hitComp;
		// with=null, ignore=a (uno de los dos jugadores), sorted=true (cercano primero)
		DayZPhysics.RaycastRV(from, to, hitPos, hitNormal, hitComp, hits, null, a, true, false, ObjIntersectView);
		if (!hits)
			return null;
		int i;
		for (i = 0; i < hits.Count(); i++)
		{
			Object o = hits.Get(i);
			if (!o)
				continue;
			if (o == a || o == b)
				continue;
			if (o.IsMan())                  // otros players NO cuentan como obstaculo (no es wallhack)
				continue;
			if (DayZCreatureAI.Cast(o))     // zombies/animales tampoco bloquean la vision
				continue;
			return o;   // primer solido = obstaculo mas cercano (hits viene ordenado)
		}
		return null;
	}

	// LOS ROBUSTA: chequea la linea a la CABEZA, el TORSO y las PIERNAS de la victima. Si AL MENOS
	// una esta despejada, el atacante PODIA verlo (no es wallhack) -> null. Solo si las TRES estan
	// bloqueadas se considera oculto. Baja falsos por arboles/arbustos/bordes (un solo rayo clipea
	// un tronco fino pero el cuerpo se ve al costado). head = ojos de la victima, feet = su GetPosition.
	static Object LosBlockerMulti(vector from, vector head, vector feet, PlayerBase a, PlayerBase b)
	{
		Object blk = LosBlocker(from, head, a, b);
		if (!blk)
			return null;                        // ve la cabeza
		vector chest = feet; chest[1] = feet[1] + 1.2;
		if (!LosBlocker(from, chest, a, b))
			return null;                        // ve el torso
		vector legs = feet; legs[1] = feet[1] + 0.4;
		if (!LosBlocker(from, legs, a, b))
			return null;                        // ve las piernas
		return blk;                             // las tres tapadas -> oculto real
	}

	// ===========================================================================
	// FEATURE 1 - detector por KILL. Lo llama PlayerBase.ExorBuildKillfeed en cada
	// kill PvP. killer y victim son players reales; dist en metros; weaponName para
	// el log, weaponCls (classname) para el umbral de distancia por arma.
	// ===========================================================================
	static void OnKill(PlayerBase killer, PlayerBase victim, int dist, string weaponName, string weaponCls)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!killer || !victim)
			return;
		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		if (!ac.habilitado || !ac.detector_kill)
			return;

		string ksid = ExorGroupManager.SteamId(killer);
		if (IsExento(ksid))
			return;   // killer exento: no se evalua
		// solo_watchlist=true => SOLO se evalua a los SteamIDs vigilados (lo pedido por
		// el usuario: trakear solo los indicados). false => corre sobre TODOS los kills.
		if (ac.solo_watchlist && !ac.EsVigilado(ksid))
			return;

		int signals = 0;
		string ev = "";

		// Geometria del KILL: preferir el snapshot del ultimo impacto (ambos vivos, momento real
		// del disparo). Si no hay snapshot reciente del mismo killer (ej. murio por sangrado tardio),
		// caer a las posiciones actuales (menos preciso pero es lo que hay).
		vector kEye, vEye, aimD;
		bool hasW;
		int useDist = dist;
		ExorKillShot shot = victim.ExorGetKillShot();
		bool useShot = (shot && shot.valid && shot.killer_sid == ksid && (GetGame().GetTime() - shot.ms) <= 3000);
		if (useShot)
		{
			kEye = shot.killer_eye;
			vEye = shot.victim_eye;
			aimD = shot.aim_dir;
			hasW = shot.has_weapon;
			useDist = shot.dist;
		}
		else
		{
			kEye = EyePos(killer);
			vEye = EyePos(victim);
			aimD = AimDir(killer, hasW);
		}

		// 1) Linea de vision bloqueada (wallhack) - multi-rayo (cabeza/torso/piernas) en el momento
		// del disparo si hay snapshot. Solo cuenta si el cuerpo ENTERO estaba tapado (no un arbol al medio).
		if (ac.kill_check_los)
		{
			vector vFeet = victim.GetPosition();
			if (useShot)
				vFeet = shot.victim_pos;
			Object blk = LosBlockerMulti(kEye, vEye, vFeet, killer, victim);
			if (blk)
			{
				signals++;
				ev = ev + string.Format(" | LOS=BLOQUEADA(%1)", blk.GetType());
			}
		}

		// 2) Angulo CUERPO->victima en HORIZONTAL (mato a alguien que NO esta adelante = silent-aim/
		//    spinbot). Se usa la direccion del cuerpo (no el eje del modelo del arma) y se ignora el
		//    pitch (el server no tiene el cabeceo del arma de forma fiable) -> sin falsos por mirar arriba/abajo.
		if (ac.kill_check_angulo && hasW)
		{
			vector bd = aimD;        bd[1] = 0;
			vector toV = vEye - kEye; toV[1] = 0;
			if (bd.LengthSq() > 0.001 && toV.LengthSq() > 0.001)
			{
				float ang = AngleBetweenDeg(bd, toV);
				if (ang > ac.kill_angulo_grados)
				{
					signals++;
					ev = ev + string.Format(" | ANGULO=%1deg (umbral %2)", Math.Round(ang), Math.Round(ac.kill_angulo_grados));
				}
			}
		}

		// 3) Distancia sospechosa para el arma usada
		if (ac.kill_check_distancia)
		{
			float thr = ac.DistThreshold(weaponCls);
			if (thr > 0 && useDist > thr)
			{
				signals++;
				ev = ev + string.Format(" | DIST=%1m (umbral %2 para %3)", useDist, Math.Round(thr), weaponCls);
			}
		}

		if (signals < ac.kill_min_senales)
			return;

		string nivel = "BAJO";
		if (signals >= 3)
			nivel = "ALTO";
		else if (signals == 2)
			nivel = "MEDIO";

		string detalle = string.Format("mato a %1 (steam=%2) con %3 a %4m | nivel=%5 | senales=%6%7",
			ExorGroupManager.PlayerName(victim), ExorGroupManager.SteamId(victim), weaponName, dist, nivel, signals, ev);
		ExorRaidLog.Write("SOSPECHA_KILL", ksid, ExorGroupManager.PlayerName(killer), killer.GetPosition(), detalle, true);	// immediate: no perder el kill si crashea
	}

	// Guarda en la VICTIMA la geometria del impacto (ambos vivos) para que el detector por kill
	// la use al morir en vez de leer posiciones post-muerte. Lo llama PlayerBase.EEHitBy cuando
	// el atacante es un player. Barato: solo guarda posiciones/mira (el raycast LOS se hace 1 vez
	// al morir). Se gatea igual que OnKill (no exento; si solo_watchlist, solo vigilados).
	static void CaptureKillShot(PlayerBase killer, PlayerBase victim, string dmgZone)
	{
		if (!GetGame() || !GetGame().IsServer() || !killer || !victim || !killer.GetIdentity())
			return;
		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		if (!ac.habilitado || !ac.detector_kill)
			return;
		string ksid = killer.GetIdentity().GetPlainId();
		if (IsExento(ksid))
			return;
		if (ac.solo_watchlist && !ac.EsVigilado(ksid))
			return;

		ExorKillShot s = new ExorKillShot();
		s.valid = true;
		s.killer_sid = ksid;
		s.killer_eye = EyePos(killer);
		s.victim_eye = EyePos(victim);
		s.victim_pos = victim.GetPosition();
		bool hasW;
		s.aim_dir = AimDir(killer, hasW);      // direccion real del cañon en el momento del impacto
		s.has_weapon = hasW;
		s.dist = Math.Round(vector.Distance(killer.GetPosition(), victim.GetPosition()));
		s.ms = GetGame().GetTime();
		victim.ExorSetKillShot(s);
	}

	// ===========================================================================
	// FEATURE 2 - WATCHLIST. Muestreo ~1 Hz sobre los SteamIDs vigilados (pocos).
	// ===========================================================================
	ExorAcWatchState EnsureState(string sid)
	{
		ExorAcWatchState st = m_Watch.Get(sid);
		if (!st)
		{
			st = new ExorAcWatchState();
			m_Watch.Set(sid, st);
		}
		return st;
	}

	// Marca un teleport/respawn/conexion: descarta la PROXIMA medicion de delta (pos/mira/vida)
	// y abre una ventana de gracia (grace_ms) en la que se saltean los chequeos de velocidad,
	// god mode y bajo-tierra. Asi el salto del menu de spawn / SetPosition / la invulnerabilidad
	// de respawn NO generan WATCH_VELOCIDAD ni WATCH_GODMODE falsos.
	// Solo crea estado para VIGILADOS (los unicos que esos chequeos evaluan) -> no infla el map.
	static void MarkTeleport(string sid)
	{
		if (!GetGame() || !GetGame().IsServer() || sid == "")
			return;
		if (!GetExorConfig().anticheat.EsVigilado(sid))
			return;
		ExorAcWatchState st = Get().EnsureState(sid);
		st.has_last = false;     // descarta el proximo delta de posicion (salto del teleport)
		st.has_aim = false;      // y el de spinbot
		st.hp_init = false;      // y el baseline de god mode
		st.nodrop_hits = 0;
		st.vel_streak = 0;
		st.grace_until_ms = GetGame().GetTime() + GetExorConfig().anticheat.grace_ms;
	}

	// true si paso el cooldown para este par (y marca el momento). Anti-spam del log.
	bool CooldownOk(ExorAcWatchState st, string key, int nowMs, int cooldownSec)
	{
		int last = st.cooldown.Get(key);   // 0 si no existe
		if (last != 0 && (nowMs - last) < (cooldownSec * 1000))
			return false;
		st.cooldown.Set(key, nowMs);
		return true;
	}

	// ===========================================================================
	// GOD MODE - event-driven. Lo llama PlayerBase.EEHitBy cuando un vigilado recibe
	// un impacto. Si recibe varios impactos REALES (de otro player/zombie) seguidos y
	// la vida no baja = posible god mode. Reset al bajar vida o si pasa mucho sin pelea.
	// ===========================================================================
	static void OnWatchedHit(PlayerBase victim, EntityAI source)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!victim || !source)
			return;
		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		if (!ac.habilitado || !ac.watch_check_godmode)
			return;
		string sid = ExorGroupManager.SteamId(victim);
		if (sid == "" || IsExento(sid))
			return;
		if (!ac.EsVigilado(sid))
			return;

		// solo impactos de un ATACANTE real (otro player o zombie/animal); ignorar entorno
		Man root = source.GetHierarchyRootPlayer();
		bool realAtk = false;
		if (root && PlayerBase.Cast(root) != victim)
			realAtk = true;
		else if (DayZCreatureAI.Cast(source))
			realAtk = true;
		if (!realAtk)
			return;

		ExorAcWatchState st = Get().EnsureState(sid);
		int nowMs = GetGame().GetTime();
		// ventana de gracia (respawn/teleport): DayZ da invulnerabilidad breve al reaparecer; si lo
		// balean justo ahi recibe impactos sin perder vida -> NO contar como god mode.
		if (nowMs < st.grace_until_ms)
		{
			st.nodrop_hits = 0;
			st.hp_init = false;
			return;
		}
		// resetear la racha si paso mucho desde el ultimo impacto (no es una pelea sostenida)
		if (st.last_hit_ms != 0 && (nowMs - st.last_hit_ms) > 30000)
		{
			st.nodrop_hits = 0;
			st.hp_init = false;
		}
		st.last_hit_ms = nowMs;

		float cur = victim.GetHealth("", "Health");
		if (!st.hp_init)
		{
			st.hp_init = true;
			st.last_health = cur;
			return;   // baseline: todavia no se puede medir el delta
		}
		float delta = st.last_health - cur;   // positivo = perdio vida
		st.last_health = cur;
		if (delta < 0.5)
			st.nodrop_hits = st.nodrop_hits + 1;   // impacto real sin perder vida
		else
			st.nodrop_hits = 0;                     // bajo vida normal -> no es god mode

		if (st.nodrop_hits >= ac.godmode_hits && Get().CooldownOk(st, "godmode", nowMs, ac.watch_log_cooldown_seg))
		{
			string det = string.Format("recibio %1 impactos reales seguidos sin perder vida (vida=%2) = posible god mode",
				st.nodrop_hits, Math.Round(cur));
			ExorRaidLog.Write("WATCH_GODMODE", sid, ExorGroupManager.PlayerName(victim), victim.GetPosition(), det);
			st.nodrop_hits = 0;
		}
	}

	void Tick()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		// Volcar el log de auditoria bufferizado (1x/seg). Va ANTES de los early-returns
		// para que se vuelque aunque el anti-cheat este desactivado (el log lo usan tambien
		// party/anti-raid/etc.). El tick siempre corre (Start lo registra incondicional).
		ExorRaidLog.Flush();

		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		if (!ac.habilitado || !ac.watchlist_activa)
			return;
		if (!ac.watchlist || ac.watchlist.Count() == 0)
			return;
		if (!ac.watch_check_mira && !ac.watch_check_aproximacion && !ac.watch_check_velocidad && !ac.watch_check_bajo_tierra && !ac.watch_check_spinbot)
			return;

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int nowMs = GetGame().GetTime();
		float distMin = ac.watch_dist_min;
		float moveMinSq = 0.04;   // ~0.2m/tick = se esta moviendo (filtra ruido de quieto)

		int wi;
		for (wi = 0; wi < ac.watchlist.Count(); wi++)
		{
			string wsid = ac.watchlist.Get(wi);
			if (wsid == "" || IsExento(wsid))
				continue;
			PlayerBase w = ExorGroupManager.Get().FindOnline(wsid);
			if (!w || !w.IsAlive())
				continue;

			ExorAcWatchState st = EnsureState(wsid);

			bool hasW;
			AimDir(w, hasW);   // solo para saber si tiene arma en manos (el aim del arma no es fiable server-side)
			bool raised = w.IsRaised();
			vector wEye = EyePos(w);
			vector wPos = w.GetPosition();

			// direccion de "donde mira": el arma si la tiene, si no el frente del cuerpo
			// (ambas server-authoritative). La usan spinbot y seguimiento.
			// Direccion hacia donde MIRA: SIEMPRE el cuerpo (server-authoritative, fiable). NO el
			// arma (AimDir/GetDirection del modelo da un offset fijo ~117deg server-side = inservible).
			// El cuerpo sigue el yaw de la camara cuando no hay free-look -> buen proxy horizontal.
			vector lookDir = w.GetDirection();

			// vector de movimiento desde el tick anterior + dt REAL entre muestras.
			// dt real (no asumir TICK_MS fijo): si el tick se atrasa por carga, dividir por el
			// TICK_MS fijo inflaba la velocidad y daba falsos speedhack. Aca se mide el tiempo real.
			vector mv = vector.Zero;
			bool hasMove = false;
			float dtSec = TICK_MS / 1000.0;
			if (st.has_last)
			{
				mv = wPos - st.last_pos;
				if (mv.LengthSq() > moveMinSq)
					hasMove = true;
				if (st.last_ms != 0)
				{
					float d = (nowMs - st.last_ms) / 1000.0;
					if (d > 0)
						dtSec = d;
				}
			}
			st.last_pos = wPos;
			st.last_ms = nowMs;
			st.has_last = true;

			bool enVehiculo = (w.GetCommand_Vehicle() != null);

			// --- velocidad imposible / salto de posicion = speedhack/teleport ---
			// A pie el sprint llega a ~6-7 m/s; por encima de watch_velocidad_max es sospechoso.
			// PERO: (1) un salto enorme de 1 tick (>= teleport_velocidad_max) es un teleport/desync,
			// NO un correr rapido -> se descarta y NO se loguea; (2) un pico de lag de 1 tick apenas
			// arriba del umbral = rubber-band -> se exige que sea SOSTENIDO (vel_min_streak ticks).
			// En la ventana de gracia (teleport/respawn/conexion) se saltea entero.
			if (ac.watch_check_velocidad && hasMove && !enVehiculo && nowMs >= st.grace_until_ms)
			{
				float speed = mv.Length() / dtSec;
				if (speed >= ac.teleport_velocidad_max)
				{
					st.has_last = false;	// salto/desync: descartar la muestra, arrancar limpio el proximo tick
					st.vel_streak = 0;
				}
				else if (speed > ac.watch_velocidad_max)
				{
					st.vel_streak = st.vel_streak + 1;
					if (st.vel_streak >= ac.vel_min_streak && CooldownOk(st, "vel", nowMs, ac.watch_log_cooldown_seg))
					{
						string detV = string.Format("velocidad anomala a pie SOSTENIDA: %1 m/s (umbral %2) por %3 ticks = speedhack",
							Math.Round(speed), Math.Round(ac.watch_velocidad_max), st.vel_streak);
						ExorRaidLog.Write("WATCH_VELOCIDAD", wsid, ExorGroupManager.PlayerName(w), wPos, detV);
						st.vel_streak = 0;
					}
				}
				else
				{
					st.vel_streak = 0;	// velocidad normal: corta la racha
				}
			}

			// --- por DEBAJO del terreno = noclip / glitch bajo el mapa (saltear en gracia) ---
			if (ac.watch_check_bajo_tierra && !enVehiculo && nowMs >= st.grace_until_ms)
			{
				float surfaceY = GetGame().SurfaceY(wPos[0], wPos[2]);
				float dy = wPos[1] - surfaceY;
				if (dy < -ac.watch_bajo_tierra_metros && CooldownOk(st, "bajotierra", nowMs, ac.watch_log_cooldown_seg))
				{
					string detU = string.Format("por debajo del terreno: %1m bajo la superficie = noclip/glitch",
						Math.Round(-dy));
					ExorRaidLog.Write("WATCH_BAJO_TIERRA", wsid, ExorGroupManager.PlayerName(w), wPos, detU);
				}
			}

			// --- spinbot: giro de la mira imposible sostenido entre ticks ---
			if (ac.watch_check_spinbot && !enVehiculo)
			{
				if (st.has_aim)
				{
					float spin = AngleHorizDeg(lookDir, st.last_aim);
					if (spin >= ac.spinbot_grados)
						st.spin_streak = st.spin_streak + 1;
					else
						st.spin_streak = 0;
					if (st.spin_streak >= ac.spinbot_ticks && CooldownOk(st, "spinbot", nowMs, ac.watch_log_cooldown_seg))
					{
						string detSpin = string.Format("giro de mira imposible: >=%1deg/tick por %2 ticks seguidos = spinbot",
							Math.Round(ac.spinbot_grados), st.spin_streak);
						ExorRaidLog.Write("WATCH_SPINBOT", wsid, ExorGroupManager.PlayerName(w), wPos, detSpin);
						st.spin_streak = 0;
					}
				}
				st.last_aim = lookDir;
				st.has_aim = true;
			}

			int pi;
			for (pi = 0; pi < players.Count(); pi++)
			{
				PlayerBase o = PlayerBase.Cast(players.Get(pi));
				if (!o || o == w || !o.IsAlive())
					continue;
				string osid = ExorGroupManager.SteamId(o);
				if (osid == "" || IsExento(osid))
					continue;

				vector oEye = EyePos(o);
				float dist = vector.Distance(wEye, oEye);
				if (dist < distMin)
					continue;   // demasiado cerca: ruido
				vector toO = oEye - wEye;

				// --- orientado (arma en alto) a un jugador OCULTO = ESP/prefire ---
				if (ac.watch_check_mira && hasW && raised)
				{
					float ang = AngleHorizDeg(lookDir, toO);
					if (ang <= ac.watch_angulo_grados)
					{
						Object blk = LosBlockerMulti(wEye, oEye, o.GetPosition(), w, o);
						if (blk && CooldownOk(st, "mira:" + osid, nowMs, ac.watch_log_cooldown_seg))
						{
							string det1 = string.Format("apunto (arma en alto) a %1 (steam=%2) OCULTO a %3m | LOS=BLOQUEADA(%4) | ang=%5deg = ESP/prefire",
								ExorGroupManager.PlayerName(o), osid, Math.Round(dist), blk.GetType(), Math.Round(ang));
							ExorRaidLog.Write("WATCH_MIRA", wsid, ExorGroupManager.PlayerName(w), wPos, det1);
						}
					}
				}

				// --- corre DERECHO hacia un jugador OCULTO = ESP ---
				if (ac.watch_check_aproximacion && hasMove)
				{
					float angM = AngleHorizDeg(mv, toO);
					if (angM <= ac.watch_angulo_grados)
					{
						Object blk2 = LosBlockerMulti(wEye, oEye, o.GetPosition(), w, o);
						if (blk2 && CooldownOk(st, "aprox:" + osid, nowMs, ac.watch_log_cooldown_seg))
						{
							string det2 = string.Format("corrio derecho hacia %1 (steam=%2) OCULTO a %3m | LOS=BLOQUEADA(%4) = ESP",
								ExorGroupManager.PlayerName(o), osid, Math.Round(dist), blk2.GetType());
							ExorRaidLog.Write("WATCH_APROX", wsid, ExorGroupManager.PlayerName(w), wPos, det2);
						}
					}
				}

				// --- SIGUE con la mira a alguien que NO deberia ver, sostenido = ESP ---
				// "no deberia verlo" = esta LEJOS (mas de watch_lejos_metros, el cliente ni lo
				// renderiza) O esta OCULTO tras algo. Para bajar falsos positivos se exige que
				// el rastreo sea SOSTENIDO (watch_track_ticks seguidos sobre el MISMO objetivo):
				// un humano no puede lockear-seguir a un punto que no ve mientras se mueve.
				if (ac.watch_check_seguimiento && dist <= ac.watch_track_dist_max)
				{
					float angT = AngleHorizDeg(lookDir, toO);
					if (angT <= ac.watch_angulo_grados)
					{
						bool shouldntSee = false;
						string reason = "";
						if (dist >= ac.watch_lejos_metros)
						{
							shouldntSee = true;   // demasiado lejos para verlo a simple vista (no hace falta raycast)
							reason = string.Format("a %1m (demasiado lejos para verlo)", Math.Round(dist));
						}
						else
						{
							Object blkT = LosBlockerMulti(wEye, oEye, o.GetPosition(), w, o);   // multi-rayo SOLO si esta orientado y dentro de 'lejos'
							if (blkT)
							{
								shouldntSee = true;
								reason = string.Format("OCULTO a %1m (%2)", Math.Round(dist), blkT.GetType());
							}
						}
						// umbral de ticks: para objetivos MUY lejos (imposible de ver) basta menos
						// (mas delatante); para ocultos cercanos se exige mas (pudo haberlo ojeado).
						int reqTicks = ac.watch_track_ticks;
						if (dist >= ac.watch_lejos_metros)
							reqTicks = ac.watch_track_ticks_lejos;
						int streak = st.track_streak.Get(osid);
						if (shouldntSee)
						{
							streak = streak + 1;
							st.track_streak.Set(osid, streak);
							if (streak >= reqTicks && CooldownOk(st, "track:" + osid, nowMs, ac.watch_log_cooldown_seg))
							{
								string detT = string.Format("siguio con la mira a %1 (steam=%2) %3 por %4 ticks seguidos = ESP (rastrea a quien no deberia ver)",
									ExorGroupManager.PlayerName(o), osid, reason, streak);
								ExorRaidLog.Write("WATCH_SEGUIMIENTO", wsid, ExorGroupManager.PlayerName(w), wPos, detT);
								st.track_streak.Set(osid, 0);
							}
						}
						else
						{
							st.track_streak.Set(osid, 0);   // orientado pero lo VE (cerca y sin obstaculo): no cuenta
						}
					}
					else
					{
						st.track_streak.Set(osid, 0);   // dejo de apuntarle: corta la racha
					}
				}
			}
		}
	}
}
