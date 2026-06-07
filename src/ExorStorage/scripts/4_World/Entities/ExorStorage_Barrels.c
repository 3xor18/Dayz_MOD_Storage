// ============================================================================
// 3xorStorage - Entidades: barriles desplegados y empaquetados
// ============================================================================

// ---------------------------------------------------------------------------
// Barril desplegado (funcional). Hereda todo el comportamiento del barril
// vanilla (abrir/cerrar, cargo, lluvia) y agrega la accion de empaquetar.
// ---------------------------------------------------------------------------
class Exor_Barrel_Base : Barrel_ColorBase
{
	// OJO: la accion de empaquetar NO va aca (SetActions del item = solo con el
	// item EN MANOS). Las acciones sobre el barril en el mundo se registran en
	// PlayerBase (ver ExorStorage_Player.c).

	// Indestructible: el loot guardado nunca se pierde por explosiones/balas
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);
		}
	}

	// No lockeable: bloquea CodeLock / candados de combinacion / padlocks
	// de cualquier mod (el balance es: quien llega al barril, lo abre)
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

	// Solo se puede empaquetar si esta cerrado, sano y sin items adentro.
	// El liquido (agua de lluvia) NO bloquea: se descarta al empaquetar.
	// (Los barriles spawneados por admin/CE pueden traer agua invisible.)
	bool ExorCanBePacked()
	{
		if (IsOpen())
			return false;

		if (IsRuined())
			return false;

		if (GetInventory())
		{
			if (GetInventory().AttachmentCount() > 0)
				return false;

			CargoBase cargo = GetInventory().GetCargo();
			if (cargo && cargo.GetItemCount() > 0)
				return false;
		}

		return true;
	}

	// Classname del item empaquetado equivalente (lo define cada variante)
	string ExorGetPackedType()
	{
		return "";
	}
}

class Exor_Barrel_500 : Exor_Barrel_Base
{
	override string ExorGetPackedType()
	{
		return "Exor_Barrel_500_Packed";
	}
}

// ---------------------------------------------------------------------------
// Barril empaquetado (item transportable, sin cargo). Se despliega con accion.
// ---------------------------------------------------------------------------
class Exor_Barrel_Packed_Base : ItemBase
{
	// La caja tambien es indestructible (consistente con el barril)
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);
		}
	}

	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionDeployBarrel);
	}

	// Classname del barril desplegado equivalente (lo define cada variante)
	string ExorGetDeployedType()
	{
		return "";
	}
}

class Exor_Barrel_500_Packed : Exor_Barrel_Packed_Base
{
	override string ExorGetDeployedType()
	{
		return "Exor_Barrel_500";
	}
}
