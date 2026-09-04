// ============================================================================
// 3xor_Vanilla_Optimization - Anti-raid (SOLO server). Todo gateado por el
// radio del mastil: SOLO aplica dentro de territorio AJENO (un mastil que no es
// de tu party). Fuera del radio de cualquier mastil, nada de esto interviene.
//   #2 No desmantelar muros/torres en territorio ajeno  -> ActionDismantlePart
//   #3 Log de robo (tomar items dentro de territorio ajeno) -> ItemBase.OnInventoryEnter
//   #4 Log de deslogueo + sacar al reconectar dentro de territorio ajeno -> Mission
// El log forense lo escribe ExorRaidLog (1 archivo por dia, auto-purga 7 dias).
// ============================================================================
// ===========================================================================
// Combat-log por ZONA (server). Cuando hay daño PvP se registra una zona de
// combate en la posicion de CADA participante (tirador + victima), asi cubre PvP
// corto y largo sin un radio gigante. La zona vive 'combat_log_minutos' y se
// refresca con cada daño cercano. Al desloguearse, si el jugador esta dentro del
// radio de una zona viva = combat-log (por PRESENCIA, haya disparado o no).
// SIN escaneo por tick: la zona solo se evalua en el momento de desconectar.
// ===========================================================================
class ExorCombatZone
{
	float x;
	float z;
	int last_min;   // NowMinutes de la ultima actividad PvP de la zona
}

class ExorCombatZones
{
	static ref array<ref ExorCombatZone> s_Zones;

	static float Dist2D(float ax, float az, float bx, float bz)
	{
		float dx = ax - bx;
		float dz = az - bz;
		return Math.Sqrt((dx * dx) + (dz * dz));
	}

	static void PurgeExpired(int now, int ventana)
	{
		if (!s_Zones)
			return;
		int i;
		for (i = s_Zones.Count() - 1; i >= 0; i--)
		{
			if (now - s_Zones.Get(i).last_min > ventana)
				s_Zones.Remove(i);
		}
	}

	// Registra/refresca una zona en 'pos'. Si ya hay una zona dentro del radio, solo
	// le actualiza el timestamp (un tiroteo largo no spamea zonas). Distancia
	// horizontal: ignora la altura (torres / sotanos cuentan igual).
	static void Register(vector pos)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.log_combat_log || p.combat_log_minutos <= 0)
			return;
		if (!s_Zones)
			s_Zones = new array<ref ExorCombatZone>;
		int now = ExorTimeUtil.NowMinutes();
		PurgeExpired(now, p.combat_log_minutos);
		float radio = p.combat_log_radio;
		int i;
		for (i = 0; i < s_Zones.Count(); i++)
		{
			ExorCombatZone z = s_Zones.Get(i);
			if (Dist2D(pos[0], pos[2], z.x, z.z) <= radio)
			{
				z.last_min = now;
				return;
			}
		}
		ExorCombatZone nz = new ExorCombatZone();
		nz.x = pos[0];
		nz.z = pos[2];
		nz.last_min = now;
		s_Zones.Insert(nz);
	}

	// true si 'pos' cae dentro de una zona viva; devuelve el elapsed (min) de la mas reciente.
	static bool IsInActiveZone(vector pos, out int elapsedMin)
	{
		elapsedMin = -1;
		if (!s_Zones)
			return false;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		int now = ExorTimeUtil.NowMinutes();
		int ventana = p.combat_log_minutos;
		float radio = p.combat_log_radio;
		bool found = false;
		int i;
		for (i = 0; i < s_Zones.Count(); i++)
		{
			ExorCombatZone z = s_Zones.Get(i);
			if (now - z.last_min > ventana)
				continue;
			if (Dist2D(pos[0], pos[2], z.x, z.z) <= radio)
			{
				int e = now - z.last_min;
				if (!found || e < elapsedMin)
				{
					elapsedMin = e;
					found = true;
				}
			}
		}
		return found;
	}
}

class ExorAntiRaid
{
	// ------------------------- helpers de territorio -------------------------

	// El mastil AJENO en cuyo radio cae 'player' (o null si esta fuera de todo
	// territorio ajeno / parado en territorio propio).
	// Pasa por el CACHE por jugador: esto lo llama el hook de pickup de cada item y la
	// respuesta es la misma mientras el jugador no se mueva. Ver ExorTerritoryProbe.
	static TerritoryFlag EnemyTerritoryOfPlayer(PlayerBase player)
	{
		return ExorTerritoryProbe.EnemyMastOf(player);
	}

	// Etiqueta legible del territorio para el log: "grupo <id> (lider <nombre>)".
	static string GroupLabel(TerritoryFlag mast)
	{
		if (!mast)
			return "?";
		string gid = mast.ExorGetGroupId();
		ExorGroup g = ExorGroupManager.Get().FindById(gid);
		if (!g)
			return gid;
		string ownerName = "?";
		ExorGroupMember owner = g.FindMember(g.owner_id);
		if (owner)
			ownerName = owner.name;
		return string.Format("grupo %1 (lider %2)", gid, ownerName);
	}

