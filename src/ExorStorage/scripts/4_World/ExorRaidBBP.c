// ============================================================================
//  3xor_Vanilla_Optimization - RAID: parte especifica de BaseBuildingPlus
// ============================================================================
// La clase EXISTE SIEMPRE (para que ExorRaidModule compile en un server sin BBP);
// lo unico condicional es el cuerpo de los metodos que tocan clases de BBP.
// Requiere que @3xor cargue DESPUES de BBP para que el #define este activo, igual
// que los hooks de ExorAntiRaid y ExorNoBuild. Ver el comentario del lanzador.
// ============================================================================
class ExorRaidBBP
{
	// Tier REAL de la pieza (1/2/3). En BBP el tier NO es una clase: se calcula en
	// runtime segun con que material esta construida (BBPGetTier mira
	// IsPartConstructed("t3_door") y compania). La MISMA clase BBP_BDoor puede ser
	// tier 1, 2 o 3, asi que no se puede deducir del classname.
	static int TierDe(Object obj)
	{
		#ifdef BBP
		BBP_BASE b = BBP_BASE.Cast(obj);
		if (b)
		{
			int t = b.BBPGetTier();
			if (t >= 1 && t <= 3)
				return t;
		}
		#endif
		return 1;
	}

	// ⭐ Vuela SOLO LA HOJA DE LA PUERTA, no la pieza entera.
	//
	// Borrar el objeto completo (ObjectDelete) parecia lo natural pero deja al raidero
	// peor que antes: en una escotilla de piso/techo se va TODA la estructura -escalera
	// incluida- y no puede subir. En una pared con puerta se iria el panel entero.
	//
	// BBP resuelve esto con el sistema de Construction: su propio raid destruye la PARTE
	// "t1_door"/"t2_door"/"t3_door" y deja el marco en pie, que es justo el hueco por el
	// que se entra. Se copia ese criterio. Solo si la pieza NO tiene puerta se borra el
	// objeto entero (no deberia pasar: la config solo lista puertas y portones).
	//
	// DestroyPartServer acepta player = null (vanilla mismo lo llama asi en las partes
	// conectadas), asi que no hace falta saber quien la volo.
	static void DestruirPuerta(Object obj, bool piezaCompleta)
	{
		if (!obj)
			return;
		#ifdef BBP
		BBP_BASE b = BBP_BASE.Cast(obj);
		if (b)
		{
			// El candado se va con la puerta que cerraba. GetCodeLock() NO es de BBP: lo
			// agrega el mod CodeLock (BBP lo llama tambien bajo #ifdef CodeLock). Sin ese
			// guard, un server con BBP pero SIN CodeLock no compila el modulo World y no
			// arranca ("Undefined function 'BBP_BASE.GetCodeLock'").
			#ifdef CodeLock
			if (GetExorConfig().raid && GetExorConfig().raid.borrar_candado_al_destruir && b.BBP_HasLock())
			{
				EntityAI lock_ = EntityAI.Cast(b.GetCodeLock());
				if (lock_)
					GetGame().ObjectDelete(lock_);
			}
			#endif

			if (!piezaCompleta)
			{
				// SIN puerta que volar -> NO se toca. Antes caia al ObjectDelete de abajo y
				// se llevaba el MURO entero: pasaba al raidear una 2da puerta pegada, porque
				// el marco ya vaciado quedaba mas cerca del estallido y lo elegia a el.
				Construction c = b.GetConstruction();
				if (!c || !b.BBP_HasDoor())
					return;
				// de mayor a menor: la pieza tiene UNA sola hoja construida
				if (c.IsPartConstructed("t3_door"))
					c.DestroyPartServer(null, "t3_door", AT_DESTROY_PART);
				else if (c.IsPartConstructed("t2_door"))
					c.DestroyPartServer(null, "t2_door", AT_DESTROY_PART);
				else if (c.IsPartConstructed("t1_door"))
					c.DestroyPartServer(null, "t1_door", AT_DESTROY_PART);
				return;	// el marco QUEDA: por ahi se entra
			}
		}
		#endif
		GetGame().ObjectDelete(obj);
	}

	// Esta pieza sigue siendo un objetivo valido? Un marco al que YA le volaron la puerta
	// no lo es: si contara, absorberia los explosivos destinados a la puerta de al lado.
	static bool EsObjetivoValido(Object obj, ExorCfgRaidEstructura e)
	{
		if (!obj || !e)
			return false;
		if (e.destruir_pieza_completa != 0)
			return true;	// portones: se vuelan enteros, no dependen de tener hoja
		#ifdef BBP
		BBP_BASE b = BBP_BASE.Cast(obj);
		if (b && !b.BBP_HasDoor())
			return false;	// marco vacio: ya fue raideado
		#endif
		return true;
	}

