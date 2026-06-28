// ============================================================================
// 3xor_Vanilla_Optimization - Tracking de PUNTERIA (SOLO server, watchlist)
// Mide la accuracy de los jugadores VIGILADOS de forma robusta y server-authoritative,
// SIN depender de la direccion de apuntado (el server no la expone de forma fiable):
//   - disparos  = Weapon_Base.EEFired (cada bala)
//   - impactos  = PlayerBase.EEHitBy (el server sabe quien fue el atacante)
//   - accuracy  = impactos_a_jugadores / disparos_totales
// Un aimbot pega ~100%; un humano en combate real ~20-50%. Es la señal que NO se puede
// esconder (a diferencia del % de headshots, que el aimbot deja elegir la zona del cuerpo).
//
// DATA-FIRST: se loguea SIEMPRE (cada kill + resumen por vida) con timestamp, sin importar
// umbrales -> el admin ve el % de cada jugador en el tiempo y detecta patrones (a que hora/dia
// togglea el hack). Los umbrales solo agregan una linea PUNTERIA_SOSPECHOSA de atajo.
// ============================================================================

// Agregado por VIDA del tirador (se vuelca al morir/desconectar).
class ExorAcLife
{
	int shots;          // disparos con arma real (sin escopeta) = denominador de accuracy
	int shotgun_shots;  // disparos de escopeta (excluidos: 1 gatillazo = varios perdigones)
	int hits;           // impactos a JUGADORES (sin escopeta) = numerador
	int head;
	int torso;
	int arms;
	int legs;
	int near_hits;      // impactos a <30m
	int mid_hits;       // 30..150m
	int far_hits;       // >150m
	int kills;
}

class ExorAimTrack
{
	static ref map<string, ref ExorAcLife> s_Life;   // shooterSid -> agregado de la vida

	static void EnsureMaps()
	{
		if (!s_Life) s_Life = new map<string, ref ExorAcLife>;
	}

	static ExorAcLife EnsureLife(string sid)
	{
		EnsureMaps();
		ExorAcLife l = s_Life.Get(sid);
		if (!l)
		{
			l = new ExorAcLife();
			s_Life.Set(sid, l);
		}
		return l;
	}

	static string NameOf(PlayerBase p)
	{
		if (p && p.GetIdentity())
			return p.GetIdentity().GetName();
		return "?";
	}

	// nombre legible de la VICTIMA (jugador real o NPC de test)
	static string VictimName(PlayerBase p)
	{
		if (p && p.GetIdentity())
			return p.GetIdentity().GetName();
		return "NPC";
	}

	// true si el tracking aplica a este shooter (vigilado, no exento, feature on)
	static bool Applies(string sid, ExorCfgAnticheat ac)
	{
		if (!ac.habilitado || !ac.watch_check_punteria)
			return false;
		if (sid == "" || ac.EsExento(sid) || !ac.EsVigilado(sid))
			return false;
		return true;
	}

	static int Pct(int part, int whole)
	{
		if (whole <= 0) return 0;
		return Math.Round((part * 100.0) / whole);
	}

	static bool IsPellet(string ammoType)
	{
		string am = ammoType; am.ToLower();
		return am.Contains("pellet");
	}

	// ------------------------- DISPARO (Weapon_Base.EEFired) -------------------------
	static void OnShot(PlayerBase shooter, string weaponCls, string weaponName, string ammoType)
	{
		if (!GetGame() || !GetGame().IsServer() || !shooter || !shooter.GetIdentity())
			return;
		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		string sid = shooter.GetIdentity().GetPlainId();
		if (!Applies(sid, ac))
			return;

		ExorAcLife life = EnsureLife(sid);
		if (IsPellet(ammoType))
			life.shotgun_shots++;
		else
			life.shots++;
	}

	// ------------------------- IMPACTO (PlayerBase.EEHitBy en la victima) -------------------------
	// La victima es un jugador (o NPC de test) y el atacante esta vigilado -> cuenta como impacto.
	static void OnHit(PlayerBase attacker, PlayerBase victim, string dmgZone, string ammoType, int dist)
	{
		if (!GetGame() || !GetGame().IsServer() || !attacker || !victim || !attacker.GetIdentity())
			return;
		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		string sid = attacker.GetIdentity().GetPlainId();
		if (!Applies(sid, ac))
			return;
		if (IsPellet(ammoType))
			return;   // escopeta: excluida del %

		ExorAcLife life = EnsureLife(sid);
		life.hits++;
		string z = dmgZone; z.ToLower();
		if (z.Contains("head") || z.Contains("brain")) life.head++;
		else if (z.Contains("spine") || z.Contains("chest") || z.Contains("torso") || z.Contains("body")) life.torso++;
		else if (z.Contains("arm") || z.Contains("hand")) life.arms++;
		else life.legs++;

		if (dist < 30) life.near_hits++;
		else if (dist <= 150) life.mid_hits++;
		else life.far_hits++;
	}