	// ------------------------- #2: desmantelar -------------------------
	// true => hay que BLOQUEAR el desmantelado de 'obj' por 'player'.
	// Solo si: feature on + territorio on + 'obj' es parte de base/torre + cae
	// dentro del radio de un mastil AJENO. Lejos de todo mastil -> false (libre).
	// exigirBaseVanilla=true: solo bloquea si 'obj' hereda de BaseBuildingBase
	// (partes de base VANILLA). Para muros de BBP se pasa false: BBP NO hereda de
	// BaseBuildingBase y su accion (ActionDismantleBBP) ya apunta solo a partes BBP.
	static bool BloqueaDesmantelarAjeno(PlayerBase player, Object obj, bool exigirBaseVanilla = true)
	{
		if (!GetGame() || !GetGame().IsServer())
			return false;
		if (!obj)
			return false;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.bloquear_desmantelar_ajeno)
			return false;
		if (!GetExorConfig().party.territorio.habilitado)
			return false;
		if (exigirBaseVanilla && !obj.IsInherited(BaseBuildingBase))
			return false;	// solo muros / portones / torres / partes de base
		return ExorTerritoryManager.Get().FindEnemyTerritoryAt(player, obj.GetPosition()) != null;
	}

	// ------------------------- #3: robo de items -------------------------
	// Loguea cuando un ajeno toma un item estando dentro de territorio enemigo.
	static void OnPickupInEnemyTerritory(PlayerBase player, ItemBase item)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!player || !item)
			return;
		// GATE BARATO PRIMERO: dos bools estaticos ya leidos de la config. Esto corre por
		// CADA item que entra a un inventario, asi que lo primero tiene que ser lo mas
		// barato posible; antes eran 4 llamadas + derefs a la config por item.
		if (!ExorHotFlags.LogRoboContenedor())
			return;
		if (!ExorHotFlags.TerritorioOn())
			return;

		TerritoryFlag m = EnemyTerritoryOfPlayer(player);
		if (!m)
			return;	// no esta dentro de territorio ajeno: no se loguea

		string sid = ExorGroupManager.SteamId(player);
		string nm = ExorGroupManager.PlayerName(player);
		// AGRUPADO: no se escribe una linea por item (vaciar una mochila eran ~20 escrituras
		// sincronas al audit en el mismo segundo). El buffer junta la sesion de saqueo y
		// escribe UNA linea con el total y el desglose. Ver ExorRoboBuffer.c
		ExorRoboBuffer.Add(sid, nm, GroupLabel(m), player.GetPosition(), item.GetType());
	}

	// ------------------------- #5: abrir barril 3xor ajeno -------------------------
	// Loguea cuando un ajeno ABRE un barril 3xor que esta dentro de territorio
	// enemigo. Se gatea por la posicion del BARRIL (lo que se esta raideando), no
	// del jugador. Lo dispara modded ActionOpenBarrel.OnExecuteServer.
	static void OnOpenBarrelInEnemyTerritory(PlayerBase player, EntityAI barrel)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!player || !barrel)
			return;
		// GATE BARATO PRIMERO: bandera ya leida de la config. Corre por CADA apertura de
		// barril; con la feature apagada el costo es un bool y nada mas.
		if (!ExorHotFlags.LogAbrirBarril())
			return;
		if (!ExorHotFlags.TerritorioOn())
			return;

		TerritoryFlag m = ExorTerritoryManager.Get().FindEnemyTerritoryAt(player, barrel.GetPosition());
		if (!m)
			return;	// el barril no esta en territorio ajeno: no se loguea

		string sid = ExorGroupManager.SteamId(player);
		string nm = ExorGroupManager.PlayerName(player);
		string detalle = string.Format("abrio un barril dentro del territorio de %1", GroupLabel(m));
		ExorRaidLog.Write("ABRE_BARRIL", sid, nm, barrel.GetPosition(), detalle);
	}

	// ------------------------- #4a: deslogueo en base ajena -------------------------
	static void OnDisconnectInEnemyTerritory(PlayerBase player, PlayerIdentity identity)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!player || !identity)
			return;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.log_desconexion_base_ajena)
			return;
		if (!GetExorConfig().party.territorio.habilitado)
			return;

		TerritoryFlag m = EnemyTerritoryOfPlayer(player);
		if (!m)
			return;

		string detalle = string.Format("se deslogueo dentro de territorio de %1", GroupLabel(m));
		ExorRaidLog.Write("LOGOUT_BASE_AJENA", identity.GetPlainId(), identity.GetName(), player.GetPosition(), detalle);
	}

	// ------------------------- #6: combat-log (deslogueo en zona de combate) -------------------------
	// Si el jugador se desloguea estando DENTRO de una zona de combate PvP viva (ver
	// ExorCombatZones, alimentado por PlayerBase.EEHitBy), se deja rastro. Es por
	// PRESENCIA: no importa si le dispararon o no, solo estar en el area del tiroteo.
	static void OnCombatLogout(PlayerBase player, PlayerIdentity identity)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!player || !identity)
			return;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.log_combat_log || p.combat_log_minutos <= 0)
			return;
		if (!player.IsAlive())
			return;	// murio: no es deslogueo de combate

		int elapsed;
		if (!ExorCombatZones.IsInActiveZone(player.GetPosition(), elapsed))
			return;	// no esta dentro de ninguna zona de combate viva

		int radioM = Math.Round(p.combat_log_radio);
		string detalle = string.Format("se deslogueo dentro de zona de combate PvP (radio %1m) | combate activo hace %2 min (ventana %3)",
			radioM, elapsed, p.combat_log_minutos);
		ExorRaidLog.Write("COMBAT_LOG", identity.GetPlainId(), identity.GetName(), player.GetPosition(), detalle);
	}

	// ------------------------- #4b: sacar al reconectar -------------------------
	// Se llama al estar listo el cliente; difiere el chequeo unos ms para que la
	// posicion ya este asentada tras la carga.
	static void KickFromEnemyBaseIfNeeded(PlayerBase player)
	{
		if (!GetGame() || !GetGame().IsServer() || !player)
			return;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.sacar_de_base_ajena_al_reconectar)
			return;
		if (!GetExorConfig().party.territorio.habilitado)
			return;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAntiRaid.DoKickFromEnemyBase, 2000, false, player);
	}

	static void DoKickFromEnemyBase(PlayerBase player)
	{
		if (!player)
			return;
		TerritoryFlag m = EnemyTerritoryOfPlayer(player);
		if (!m)
			return;	// reaparecio fuera de territorio ajeno: nada que hacer

		vector mp = m.GetPosition();
		vector pp = player.GetPosition();
		float radius = ExorTerritoryRules.Radius();

		// direccion mastil -> jugador, en el plano horizontal (Y=0)
		vector dir = Vector(pp[0] - mp[0], 0, pp[2] - mp[2]);
		float len = dir.Length();
		if (len < 0.01)
			dir = Vector(1, 0, 0);	// estaba justo sobre el mastil: empuje arbitrario
		else
			dir = dir * (1.0 / len);

		float push = radius + 5.0;	// 5m de margen fuera del borde
		vector flat = mp + (dir * push);
		float gy = GetGame().SurfaceY(flat[0], flat[2]);
		vector dest = Vector(flat[0], gy, flat[2]);
		player.SetPosition(dest);

		string sid = ExorGroupManager.SteamId(player);
		string nm = ExorGroupManager.PlayerName(player);
		int pushM = Math.Round(push);
		string detalle = string.Format("reconecto dentro de territorio de %1 -> movido al borde (%2m del mastil)", GroupLabel(m), pushM);
		ExorRaidLog.Write("EXPULSADO_BASE_AJENA", sid, nm, pp, detalle);
		player.MessageImportant("Apareciste dentro de un territorio ajeno: fuiste movido afuera.");
	}
}