	// Jugador vivo mas cercano a 'pos' dentro de 'radio'. Se usa SOLO para atribuir el
	// raid en el log: cuando una granada estalla ya no tiene dueno, asi que esto es un
	// INDICIO, no una prueba. Usa el cache de jugadores del tick (no hace GetPlayers).
	static PlayerBase JugadorMasCercano(vector pos, float radio)
	{
		array<Man> players = ExorTick1Hz.Jugadores();
		if (!players)
			return null;
		float mejor = radio * radio;
		PlayerBase elegido = null;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (!p || !p.IsAlive())
				continue;
			float d2 = ExorMath.Dist2DSq(pos, p.GetPosition());
			if (d2 <= mejor)
			{
				mejor = d2;
				elegido = p;
			}
		}
		return elegido;
	}

	// Busca la pieza RAIDEABLE mas cercana al estallido. Solo la mas cercana cuenta:
	// asi una granada no le baja el contador a media base de una.
	static Object PiezaMasCercana(vector pos)
	{
		ExorCfgRaid r = GetExorConfig().raid;
		if (!r)
			return null;
		array<Object> objs = new array<Object>;
		array<CargoBase> cargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D(pos, r.radio_deteccion_metros, objs, cargos);

		Object mejor = null;
		float bestD = 999999;
		int i;
		for (i = 0; i < objs.Count(); i++)
		{
			Object o = objs.Get(i);
			if (!o)
				continue;
			ExorCfgRaidEstructura e = r.BuscarEstructura(o.GetType());
			if (!e)
				continue;	// no es una clase raideable
			if (!EsObjetivoValido(o, e))
				continue;	// marco ya vaciado: que no le robe el impacto a la puerta de al lado
			float d = vector.Distance(pos, o.GetPosition());
			if (d < bestD)
			{
				bestD = d;
				mejor = o;
			}
		}
		return mejor;
	}
}

// ----------------------------------------------------------------------------
//  EXPLOSIVOS
// ----------------------------------------------------------------------------
// Una granada LANZADA se borra del mundo justo al estallar, sin dueno en la
// jerarquia. Aprovechamos ese borrado para detectar la explosion, aunque el blast
// haya quedado tapado por una estructura. Las granadas EN INVENTARIO (con dueno)
// no cuentan. Mismo mecanismo que usa NoWallDamage para las puertas vanilla.
modded class Grenade_Base
{
	override void EEDelete(EntityAI parent)
	{
		if (GetGame() && GetGame().IsServer() && !GetHierarchyParent())
			ExorRaidExplosivo.Detonacion(GetPosition(), GetType(), this);
		super.EEDelete(parent);
	}
}

// Claymore, plastico e IED NO son Grenade_Base: heredan de ExplosivesBase, que al
// estallar llama OnExplode() y recien despues se auto-borra a los 0,25 s. Se engancha
// ahi y no en el borrado, para no confundir "lo tiraron al piso y despawneo" con
// "exploto".
modded class ExplosivesBase
{
	override void OnExplode()
	{
		if (GetGame() && GetGame().IsServer())
			ExorRaidExplosivo.Detonacion(GetPosition(), GetType(), this);
		super.OnExplode();
	}
}

// La mina de piso es otra rama mas: LandMineTrap hereda de TrapBase, no de
// ExplosivesBase, y detona por Explode().
modded class LandMineTrap
{
	override void Explode(int damageType, string ammoType = "")
	{
		if (GetGame() && GetGame().IsServer())
			ExorRaidExplosivo.Detonacion(GetPosition(), GetType(), this);
		super.Explode(damageType, ammoType);
	}
}

class ExorRaidExplosivo
{
	static void Detonacion(vector pos, string clase, EntityAI fuente)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorCfgRaid r = GetExorConfig().raid;
		if (!r || !r.activado)
			return;
		if (!ExorMuebleRules.IsLootFreeNow())
			return;	// fuera de la ventana no se cuenta nada
		Object pieza = ExorRaidBBP.PiezaMasCercana(pos);
		if (!pieza)
			return;
		ExorRaidModule.Impacto(pieza, clase, false, pos, fuente);
	}
}

// ----------------------------------------------------------------------------
//  BALAS
// ----------------------------------------------------------------------------
// ⚠️ SIN VERIFICAR TODAVIA. Las piezas BBP no tienen DamageSystem, asi que puede
// que el motor NUNCA dispare EEHitBy sobre ellas y este hook no corra nunca. Es lo
// PRIMERO a comprobar in-game (poner log_cada_impacto en 1 y tirarle unos tiros a
// una puerta dentro del horario). Si no llega, las balas no se pueden contar por
// esta via y hay que sacarlas de la config o buscar otro camino.
#ifdef BBP
modded class BBP_BASE
{
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
		int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (damageType != DamageType.FIRE_ARM)
			return;	// solo balas: los explosivos ya se cuentan por su propio hook
		ExorRaidModule.Impacto(this, "Bala", true, GetPosition(), null);
	}
}
#endif
