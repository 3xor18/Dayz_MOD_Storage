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
	static vector AimDir(PlayerBase p, out bool hasWeapon)
	{
		hasWeapon = false;
		if (!p || !p.GetHumanInventory())
			return vector.Zero;
		Weapon_Base w = Weapon_Base.Cast(p.GetHumanInventory().GetEntityInHands());
		if (!w)
			return vector.Zero;
		hasWeapon = true;
		return w.GetDirection();
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

		vector kEye = EyePos(killer);
		vector vEye = EyePos(victim);

		// 1) Linea de vision bloqueada (wallhack)
		if (ac.kill_check_los)
		{
			Object blk = LosBlocker(kEye, vEye, killer, victim);
			if (blk)
			{
				signals++;
				ev = ev + string.Format(" | LOS=BLOQUEADA(%1)", blk.GetType());
			}
		}

		// 2) Angulo del arma vs la victima (mato sin apuntar = aimbot / "mira al cielo")
		if (ac.kill_check_angulo)
		{
			bool hasW;
			vector aim = AimDir(killer, hasW);
			if (hasW)
			{
				float ang = AngleBetweenDeg(aim, vEye - kEye);
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
			if (thr > 0 && dist > thr)
			{
				signals++;
				ev = ev + string.Format(" | DIST=%1m (umbral %2 para %3)", dist, Math.Round(thr), weaponCls);
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
		ExorRaidLog.Write("SOSPECHA_KILL", ksid, ExorGroupManager.PlayerName(killer), killer.GetPosition(), detalle);
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
			vector aim = AimDir(w, hasW);
			bool raised = w.IsRaised();
			vector wEye = EyePos(w);
			vector wPos = w.GetPosition();

			// direccion de "donde mira": el arma si la tiene, si no el frente del cuerpo
			// (ambas server-authoritative). La usan spinbot y seguimiento.
			vector lookDir;
			if (hasW)
				lookDir = aim;
			else
				lookDir = w.GetDirection();

			// vector de movimiento desde el tick anterior
			vector mv = vector.Zero;
			bool hasMove = false;
			if (st.has_last)
			{
				mv = wPos - st.last_pos;
				if (mv.LengthSq() > moveMinSq)
					hasMove = true;
			}
			st.last_pos = wPos;
			st.has_last = true;

			bool enVehiculo = (w.GetCommand_Vehicle() != null);

			// --- velocidad imposible / salto de posicion = speedhack/teleport ---
			// Se reusa el vector de movimiento (gratis). A pie el sprint llega a ~6-7 m/s;
			// por encima de watch_velocidad_max es sospechoso. Un teleport da un salto
			// enorme de 1 tick. Los vehiculos se saltean (van legitimamente rapido).
			if (ac.watch_check_velocidad && hasMove && !enVehiculo)
			{
				float speed = mv.Length() / (TICK_MS / 1000.0);
				if (speed > ac.watch_velocidad_max && CooldownOk(st, "vel", nowMs, ac.watch_log_cooldown_seg))
				{
					string detV = string.Format("velocidad anomala a pie: %1 m/s (umbral %2) = speedhack/teleport",
						Math.Round(speed), Math.Round(ac.watch_velocidad_max));
					ExorRaidLog.Write("WATCH_VELOCIDAD", wsid, ExorGroupManager.PlayerName(w), wPos, detV);
				}
			}

			// --- por DEBAJO del terreno = noclip / glitch bajo el mapa ---
			if (ac.watch_check_bajo_tierra && !enVehiculo)
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
					float spin = AngleBetweenDeg(lookDir, st.last_aim);
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

				// --- apunta (arma en alto) a un jugador OCULTO = ESP/prefire ---
				if (ac.watch_check_mira && hasW && raised)
				{
					float ang = AngleBetweenDeg(aim, toO);
					if (ang <= ac.watch_angulo_grados)
					{
						Object blk = LosBlocker(wEye, oEye, w, o);
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
					float angM = AngleBetweenDeg(mv, toO);
					if (angM <= ac.watch_angulo_grados)
					{
						Object blk2 = LosBlocker(wEye, oEye, w, o);
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
					float angT = AngleBetweenDeg(lookDir, toO);
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
							Object blkT = LosBlocker(wEye, oEye, w, o);   // raycast SOLO si esta orientado y dentro de 'lejos'
							if (blkT)
							{
								shouldntSee = true;
								reason = string.Format("OCULTO a %1m (%2)", Math.Round(dist), blkT.GetType());
							}
						}
						int streak = st.track_streak.Get(osid);
						if (shouldntSee)
						{
							streak = streak + 1;
							st.track_streak.Set(osid, streak);
							if (streak >= ac.watch_track_ticks && CooldownOk(st, "track:" + osid, nowMs, ac.watch_log_cooldown_seg))
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
