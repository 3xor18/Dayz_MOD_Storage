// ============================================================================
// 3xor_Vanilla_Optimization - KOTH (King of the Hill) - manager (SOLO server)
// ----------------------------------------------------------------------------
// Cada COLOR de koth.json es un koth INDEPENDIENTE (ExorKothRun) con su propio
// ciclo, corriendo en paralelo (varios activos a la vez). Por color se configura:
// min players, osos, zombies (+lista clase_zombie), radio no-spawn-con-player,
// tiempos de inicio/reciclo/completar, coordenadas e items. El resto (radios de
// captura, avisos, bonos, despawn, limpieza, classnames, pallet) es GLOBAL.
// Coordenadas: se elige UNA al azar al programar el ciclo y ESA es la que se anuncia y
// la que se respeta. Si a la hora de aparecer hay gente encima se avisa por chat y se les
// da tiempo para alejarse; si no se alejan, ese koth se descarta y se programa uno NUEVO
// desde cero en otra zona (con su cuenta atras y sus avisos). Ver ExorKothRun.TickOcupado.
// ============================================================================
class ExorKothState
{
	static const int IDLE      = 0;
	static const int SCHEDULED = 1;
	static const int ACTIVE    = 2;
	static const int COMPLETED = 3;
	// La hora de aparecer llego pero la coordenada anunciada tiene gente encima: se les
	// aviso y se les esta dando tiempo para alejarse. Ver ExorKothRun.TickOcupado.
	static const int OCUPADO   = 4;
}

// ----------------------------------------------------------------------------
// Estado en vivo de UN koth (un color)
// ----------------------------------------------------------------------------
class ExorKothRun
{
	int m_ColorIdx;
	int m_State;
	int m_ScheduledAtMs;
	int m_SpawnAtMs;
	int m_ScheduleDelaySec;	// delay del re-arme (para pausar el contador si faltan players)
	ref array<bool> m_WarnFired;
	TerritoryFlag m_Mast;
	Object m_Pallet;
	Object m_Fireworks;
	ref array<Object> m_Animals;
	float m_Progress;
	int m_LastTickMs;
	bool m_CaptureAnnounced;
	int m_AbandonSinceMs;
	int m_LastPctMs;
	int m_CleanupAtMs;
	vector m_Pos;
	int m_SmokeState;
	// gracia por coordenada ocupada: hasta cuando se espera y cuando fue el ultimo aviso
	int m_GraciaHastaMs;
	int m_UltimoAvisoGraciaMs;
	bool m_MarkerShown;
	ref ExorKothMarkerDTO m_LastMarker;

	void ExorKothRun(int colorIdx)
	{
		m_ColorIdx = colorIdx;
		m_Animals = new array<Object>;
		m_WarnFired = new array<bool>;
		m_State = ExorKothState.IDLE;
	}

	ExorCfgKothColor Cfg()
	{
		return GetExorConfig().koth.colores.Get(m_ColorIdx);
	}

	void Start()
	{
		GoScheduled(Cfg().segundos_para_inicio_koth_al_reinicar_server);
	}

	int Online()
	{
		// conteo CACHEADO por BarrelTick (cada 5s). Antes armaba un array y llamaba
		// GetPlayers() por cada run de KOTH, 1 vez por segundo, solo para contar.
		return ExorVO_Manager.s_PopCount;
	}