// ===========================================================================
// #2 - Bloqueo de desmantelar en territorio ajeno.
// ActionCondition corre en cliente y server; el helper solo bloquea en server
// (autoritativo). En cliente la accion puede aparecer, pero el server la rechaza.
// NOTA: el mod @NoWallDamage tambien moddea ActionDismantlePart (bloqueo por
// horario de raid). Son PBOs distintos y encadenan por super: conviven OK.
// ===========================================================================
modded class ActionDismantlePart
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		bool ok = super.ActionCondition(player, target, item);
		if (!ok)
			return false;
		if (target && ExorAntiRaid.BloqueaDesmantelarAjeno(player, target.GetObject()))
			return false;
		return ok;
	}
}

// ===========================================================================
// Cobertura BaseBuildingPlus (BBP): BBP usa su PROPIA accion ActionDismantleBBP
// (NO la vanilla ActionDismantlePart) y sus muros NO heredan de BaseBuildingBase.
// Se hookea igual, gateado por territorio ajeno (exigirBaseVanilla=false).
// Compila SOLO si BBP esta cargado (BaseBuildingPlus hace #define BBP): en un
// server vanilla este bloque NO existe y el mod carga limpio. Requiere que
// @3xor cargue DESPUES de BBP para que el #define este activo al compilar.
// ===========================================================================
#ifdef BBP
modded class ActionDismantleBBP
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		bool ok = super.ActionCondition(player, target, item);
		if (!ok)
			return false;
		if (target && ExorAntiRaid.BloqueaDesmantelarAjeno(player, target.GetObject(), false))
			return false;
		return ok;
	}
}
#endif
