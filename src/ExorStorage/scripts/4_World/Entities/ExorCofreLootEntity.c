// ============================================================================
//  3xor_Vanilla_Optimization - COFRE DE LOOT (entidad)
// ----------------------------------------------------------------------------
//  El baul marino vanilla retexturizado a camuflado. Va FIJO en el piso: no se
//  agarra con las manos, no se guarda en una mochila y no entra al baul de un
//  auto. Se abre reventandolo a golpes de melee o a tiros (cuantos, en
//  cofres_loot.json), y el loot se crea RECIEN en ese momento.
//
//  El estado abierto/cerrado va SINCRONIZADO al cliente porque de el depende que
//  el cargo se pueda ver: mientras esta cerrado, CanDisplayCargo() devuelve false
//  y el jugador no ve -ni toca- lo que hay adentro.
//
//  COSTO POR FRAME: cero. La entidad no tiene tick propio; solo reacciona a
//  EEHitBy (que corre unicamente cuando alguien le pega). El ciclo de vida
//  -aparecer, despawnear, reaparecer- lo lleva el manager ExorCofreLoot con un
//  latido lento de 30 s.
// ============================================================================
class Exor_CofreLoot : Container_Base
{
	// sincronizado: de esto depende que se vea (y se pueda tocar) el cargo
	bool m_ExorAbierto;

	// ---- solo server ----
	float m_ExorProgreso;		// 0..1 ; a 1 se abre. Fraccionario: mezclar balas y golpes suma
	int   m_ExorUltimoHitMs;	// dedup de impactos del MISMO disparo (perdigones de escopeta)
	int   m_ExorUltimoAvisoPct;	// ultimo porcentaje avisado al que le pega (para no spamear)
	int   m_ExorPunto;			// indice de la posicion de cofres_loot.json (-1 = suelto/admin)
	int   m_ExorBorrarEnMs;		// uptime ms en que se borra si nadie lo vacio (0 = sin deadline)
	string m_ExorTipo;			// nombre de la tabla de loot que le toco al spawnear

	void Exor_CofreLoot()
	{
		RegisterNetSyncVariableBool("m_ExorAbierto");
		m_ExorPunto = -1;
		m_ExorTipo = "";
	}

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			// El motor NO tiene que poder romperlo ni arruinarlo: la "vida" del cofre es
			// nuestro contador de progreso, no sus hitpoints. Con hitpoints reales, 20 tiros
			// lo dejarian arruinado (y el cargo inutilizable) antes de que se abra.
			SetHealth("", "", GetMaxHealth("", "Health"));
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame() && GetGame().IsServer())
			ExorCofreLoot.Get().OnCofreBorrado(this);
		super.EEDelete(parent);
	}

	// ------------------------------------------------------------------------
	//  NO SE AGARRA NI SE MUEVE
	// ------------------------------------------------------------------------
	override bool IsTakeable()
	{
		return false;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	override bool CanPutInCargo(EntityAI parent)
	{
		return false;
	}

	// ------------------------------------------------------------------------
	//  CERRADO = NO SE VE NI SE TOCA EL CONTENIDO
	// ------------------------------------------------------------------------
	override bool CanDisplayCargo()
	{
		return m_ExorAbierto;
	}

	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (!m_ExorAbierto)
			return false;
		return super.CanReceiveItemIntoCargo(item);
	}

	// OJO: el parametro se tiene que llamar 'cargo' como en el prototipo de EntityAI.
	// Con otro nombre el compilador tira 'Can't find variable' y NO compila el modulo World.
	override bool CanReleaseCargo(EntityAI cargo)
	{
		if (!m_ExorAbierto)
			return false;
		return super.CanReleaseCargo(cargo);
	}

	// ------------------------------------------------------------------------
	//  GOLPES Y TIROS
	// ------------------------------------------------------------------------
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
		int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		if (!GetGame() || !GetGame().IsServer())
			return;
		// que no se arruine por el camino: el daño real no decide nada aca
		SetHealth("", "", GetMaxHealth("", "Health"));
		ExorCofreLoot.Get().Impacto(this, damageType, source);
	}

	// El cofre NO persiste a proposito: lo crea y lo borra el manager en cada arranque
	// (ver ExorCofreLoot.LimpiarHuerfanos). Guardar su estado solo serviria para
	// resucitar cofres a medio abrir y para llenar la persistencia de entidades que el
	// modulo iba a recrear igual.
	void ExorAbrirVisual()
	{
		m_ExorAbierto = true;
		SetSynchDirty();
	}
}
