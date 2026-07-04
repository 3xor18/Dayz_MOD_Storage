// ============================================================================
// 3xor_Vanilla_Optimization - Zonas de NO construccion (nobuild.json)  [SOLO server]
// El admin define centros (x,y,z del mundo) con un radio en metros; dentro de ese
// radio NADIE puede construir/colocar nada, SALVO los classnames de la whiteList
// (minas/trampas/explosivos/fuegos artificiales) para no romper el PvP.
//
// Enforce SERVER-SIDE (no depende de que el cliente conozca las zonas):
//   1) ItemBase.CanBePlaced (rama server)  -> corta el deploy antes de completarse.
//      (integrado en ExorTerritory_Items.c, que ya modea CanBePlaced)
//   2) ItemBase.OnPlacementComplete (backstop, en ExorTerritory_Items.c) -> si algo se
//      coloco igual, se borra. Cubre TODO kit/deployable (fence/watchtower/tienda/barril/
//      fogata) y los kits de BuildEverywhere y BaseBuildingPlus (todos son ItemBase).
//   3) ActionBuildPart -> bloquea AGREGAR partes a una base vanilla ya existente que
//      quede dentro de una zona (cubre tambien BuildEverywhere, que usa partes vanilla).
// ============================================================================
class ExorNoBuild
{
	// true si se PUEDE construir/colocar 'itemType' en 'pos'. false = zona prohibida.
	static bool CanBuildAt(vector pos, string itemType)
	{
		ExorCfgNoBuild cfg = GetExorConfig().nobuild;
		if (!cfg || !cfg.activado)
			return true;
		if (!cfg.lugares_no_permitidos || cfg.lugares_no_permitidos.Count() == 0)
			return true;
		if (IsWhitelisted(cfg, itemType))
			return true;

		int i;
		for (i = 0; i < cfg.lugares_no_permitidos.Count(); i++)
		{
			ExorCfgNoBuildZona z = cfg.lugares_no_permitidos.Get(i);
			if (!z || !z.posicion || z.desabilitar_construccion_en_metros <= 0)
				continue;
			// distancia HORIZONTAL (x,z del mundo); 'y' = altura, se ignora
			float dx = pos[0] - z.posicion.x;
			float dz = pos[2] - z.posicion.z;
			float dist = Math.Sqrt(dx * dx + dz * dz);
			if (dist < z.desabilitar_construccion_en_metros)
				return false;
		}
		return true;
	}

	// whitelist por 'contains' case-insensitive: la entrada de config puede ser el
	// classname exacto o un pedazo (ej. "FireworksLauncher" pega tambien en
	// "Anniversary_FireworksLauncher").
	static bool IsWhitelisted(ExorCfgNoBuild cfg, string itemType)
	{
		if (!cfg.whiteList || itemType == "")
			return false;
		string t = itemType;
		t.ToLower();
		int i;
		for (i = 0; i < cfg.whiteList.Count(); i++)
		{
			string w = cfg.whiteList.Get(i);
			if (w == "")
				continue;
			w.ToLower();
			if (t.Contains(w))
				return true;
		}
		return false;
	}

	// aviso al jugador (server). Reusa el canal de mensaje importante de vanilla.
	static void Warn(Man player)
	{
		PlayerBase pb = PlayerBase.Cast(player);
		if (pb)
			pb.MessageImportant("No se puede construir en esta zona (zona protegida).");
	}

	// true si hay que BLOQUEAR este kit de base en 'pos'. Loguea + avisa. Lo usan los
	// overrides de los kits (FenceKit/WatchtowerKit/TerritoryFlagKit): un kit de base NO
	// pasa por el borrado de ItemBase.OnPlacementComplete (crea su objeto con CreateObjectEx
	// y despues se borra), asi que hay que cortar ANTES de que llame a super.
	static bool BlockKit(Man player, vector pos, string type)
	{
		if (!GetGame() || !GetGame().IsServer())
			return false;
		if (CanBuildAt(pos, type))
			return false;
		Print(string.Format("%1 NoBuild: kit '%2' BLOQUEADO en zona (%3)", ExorStorageConstants.LOG, type, pos.ToString()));
		Warn(player);
		return true;
	}
}

// ---------------------------------------------------------------------------
// Kits de base: cortar la construccion ANTES de que el kit cree su objeto (Fence/
// Watchtower/TerritoryFlag). Si esta en zona, NO se llama a super -> el objeto nunca
// se crea y el kit se queda en las manos (no se pierde). Solo afecta colocaciones
// NUEVAS (OnPlacementComplete NO se llama al cargar bases de persistencia -> las bases
// existentes NUNCA se tocan).
// ---------------------------------------------------------------------------
modded class FenceKit
{
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		if (ExorNoBuild.BlockKit(player, position, GetType()))
			return;
		super.OnPlacementComplete(player, position, orientation);
	}
}

modded class WatchtowerKit
{
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		if (ExorNoBuild.BlockKit(player, position, GetType()))
			return;
		super.OnPlacementComplete(player, position, orientation);
	}
}

modded class TerritoryFlagKit
{
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		if (ExorNoBuild.BlockKit(player, position, GetType()))
			return;
		super.OnPlacementComplete(player, position, orientation);
	}
}

// ---------------------------------------------------------------------------
// Cobertura BaseBuildingPlus (BBP): TODOS los kits/holograms de BBP (muros, pisos,
// techos, puertas, foundations, etc.) heredan de BBP_KIT_BASE. Como el FenceKit vanilla,
// el KIT spawnea la pieza (BBP_BASE) con CreateObjectEx en su OnPlacementComplete, asi
// que cortamos ANTES de super -> la pieza nunca se crea y el kit queda en las manos.
// Un solo hook en la base cubre TODAS las piezas BBP. Solo afecta colocaciones nuevas
// (OnPlacementComplete NO corre al cargar persistencia -> bases BBP existentes intactas).
// Compila SOLO con BBP cargado (#define BBP). En server vanilla este bloque NO existe y
// el mod carga limpio. Requiere @3xor DESPUES de BBP para que el #define este activo.
// ---------------------------------------------------------------------------
#ifdef BBP
modded class BBP_KIT_BASE
{
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		if (ExorNoBuild.BlockKit(player, position, GetType()))
			return;
		super.OnPlacementComplete(player, position, orientation);
	}
}
#endif

// ---------------------------------------------------------------------------
// Bloquear AGREGAR partes a una base vanilla ya existente dentro de una zona.
// (BuildEverywhere usa la accion vanilla, asi que tambien queda cubierto.)
// ---------------------------------------------------------------------------
modded class ActionBuildPart
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!super.ActionCondition(player, target, item))
			return false;
		if (GetGame() && GetGame().IsServer() && target)
		{
			Object o = target.GetObject();
			if (o && !ExorNoBuild.CanBuildAt(o.GetPosition(), o.GetType()))
				return false;
		}
		return true;
	}
}