	// una coordenada cualquiera (para la marca preliminar de los avisos)
	// 'evitar' = coordenada que NO se quiere volver a elegir (la que quedo ocupada y obligo
	// a reprogramar). Con una sola coordenada configurada se devuelve esa igual: no hay a
	// donde mudarse y es mejor repetir que quedarse sin koth.
	vector ResolvePos(ExorCfgKothColor c, vector evitar = "0 0 0")
	{
		if (!c.coordenadas || c.coordenadas.Count() == 0)
			return "0 0 0";
		bool hayQueEvitar = (evitar[0] != 0 || evitar[2] != 0);
		array<int> candidatas = new array<int>;
		int k;
		for (k = 0; k < c.coordenadas.Count(); k++)
		{
			ExorCfgKothCoord cc = c.coordenadas.Get(k);
			if (hayQueEvitar && ExorMath.Dist2DSq(Vector(cc.x, 0, cc.z), Vector(evitar[0], 0, evitar[2])) < 25)
				continue;	// es la misma que hay que evitar (a menos de 5 m)
			candidatas.Insert(k);
		}
		if (candidatas.Count() == 0)
		{
			for (k = 0; k < c.coordenadas.Count(); k++)
				candidatas.Insert(k);
		}
		int idx = candidatas.Get(Math.RandomInt(0, candidatas.Count()));
		ExorCfgKothCoord co = c.coordenadas.Get(idx);
		vector p = Vector(co.x, co.y, co.z);
		if (p[1] <= 0)
			p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	// Cuantos jugadores VIVOS hay dentro de 'radio' de 'pos'. Usa la lista compartida del
	// latido de 1 Hz (ver ExorTick1Hz): no pide la suya.
	int ContarCerca(vector pos, float radio)
	{
		array<Man> players = ExorTick1Hz.Jugadores();
		float r2 = radio * radio;
		int n = 0;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb || !pb.IsAlive())
				continue;
			if (vector.DistanceSq(pb.GetPosition(), pos) <= r2)
				n++;
		}
		return n;
	}

	// ------------------------- transiciones -------------------------
	void GoScheduled(int delaySec, vector evitar = "0 0 0")
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		int now = GetGame().GetTime();
		m_ScheduleDelaySec = delaySec;
		m_ScheduledAtMs = now;
		m_SpawnAtMs = now + (delaySec * 1000);
		m_State = ExorKothState.SCHEDULED;
		m_GraciaHastaMs = 0;
		m_UltimoAvisoGraciaMs = 0;
		m_WarnFired = new array<bool>;
		int i;
		for (i = 0; i < g.segundos_aviso_antes_crear_koth.Count(); i++)
			m_WarnFired.Insert(false);
		m_Pos = ResolvePos(c, evitar);
		Print(string.Format("%1 KOTH[%2] programado en %3s", ExorStorageConstants.LOG, c.color, delaySec));
	}

	void Tick(int now)
	{
		if (m_State == ExorKothState.SCHEDULED)
			TickScheduled(now);
		else if (m_State == ExorKothState.OCUPADO)
			TickOcupado(now);
		else if (m_State == ExorKothState.ACTIVE)
			TickActive(now);
		else if (m_State == ExorKothState.COMPLETED)
			TickCompleted(now);
	}

	void TickScheduled(int now)
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		int argb = ExorKoth.ColorArgb(c.color);

		// faltan players -> koth en pausa: sin avisos ni marca. Se reinicia la
		// cuenta atras cada tick, asi cuando lleguen al minimo avisa desde cero.
		if (c.cantidad_minima_players_online > Online())
		{
			if (m_MarkerShown)
				ClearMarker();
			m_ScheduledAtMs = now;
			m_SpawnAtMs = now + (m_ScheduleDelaySec * 1000);
			int wf;
			for (wf = 0; wf < m_WarnFired.Count(); wf++)
				m_WarnFired.Set(wf, false);
			return;
		}

		int i;
		for (i = 0; i < g.segundos_aviso_antes_crear_koth.Count(); i++)
		{
			if (i >= m_WarnFired.Count())
				continue;
			if (m_WarnFired.Get(i))
				continue;
			int w = g.segundos_aviso_antes_crear_koth.Get(i);
			if ((m_SpawnAtMs - (w * 1000)) < m_ScheduledAtMs)
			{
				m_WarnFired.Set(i, true);
				continue;
			}
			if (now >= m_SpawnAtMs - (w * 1000) && now < m_SpawnAtMs)
			{
				m_WarnFired.Set(i, true);
				int ix = Math.Round(m_Pos[0]);
				int iz = Math.Round(m_Pos[2]);
				ExorKoth.Alert(string.Format("Koth iniciara en %1 en X:%2 Z:%3", ExorKoth.FmtTiempo(w), ix, iz), 13, argb);
				if (g.colocar_marca_mapa_para_todo_el_server)
					SendMarker(true, argb, "Koth");
			}
		}

		if (now >= m_SpawnAtMs)
			SpawnKoth();
	}

	void SpawnKoth()
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		int now = GetGame().GetTime();

		// elegible? (min players)
		if (c.cantidad_minima_players_online > Online())
		{
			m_ScheduledAtMs = now;
			m_SpawnAtMs = now + 30000;
			return;
		}
		// LA COORDENADA ANUNCIADA MANDA.
		// Antes, si habia gente encima se elegia OTRA coordenada en silencio: el koth
		// aparecia en un lugar distinto al que se venia anunciando y la gente que habia
		// viajado hasta ahi no encontraba nada. Ahora la posicion anunciada es la que se
		// respeta: si esta ocupada se avisa por chat y se les da tiempo para alejarse
		// (estado OCUPADO). Si no se alejan, ese koth se descarta y se programa uno NUEVO
		// desde cero en otra zona, con sus propios avisos. Ver TickOcupado.
		if (c.no_spawnear_con_player_cerca_en_metros > 0)
		{
			int cuantos = ContarCerca(m_Pos, c.no_spawnear_con_player_cerca_en_metros);
			if (cuantos > 0)
			{
				EntrarEnGracia(now, cuantos);
				return;
			}
		}

		int argb = ExorKoth.ColorArgb(c.color);
		int smokeIdx = ExorKoth.SmokeIdx(c.color);

		// mastil (guard: NO reclama territorio)
		ExorKoth.s_SpawningKothMast = true;
		Object mo = GetGame().CreateObjectEx(g.clase_mastil, m_Pos, ECE_PLACE_ON_SURFACE);
		ExorKoth.s_SpawningKothMast = false;
		m_Mast = TerritoryFlag.Cast(mo);
		if (m_Mast)
			m_Mast.ExorKothSetup(smokeIdx);

		SpawnAI(c);

		m_Progress = 0;
		m_CaptureAnnounced = false;
		m_AbandonSinceMs = 0;
		m_LastPctMs = now;
		m_LastTickMs = now;
		m_SmokeState = 0;
		m_State = ExorKothState.ACTIVE;

		if (g.avisar_spawn_koth)
		{
			int ix = Math.Round(m_Pos[0]);
			int iz = Math.Round(m_Pos[2]);
			ExorKoth.Alert(string.Format("Koth activo en X:%1 Z:%2", ix, iz), 15, argb);
		}
		if (g.colocar_marca_mapa_para_todo_el_server)
			SendMarker(true, argb, "Koth activo");
		Print(string.Format("%1 KOTH[%2] spawn pos=%3", ExorStorageConstants.LOG, c.color, m_Pos));
	}

	// ------------------------------------------------------------------------
	//  COORDENADA OCUPADA: avisar, esperar, y si no se alejan, reprogramar
	// ------------------------------------------------------------------------
	// El aviso dice CUANTOS hay y QUE va a pasar, para que sea una decision del jugador y
	// no una sorpresa. Se repite cada 30 s mientras dura la gracia.
	void EntrarEnGracia(int now, int cuantos)
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		int argb = ExorKoth.ColorArgb(c.color);

		// gracia en 0 = comportamiento inmediato: se descarta y se reprograma sin esperar
		int graciaMs = g.segundos_gracia_para_alejarse * 1000;
		m_State = ExorKothState.OCUPADO;
		m_GraciaHastaMs = now + graciaMs;
		m_UltimoAvisoGraciaMs = now;

		ExorKoth.Alert(string.Format("Existen %1 players cerca del koth, si no se alejan se spawnea en otra area (%2)",
			cuantos, ExorKoth.FmtTiempo(g.segundos_gracia_para_alejarse)), 15, argb);
		Print(string.Format("%1 KOTH[%2] coordenada ocupada por %3 player(s) -> gracia de %4s", ExorStorageConstants.LOG, c.color, cuantos, g.segundos_gracia_para_alejarse));
	}

	void TickOcupado(int now)
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		int argb = ExorKoth.ColorArgb(c.color);

		int cuantos = 0;
		if (c.no_spawnear_con_player_cerca_en_metros > 0)
			cuantos = ContarCerca(m_Pos, c.no_spawnear_con_player_cerca_en_metros);

		// SE ALEJARON: el koth aparece donde se habia anunciado, que es lo que la gente espera.
		if (cuantos == 0)
		{
			ExorKoth.Alert("Se alejaron del koth: aparece donde estaba anunciado", 11, argb);
			m_State = ExorKothState.SCHEDULED;
			m_SpawnAtMs = now;	// ya
			m_GraciaHastaMs = 0;
			SpawnKoth();
			return;
		}

		// todavia dentro de la gracia: recordarselo cada 30 s
		if (now < m_GraciaHastaMs)
		{
			if (now - m_UltimoAvisoGraciaMs >= 30000)
			{
				m_UltimoAvisoGraciaMs = now;
				int restan = (m_GraciaHastaMs - now) / 1000;
				ExorKoth.Alert(string.Format("Existen %1 players cerca del koth, si no se alejan se spawnea en otra area (%2)",
					cuantos, ExorKoth.FmtTiempo(restan)), 13, argb);
			}
			return;
		}

		// NO SE ALEJARON: este koth se descarta y se programa uno NUEVO desde cero en otra
		// zona. No aparece de golpe en otro lado: arranca su cuenta atras y sus avisos.
		vector ocupada = m_Pos;
		ClearMarker();
		int nuevoDelay = g.segundos_reprogramar_si_no_se_alejan;
		ExorKoth.Alert(string.Format("El koth se cancela por %1 players encima: se creara otro en %2 en OTRA zona",
			cuantos, ExorKoth.FmtTiempo(nuevoDelay)), 15, argb);
		Print(string.Format("%1 KOTH[%2] CANCELADO (no se alejaron) -> nuevo koth en %3s en otra coordenada", ExorStorageConstants.LOG, c.color, nuevoDelay));
		GoScheduled(nuevoDelay, ocupada);
	}

	// infectados (cantidad_zombies, clase de la lista clase_zombie) + osos (spawnear_osos)
	void SpawnAI(ExorCfgKothColor c)
	{
		CleanupAnimals();
		int i;
		if (c.cantidad_zombies > 0 && c.clase_zombie && c.clase_zombie.Count() > 0)
		{
			for (i = 0; i < c.cantidad_zombies; i++)
			{
				string zc = c.clase_zombie.Get(Math.RandomInt(0, c.clase_zombie.Count()));
				vector zp = RandAround(3, 10);
				Object z = GetGame().CreateObject(zc, zp, false, true, true);
				if (z)
					m_Animals.Insert(z);
			}
		}
		if (c.spawnear_osos > 0)
		{
			for (i = 0; i < c.spawnear_osos; i++)
			{
				vector bp = RandAround(5, 15);
				Object b = GetGame().CreateObject("Animal_UrsusArctos", bp, false, true, true);
				if (b)
					m_Animals.Insert(b);
			}
		}
	}

	vector RandAround(float rmin, float rmax)
	{
		float ang = Math.RandomFloat(0, Math.PI2);
		float rad = Math.RandomFloat(rmin, rmax);
		vector p = m_Pos + Vector(Math.Cos(ang) * rad, 0, Math.Sin(ang) * rad);
		p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	void TickActive(int now)
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		int smokeIdx = ExorKoth.SmokeIdx(c.color);
		int argb = ExorKoth.ColorArgb(c.color);

		float dt = (now - m_LastTickMs) / 1000.0;
		m_LastTickMs = now;
		if (dt < 0)
			dt = 0;

		int capN;
		int proxN;
		int nearN;
		string names;
		CountAround(g, capN, proxN, nearN, names);

		if (capN >= 1)
		{
			m_AbandonSinceMs = 0;
			if (!m_CaptureAnnounced && g.avisar_que_un_player_inicico_koth)
			{
				ExorKoth.Alert("Koth lo estan capturando!", 11, argb);
				m_CaptureAnnounced = true;
			}
			if (m_SmokeState != smokeIdx)
			{
				if (m_Mast)
					m_Mast.ExorKothSetSmoke(smokeIdx);
				m_SmokeState = smokeIdx;
			}
			int eff = capN;
			if (eff > g.cantidad_maxima_players_en_bono)
				eff = g.cantidad_maxima_players_en_bono;
			int extra = eff - 1;
			float mult = 1.0 + (extra * (g.bonus_por_mas_de_un_player_en_radio_en_procentaje_por_player / 100.0));
			if (proxN > 0)
				mult = mult + (g.bonus_proximidad_mastil_menos_de_5_metros_en_procentaje_solo_cuenta_1_player / 100.0);

			float baseSec = c.segundos_para_completar_koth;
			if (baseSec < 1)
				baseSec = 1;
			m_Progress = m_Progress + ((dt / baseSec) * mult);
			if (m_Progress > 1)
				m_Progress = 1;
			if (m_Mast)
				m_Mast.ExorKothRaise(m_Progress);

			if (g.segundos_aviso_porcentaje_completado > 0 && m_Progress < 1)
			{
				if (now - m_LastPctMs >= g.segundos_aviso_porcentaje_completado * 1000)
				{
					m_LastPctMs = now;
					int pct = Math.Round(m_Progress * 100);
					ExorKoth.Alert("Koth al " + pct.ToString() + "%", 11, argb);
				}
			}

			if (m_Progress >= 1)
			{
				CompleteKoth(g, c, names);
				return;
			}
		}
		else
		{
			if (m_SmokeState != 0)
			{
				if (m_Mast)
					m_Mast.ExorKothSetSmoke(0);
				m_SmokeState = 0;
			}
			if (nearN == 0)
			{
				if (m_AbandonSinceMs == 0)
					m_AbandonSinceMs = now;
				if (g.segundos_despawner_koth_sin_player_cerca > 0 && (now - m_AbandonSinceMs) >= g.segundos_despawner_koth_sin_player_cerca * 1000)
				{
					DespawnKoth();
					return;
				}
			}
			else
			{
				m_AbandonSinceMs = 0;
			}
		}
	}

	void CountAround(ExorCfgKoth g, out int capN, out int proxN, out int nearN, out string namesInCap)
	{
		capN = 0;
		proxN = 0;
		nearN = 0;
		namesInCap = "";
		// Lista compartida del latido de 1 Hz. Antes esto pedia la suya POR EVENTO KOTH
		// ACTIVO y por segundo: con varios eventos a la vez eran varios GetPlayers() y
		// varios arrays nuevos por segundo para responder exactamente lo mismo.
		array<Man> players = ExorTick1Hz.Jugadores();
		// radios al CUADRADO: comparar d2 evita una raiz cuadrada por jugador por segundo
		float r2cap  = g.metros_cercania_player_para_completar * g.metros_cercania_player_para_completar;
		float r2prox = g.metros_proximidad_mastil_para_bonus * g.metros_proximidad_mastil_para_bonus;
		float r2near = g.metros_para_detectar_falta_player * g.metros_para_detectar_falta_player;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb || !pb.IsAlive())
				continue;
			float d = vector.DistanceSq(pb.GetPosition(), m_Pos);
			if (d <= r2cap)
			{
				capN++;
				string nm = "jugador";
				if (pb.GetIdentity())
					nm = pb.GetIdentity().GetName();
				if (namesInCap != "")
					namesInCap = namesInCap + ", ";
				namesInCap = namesInCap + nm;
			}
			if (d <= r2prox)
				proxN++;
			if (d <= r2near)
				nearN++;
		}
	}

	void CompleteKoth(ExorCfgKoth g, ExorCfgKothColor c, string names)
	{
		int now = GetGame().GetTime();
		int argb = ExorKoth.ColorArgb(c.color);
		vector pos = m_Pos;

		string who = names;
		if (who == "")
			who = "los jugadores";
		ExorKoth.Alert(string.Format("Koth completado por: %1", who), 17, argb);

		if (m_Mast)
		{
			GetGame().ObjectDelete(m_Mast);
			m_Mast = null;
		}

		string palletCls = ExorKoth.PalletClassFor(g, c.color);
		m_Pallet = GetGame().CreateObjectEx(palletCls, pos, ECE_PLACE_ON_SURFACE);
		if (m_Pallet)
		{
			m_Pallet.SetOrientation(Vector(g.pallet_yaw, g.pallet_pitch, g.pallet_roll));
			vector pp = m_Pallet.GetPosition();
			pp[1] = pp[1] + g.altura_extra_pallet;
			m_Pallet.SetPosition(pp);
			Barrel_ColorBase brl = Barrel_ColorBase.Cast(m_Pallet);
			if (brl)
				brl.Open();	// el barril cerrado rechaza el cargo y esconde los items
		}
		RollAndFill(c, m_Pallet, pos);

		if (g.clase_fuegos_artificiales != "")
		{
			vector fpos = pos + Vector(g.metros_fuegos_lejos_del_pallet, 0, 0);
			fpos[1] = GetGame().SurfaceY(fpos[0], fpos[2]);
			m_Fireworks = GetGame().CreateObjectEx(g.clase_fuegos_artificiales, fpos, ECE_PLACE_ON_SURFACE);
			FireworksBase fb = FireworksBase.Cast(m_Fireworks);
			if (fb)
				fb.ExorKothIgnite();
		}

		CleanupAnimals();

		if (g.colocar_marca_mapa_para_todo_el_server)
			SendMarker(true, argb, "Koth completado");

		m_CleanupAtMs = now + (g.segundos_limpiar_cosas_al_completar_koth * 1000);
		m_State = ExorKothState.COMPLETED;
	}

	void RollAndFill(ExorCfgKothColor c, Object container, vector pos)
	{
		EntityAI cont = EntityAI.Cast(container);
		if (!cont || !c.item)
			return;
		int puestos = 0;
		int nospace = 0;
		int i;
		for (i = 0; i < c.item.Count(); i++)
		{
			ExorCfgKothItem it = c.item.Get(i);
			if (!it || it.classname == "")
				continue;
			int roll = Math.RandomInt(0, 100);
			if (roll >= it.probabilidad_drop_en_porcentaje_maximo_100)
				continue;
			EntityAI e = cont.GetInventory().CreateInInventory(it.classname);
			if (e)
			{
				puestos++;
			}
			else
			{
				nospace++;
				Print(string.Format("%1 KOTH[%2] item NO entro (classname invalido / mod no cargado / lleno): %3", ExorStorageConstants.LOG, c.color, it.classname));
			}
		}
		Print(string.Format("%1 KOTH[%2] recompensa: %3 puestos, %4 no entraron", ExorStorageConstants.LOG, c.color, puestos, nospace));
	}

	void TickCompleted(int now)
	{
		ExorCfgKoth g = GetExorConfig().koth;
		ExorCfgKothColor c = Cfg();
		if (now < m_CleanupAtMs)
			return;
		if (g.metros_limpieza_sin_player_cerca > 0 && ExorVO_Manager.IsAlivePlayerNear(m_Pos, g.metros_limpieza_sin_player_cerca))
			return;	// hay alguien looteando: esperar
		CleanupRewards();
		ClearMarker();
		GoScheduled(c.segundos_spawnear_nuevo_koth_tras_completar_otro);
	}

	void DespawnKoth()
	{
		ExorCfgKothColor c = Cfg();
		int argb = ExorKoth.ColorArgb(c.color);
		ExorKoth.Alert("Koth desaparecio (nadie lo capturo)", 13, argb);
		if (m_Mast)
		{
			GetGame().ObjectDelete(m_Mast);
			m_Mast = null;
		}
		CleanupAnimals();
		ClearMarker();
		GoScheduled(c.segundos_spawnear_nuevo_koth_tras_completar_otro);
	}

	// ------------------------- limpieza -------------------------
	void CleanupAnimals()
	{
		if (!m_Animals)
			return;
		int i;
		for (i = 0; i < m_Animals.Count(); i++)
		{
			if (m_Animals.Get(i))
				GetGame().ObjectDelete(m_Animals.Get(i));
		}
		m_Animals.Clear();
	}

	void CleanupRewards()
	{
		if (m_Pallet)
		{
			GetGame().ObjectDelete(m_Pallet);
			m_Pallet = null;
		}
		if (m_Fireworks)
		{
			GetGame().ObjectDelete(m_Fireworks);
			m_Fireworks = null;
		}
		CleanupAnimals();
	}

	// ------------------------- marca del mapa -------------------------
	void SendMarker(bool show, int argb, string label)
	{
		ExorKothMarkerDTO d = new ExorKothMarkerDTO();
		d.show = show;
		d.id = m_ColorIdx;
		d.x = m_Pos[0];
		d.y = m_Pos[1];
		d.z = m_Pos[2];
		d.argb = argb;
		d.label = label;
		m_LastMarker = d;
		m_MarkerShown = show;
		ExorKoth.BroadcastMarker(d);
	}

	void ClearMarker()
	{
		SendMarker(false, 0, "");
	}

	void SyncMarkerToPlayer(PlayerBase pb)
	{
		if (!m_MarkerShown || !m_LastMarker || !pb || !pb.GetIdentity())
			return;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(m_LastMarker, false, data);
		pb.RPCSingleParam(ExorRPC.KOTH_SYNC, new Param1<string>(data), true, pb.GetIdentity());
	}
}

