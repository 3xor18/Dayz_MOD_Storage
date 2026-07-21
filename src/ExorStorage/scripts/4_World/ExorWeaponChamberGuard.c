// ============================================================================
// 3xor_Vanilla_Optimization - GUARD de chambering (anti-crash de armas rotas)
// ============================================================================
// PROBLEMA (produccion 20-jul 23:59): un arma que entra a cargar cartucho SIN
// magazine (m_srcMagazine=NULL) revienta el FSM VANILLA:
//   - AcquireCartridgeFromMagazine() desreferencia m_srcMagazine sin chequear
//     null -> "NULL pointer to instance" (excepcion dura de script).
//   - ShowBullet() loguea un Error por frame -> spam.
// Se vio con un crossbow de mod, pero el fix es GENERICO (no menciona ese mod):
// SOLO actua cuando m_srcMagazine es NULL. Para un arma normal ese valor nunca es
// null aca (el FSM lo setea en OnEntry) -> el arma y el personaje NO cambian.
// Ademas LOGUEA (throttled 60s) que arma lo disparo, para cazar futuras armas rotas.
// ============================================================================
class ExorWpnGuard
{
	static int s_LastLogMs;

	static void LogNull(Object wpn, string origen)
	{
		int now = GetGame().GetTime();
		if (s_LastLogMs != 0 && now - s_LastLogMs < 60000)
			return;
		s_LastLogMs = now;
		string dbg = "arma-desconocida";
		if (wpn)
			dbg = Object.GetDebugName(wpn);
		Print("[3xorWpnGuard] Evitado crash de chambering sin magazine en " + origen + " arma " + dbg);
	}
}

// ShowBullet/AcquireCartridgeFromMagazine viven en WeaponChambering_Base; TODAS las
// subclases (WeaponChambering_Cartridge, _Preparation, _MultiMuzzle, etc.) las heredan
// sin override -> moddear la base cubre todos los casos de chambering.
modded class WeaponChambering_Base
{
	override bool AcquireCartridgeFromMagazine()
	{
		if (!m_srcMagazine)
		{
			ExorWpnGuard.LogNull(m_weapon, "AcquireCartridgeFromMagazine");
			return false;
		}
		return super.AcquireCartridgeFromMagazine();
	}

	override bool ShowBullet(int muzzleIndex)
	{
		if (!m_srcMagazine)
		{
			ExorWpnGuard.LogNull(m_weapon, "ShowBullet");
			return false;
		}
		return super.ShowBullet(muzzleIndex);
	}
}
