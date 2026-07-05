// ============================================================================
// 3xor_Vanilla_Optimization - COFRE (items del modulo de cofres de recompensa)
// ----------------------------------------------------------------------------
//  - Exor_Cofre_*_Packed (ItemBase): CAJA CERRADA carriable (item normal del mod).
//    Al soltarse en el piso de una zona ABIERTA, tras N min se transforma en el
//    cofre abierto (lo hace el manager ExorCofre).
//  - Exor_Cofre_* (Barrel_ColorBase): COFRE ABIERTO de 1000 slots.
//  Ambos se REGISTRAN con el manager y llevan un DEADLINE de despawn (minuto de
//  reloj real, persistido): al cerrar el evento el manager les pone deadline = ahora
//  + minutos_despawn_cofres_tras_evento y luego los borra. Sobrevive reinicios.
//  - Exor_CofreLight: luz (Chemlight) NO agarrable. El manager pone un circulo de estas
//    alrededor de la mesa (luz en el piso, sin humo) y las despawnea al cerrar la ventana.
// ============================================================================

// ----------------------------------------------------------------------------
// CAJA CERRADA (item carriable). Al soltarse en zona activa -> se abre.
// ----------------------------------------------------------------------------
class Exor_Cofre_Packed_Base : ItemBase
{
	// ms de uptime en que empezo a contar en una zona activa (0 = no cuenta). Runtime.
	int m_ExorDeployStartMs;
	// minuto de reloj REAL en que hay que despawnearla (0 = sin deadline). Persiste.
	int m_ExorDespawnAtMin;
	// slot de la mesa donde esta colocada (-1 = ninguno). Runtime.
	int m_ExorSlotIdx;
	int m_ExorSlotZone;
	// sincronizado al cliente: la caja esta seteada en la mesa (no se puede tomar)
	bool m_ExorOnTable;

	void Exor_Cofre_Packed_Base()
	{
		RegisterNetSyncVariableBool("m_ExorOnTable");
	}

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);	// indestructible (consistente con el resto del mod)
			m_ExorDeployStartMs = 0;
			m_ExorSlotIdx = -1;
			m_ExorSlotZone = -1;
			ExorCofre.Get().RegisterBox(this);
		}
	}

	// marca/desmarca la caja como "en la mesa" (sincroniza al cliente para bloquear tomarla)
	void ExorSetOnTable(bool v)
	{
		m_ExorOnTable = v;
		SetSynchDirty();
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			ExorCofre.Get().UnregisterBox(this);
		super.EEDelete(parent);
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorDespawnAtMin);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		if (!ctx.Read(m_ExorDespawnAtMin))
			m_ExorDespawnAtMin = 0;
		return true;
	}

	// una vez seteada en la mesa, NO se puede tomar (evita bugs). Solo se lootea al abrirse.
	override bool CanPutIntoHands(EntityAI parent)
	{
		if (m_ExorOnTable)
			return false;
		return super.CanPutIntoHands(parent);
	}

	override bool IsTakeable()
	{
		if (m_ExorOnTable)
			return false;
		return super.IsTakeable();
	}

	string ExorGetColor()    { return ""; }
	string ExorGetOpenType() { return ""; }
}

class Exor_Cofre_Azul_Packed : Exor_Cofre_Packed_Base
{
	override string ExorGetColor() { return "azul"; }
	override string ExorGetOpenType() { return "Exor_Cofre_Azul"; }
}

class Exor_Cofre_Verde_Packed : Exor_Cofre_Packed_Base
{
	override string ExorGetColor() { return "verde"; }
	override string ExorGetOpenType() { return "Exor_Cofre_Verde"; }
}

class Exor_Cofre_Rojo_Packed : Exor_Cofre_Packed_Base
{
	override string ExorGetColor() { return "rojo"; }
	override string ExorGetOpenType() { return "Exor_Cofre_Rojo"; }
}

// ----------------------------------------------------------------------------
// COFRE ABIERTO (contenedor de 1000 slots). No se toma en mano; despawnea 2h
// despues del evento (deadline puesto por el manager).
// ----------------------------------------------------------------------------
class Exor_Cofre_Base : Barrel_ColorBase
{
	int m_ExorDespawnAtMin;	// minuto de reloj REAL para despawnear (0 = sin deadline). Persiste.
	int m_ExorSlotIdx;		// slot de la mesa (-1 = ninguno). Runtime.
	int m_ExorSlotZone;

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);	// el loot nunca se pierde por dano
			m_ExorSlotIdx = -1;
			m_ExorSlotZone = -1;
			ExorCofre.Get().RegisterChest(this);
		}
	}

	// cantidad de items en el cargo (para saber si tiene loot)
	int ExorCargoCount()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return 0;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return 0;
		return cargo.GetItemCount();
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			ExorCofre.Get().UnregisterChest(this);
		super.EEDelete(parent);
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorDespawnAtMin);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		if (!ctx.Read(m_ExorDespawnAtMin))
			m_ExorDespawnAtMin = 0;
		return true;
	}

	// Fijo en el suelo: no se puede tomar en la mano ni mover.
	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	// No lockeable: bloquea CodeLock / candados de cualquier mod.
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (attachment)
		{
			string type = attachment.GetType();
			type.ToLower();
			if (type.Contains("codelock") || type.Contains("combinationlock") || type.Contains("padlock") || type.Contains("lock"))
				return false;
		}
		return super.CanReceiveAttachment(attachment, slotId);
	}

	string ExorGetColor() { return ""; }
}

class Exor_Cofre_Azul : Exor_Cofre_Base { override string ExorGetColor() { return "azul"; } }
class Exor_Cofre_Verde : Exor_Cofre_Base { override string ExorGetColor() { return "verde"; } }
class Exor_Cofre_Rojo : Exor_Cofre_Base { override string ExorGetColor() { return "rojo"; } }

// ----------------------------------------------------------------------------
// LUZ DEL EVENTO. Hereda de Chemlight_NearBubble (luz estable, sin humo) pero es
// fija: no se puede tomar/mover ni se rompe. El manager pone un circulo alrededor
// de la mesa y las despawnea al cerrar la ventana.
// ----------------------------------------------------------------------------
class Exor_CofreLight : ItemBase
{
	protected ScriptedLightBase m_ExorLight;	// cliente: la luz real (RoadflareLight)

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);
			ExorEncender();
		}
		// CLIENTE: crear la luz REAL (la roadflare no prende su luz sola via SwitchOn).
		if (GetGame().IsClient())
			ExorCrearLuz();
	}

	// prende la energia de la roadflare (para el humo/flama del item si la usa)
	void ExorEncender()
	{
		ComponentEnergyManager em = GetCompEM();
		if (em)
		{
			em.SetEnergy(9999999);
			if (!em.IsSwitchedOn())
				em.SwitchOn();
		}
	}

	// crea la luz de roadflare (ilumina de verdad) y la pega a este objeto
	void ExorCrearLuz()
	{
		if (m_ExorLight)
			return;
		m_ExorLight = ScriptedLightBase.CreateLight(RoadflareLight, "0 0 0");
		if (m_ExorLight)
			m_ExorLight.AttachOnObject(this, "0 0 0", "0 0 0");
	}

	override void EEDelete(EntityAI parent)
	{
		if (m_ExorLight)
		{
			GetGame().ObjectDelete(m_ExorLight);
			m_ExorLight = null;
		}
		super.EEDelete(parent);
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	override bool IsTakeable()
	{
		return false;
	}
}