// ============================================================================
// Manager: un ExorKothRun por color, todos tickeados en paralelo.
// ============================================================================
class ExorKoth
{
	static ref ExorKoth s_Instance;
	static bool s_SpawningKothMast;	// lo lee TerritoryFlag.EEInit (mastil de koth no reclama territorio)

	ref array<ref ExorKothRun> m_Runs;

	void ExorKoth()
	{
		m_Runs = new array<ref ExorKothRun>;
	}

	static ExorKoth Get()
	{
		if (!s_Instance)
			s_Instance = new ExorKoth();
		return s_Instance;
	}

	static void Start()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgKoth cfg = GetExorConfig().koth;
		if (!cfg.activar)
		{
			Print(string.Format("%1 KOTH desactivado (koth.json activar=false)", ExorStorageConstants.LOG));
			return;
		}
		if (!cfg.colores || cfg.colores.Count() == 0)
		{
			Print(string.Format("%1 KOTH sin ubicaciones (colores vacio): no arranca", ExorStorageConstants.LOG));
			return;
		}
		int i;
		for (i = 0; i < cfg.colores.Count(); i++)
		{
			if (!cfg.colores.Get(i))
				continue;
			ExorKothRun run = new ExorKothRun(i);
			Get().m_Runs.Insert(run);
			run.Start();
		}
		// El latido de 1 Hz lo da ExorTick1Hz (uno solo para party/KOTH/cofre): asi la lista
		// de jugadores y la hora local se calculan una vez y no tres.
		ExorTick1Hz.Start();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().CleanupOrphans, 15000, false);
		Print(string.Format("%1 KOTH iniciado (%2 koth configurados)", ExorStorageConstants.LOG, cfg.colores.Count()));
	}

	// Envoltorio cronometrado: mide cuanto tarda ESTE subsistema por tick y solo loguea
	// si se pasa del umbral. Se agenda este en vez de Tick() directo. Ver ExorPerfMonitor.
	void TickTimed()
	{
		int t = ExorPerfMonitor.Now();
		Tick();
		ExorPerfMonitor.Fin("koth", t);
	}

	void Tick()
	{
		if (!GetGame().IsServer())
			return;
		if (!GetExorConfig().koth.activar)
			return;
		int now = GetGame().GetTime();
		int i;
		for (i = 0; i < m_Runs.Count(); i++)
		{
			if (m_Runs.Get(i))
				m_Runs.Get(i).Tick(now);
		}
	}

	void SyncMarkersToPlayer(PlayerBase pb)
	{
		int i;
		for (i = 0; i < m_Runs.Count(); i++)
		{
			if (m_Runs.Get(i))
				m_Runs.Get(i).SyncMarkerToPlayer(pb);
		}
	}

	void CleanupOrphans()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgKoth cfg = GetExorConfig().koth;
		if (!cfg.colores)
			return;
		string fwCls = cfg.clase_fuegos_artificiales;
		int total = 0;
		int i;
		for (i = 0; i < cfg.colores.Count(); i++)
		{
			ExorCfgKothColor c = cfg.colores.Get(i);
			if (!c || !c.coordenadas)
				continue;
			int k;
			for (k = 0; k < c.coordenadas.Count(); k++)
			{
				ExorCfgKothCoord co = c.coordenadas.Get(k);
				vector p = Vector(co.x, co.y, co.z);
				if (p[1] <= 0)
					p[1] = GetGame().SurfaceY(p[0], p[2]);
				array<Object> objs = new array<Object>;
				array<CargoBase> cg = new array<CargoBase>;
				GetGame().GetObjectsAtPosition3D(p, 30.0, objs, cg);
				int j;
				for (j = 0; j < objs.Count(); j++)
				{
					Object o = objs.Get(j);
					if (!o)
						continue;
					string t = o.GetType();
					bool esCrate = (t == "Exor_KothCrate_1" || t == "Exor_KothCrate_2" || t == "Exor_KothCrate_3");
					bool esFuego = (fwCls != "" && o.IsKindOf(fwCls));
					if (esCrate || esFuego)
					{
						GetGame().ObjectDelete(o);
						total++;
					}
				}
			}
		}
		if (total > 0)
			Print(string.Format("%1 KOTH: %2 objetos huerfanos limpiados al arrancar", ExorStorageConstants.LOG, total));
	}

	// ------------------------- helpers estaticos -------------------------
	static int ColorArgb(string c)
	{
		if (c == "amarillo")
			return ARGB(255, 240, 210, 30);
		if (c == "verde")
			return ARGB(255, 60, 220, 60);
		if (c == "morado")
			return ARGB(255, 170, 80, 220);
		return ARGB(255, 240, 240, 240);
	}

	static int SmokeIdx(string c)
	{
		if (c == "amarillo")
			return 1;
		if (c == "verde")
			return 2;
		if (c == "morado")
			return 3;
		return 0;
	}

	static string PalletClassFor(ExorCfgKoth g, string color)
	{
		if (g.clase_pallet_recompensa != "")
			return g.clase_pallet_recompensa;
		if (color == "verde")
			return "Exor_KothCrate_2";
		if (color == "morado")
			return "Exor_KothCrate_3";
		return "Exor_KothCrate_1";
	}

	static string FmtTiempo(int seg)
	{
		if (seg >= 60)
			return (seg / 60).ToString() + " minutos";
		return seg.ToString() + " segundos";
	}

	static void Alert(string msg, int durSec, int argb)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorKfDTO d = new ExorKfDTO();
		d.msg = msg;
		d.msgargb = argb;	// color de la palabra "Koth" (= color del koth que avisa)
		d.dur = durSec;
		d.max = 5;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(d, false, data);
		Param1<string> p = new Param1<string>(data);
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				pb.RPCSingleParam(ExorRPC.KILLFEED, p, true, pb.GetIdentity());
		}
		Print(string.Format("%1 KOTH: %2", ExorStorageConstants.LOG, msg));
	}

	static void BroadcastMarker(ExorKothMarkerDTO d)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(d, false, data);
		Param1<string> p = new Param1<string>(data);
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				pb.RPCSingleParam(ExorRPC.KOTH_SYNC, p, true, pb.GetIdentity());
		}
	}
}
