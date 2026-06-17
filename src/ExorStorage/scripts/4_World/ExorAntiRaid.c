// ============================================================================
// 3xor_Vanilla_Optimization - Anti-raid (SOLO server). Todo gateado por el
// radio del mastil: SOLO aplica dentro de territorio AJENO (un mastil que no es
// de tu party). Fuera del radio de cualquier mastil, nada de esto interviene.
//   #2 No desmantelar muros/torres en territorio ajeno  -> ActionDismantlePart
//   #3 Log de robo (tomar items dentro de territorio ajeno) -> ItemBase.OnInventoryEnter
//   #4 Log de deslogueo + sacar al reconectar dentro de territorio ajeno -> Mission
// El log forense lo escribe ExorRaidLog (1 archivo por dia, auto-purga 7 dias).
// ============================================================================
class ExorAntiRaid
{
	// ------------------------- helpers de territorio -------------------------

	// El mastil AJENO en cuyo radio cae 'player' (o null si esta fuera de todo
	// territorio ajeno / parado en territorio propio).
	static TerritoryFlag EnemyTerritoryOfPlayer(PlayerBase player)
	{
		if (!player)
			return null;
		return ExorTerritoryManager.Get().FindEnemyTerritoryAt(player, player.GetPosition());
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
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.log_robo_contenedor)
			return;
		if (!GetExorConfig().party.territorio.habilitado)
			return;

		TerritoryFlag m = EnemyTerritoryOfPlayer(player);
		if (!m)
			return;	// no esta dentro de territorio ajeno: no se loguea

		string sid = ExorGroupManager.SteamId(player);
		string nm = ExorGroupManager.PlayerName(player);
		string detalle = string.Format("tomo '%1' dentro de territorio de %2", item.GetType(), GroupLabel(m));
		ExorRaidLog.Write("ROBO_ITEM", sid, nm, player.GetPosition(), detalle);
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
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.log_abrir_barril_ajeno)
			return;
		if (!GetExorConfig().party.territorio.habilitado)
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