	// ------------------------- KILL (el vigilado mato a un jugador) -------------------------
	// Loguea CADA kill con la accuracy de la vida hasta ese momento (data por kill, con timestamp).
	static void OnKill(PlayerBase killer, PlayerBase victim, int dist)
	{
		if (!GetGame() || !GetGame().IsServer() || !killer || !victim || !killer.GetIdentity())
			return;
		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		string sid = killer.GetIdentity().GetPlainId();
		if (!Applies(sid, ac))
			return;

		ExorAcLife life = EnsureLife(sid);
		life.kills++;

		int acc = Pct(life.hits, life.shots);
		int hs = Pct(life.head, life.hits);
		string det = string.Format("kill a %1 a %2m | acc vida=%3%% (%4/%5) | HS=%6%% | kills_vida=%7",
			VictimName(victim), dist, acc, life.hits, life.shots, hs, life.kills);
		ExorRaidLog.Write("PUNTERIA_KILL", sid, NameOf(killer), killer.GetPosition(), det, true);
	}

	// ------------------------- fin de vida del tirador (muerte/desconexion) -------------------------
	static void OnPlayerGone(PlayerBase p, string sid)
	{
		if (!GetGame() || !GetGame().IsServer() || sid == "")
			return;
		EnsureMaps();
		ExorAcLife life = s_Life.Get(sid);
		if (life)
		{
			LogLifeSummary(p, sid, life);
			s_Life.Remove(sid);
		}
	}

	// Resumen de la vida: SIEMPRE se loguea (data-first) si hubo algun disparo. Ademas, si cruza
	// umbrales con muestra suficiente, agrega una linea PUNTERIA_SOSPECHOSA con nivel.
	static void LogLifeSummary(PlayerBase p, string sid, ExorAcLife life)
	{
		int totalShots = life.shots + life.shotgun_shots;
		if (totalShots < 1 && life.kills < 1)
			return;   // no disparo ni mato: nada que registrar

		ExorCfgAnticheat ac = GetExorConfig().anticheat;
		int acc = Pct(life.hits, life.shots);
		int hs = Pct(life.head, life.hits);
		vector pos = vector.Zero;
		if (p) pos = p.GetPosition();

		string det = "vida: disparos=" + life.shots.ToString() + " impactos=" + life.hits.ToString() + " acc=" + acc.ToString() + "%";
		det = det + " | HS=" + hs.ToString() + "% (cabeza=" + life.head.ToString() + " torso=" + life.torso.ToString() + " brazos=" + life.arms.ToString() + " piernas=" + life.legs.ToString() + ")";
		det = det + " | impactos x rango: cerca=" + life.near_hits.ToString() + " medio=" + life.mid_hits.ToString() + " LEJOS=" + life.far_hits.ToString();
		det = det + " | escopeta=" + life.shotgun_shots.ToString() + " | kills=" + life.kills.ToString();
		ExorRaidLog.Write("PUNTERIA_RESUMEN", sid, NameOf(p), pos, det);

		// atajo de sospecha (la data cruda de arriba siempre queda igual). Accuracy = señal dominante.
		if (life.shots >= ac.aim_min_shots_resumen)
		{
			string why = "";
			string nivel = "";
			int thrAcc = Math.Round(ac.aim_acc_sospechosa * 100);
			int thrAccAlta = Math.Round(ac.aim_acc_alta * 100);
			int thrHs = Math.Round(ac.aim_hs_sospechosa * 100);

			if (acc >= thrAccAlta)  { nivel = "ALTO";  why = why + " acc=" + acc.ToString() + "% (casi seguro aimbot)"; }
			else if (acc >= thrAcc) { nivel = "MEDIO"; why = why + " acc=" + acc.ToString() + "%"; }
			if (hs >= thrHs)        { if (nivel == "") nivel = "BAJO"; why = why + " HS=" + hs.ToString() + "%"; }

			if (why != "")
				ExorRaidLog.Write("PUNTERIA_SOSPECHOSA", sid, NameOf(p), pos,
					"nivel=" + nivel + " |" + why + " (disparos=" + life.shots.ToString() + " impactos=" + life.hits.ToString() + ")", true);
		}
	}
}

// ===========================================================================
// Hook de DISPARO: el arma EN MANOS de un player dispara. Server-side.
// ===========================================================================
modded class Weapon_Base
{
	override void EEFired(int muzzleType, int mode, string ammoType)
	{
		super.EEFired(muzzleType, mode, ammoType);
		if (!GetGame() || !GetGame().IsServer())
			return;
		PlayerBase shooter = PlayerBase.Cast(GetHierarchyRootPlayer());
		if (!shooter)
			return;
		ExorAimTrack.OnShot(shooter, GetType(), GetDisplayName(), ammoType);
	}
}
