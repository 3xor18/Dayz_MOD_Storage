// ============================================================================
// 3xorStorage - REFRIGERADOR
// ----------------------------------------------------------------------------
// Subclase FINA de Exor_OpenableStorage (mueble abrible + virtualizable). Solo
// agrega lo propio de una nevera:
//   - FILTRO: comida (Edible_Base) + agua/bebidas (Bottle_Base).
//   - BATERIA de coche (attachment "CarBattery"): una bateria LLENA dura ~3 dias.
//   - CONSERVACION por energia (optimizada, sin lag para 55 players):
//       * CON bateria  -> la comida se VIRTUALIZA (sale del mundo = 0 costo) y NO
//         se pudre (ni real -CanProcessDecay- ni virtualizada -no existe-).
//         Al restaurarla aparece FRIA (no congelada).
//       * SIN bateria  -> la comida perecedera se deja REAL (no se virtualiza) y
//         se pudre normal con el motor vanilla. Si la bateria se agota mientras
//         estaba virtualizada, se restaura para que empiece a pudrirse.
//     Agua/bebidas (no se pudren) se virtualizan siempre.
//   - Se COLOCA con holograma (item empacado) y se RE-EMPACA vacia + con un
//     destornillador en la mano (ver ExorActionPackFridge).
// La logica de abrir/cerrar, animacion de puerta, virtualizacion, persistencia,
// colision e indestructibilidad vienen de Exor_OpenableStorage.
// ============================================================================

class Exor_Fridge : Exor_OpenableStorage
{
	// --- BATERIA / energia ---
	protected ref Timer	m_ExorFridgeTimer;
	protected bool		m_ExorPowered;		// true = bateria puesta y con carga
	protected bool		m_ExorPoweredPrev;	// para detectar el cambio powered->unpowered
	protected const float	EXOR_FRIDGE_TICK = 60.0;	// tick de bateria (s)
	protected const float	EXOR_FRIDGE_DAYS = 3.0;		// una bateria LLENA dura ~3 dias
	// temperatura "fria pero NO congelada" que se pone a la comida al restaurarla con bateria
	protected const float	EXOR_FRIDGE_COLD_TEMP = 3.0;

	// --- hooks de la base ---
	override string ExorGetDoorAnimSource()	{ return "Lid"; }
	override string ExorGetPackedType()		{ return "Exor_Refrigerador_Packed"; }

	// FILTRO: comida (vegetales, carnes, latas, sodas = Edible_Base) + agua
	// (cantimplora, botella, water pouch, olla = Bottle_Base, NO heredan Edible_Base).
	override bool ExorCanStore(EntityAI item)
	{
		return item.IsInherited(Edible_Base) || item.IsInherited(Bottle_Base);
	}

	// VIRTUALIZAR: con bateria siempre; sin bateria solo si NO hay comida perecedera
	// (para dejarla real y que se pudra). Agua/bebidas no cuentan (no se pudren).
	override bool ExorCanVirtualizeNow()
	{
		if (m_ExorPowered)
			return true;
		return !ExorHasPerishableFood();
	}

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			m_ExorFridgeTimer = new Timer(CALL_CATEGORY_SYSTEM);
			m_ExorFridgeTimer.Run(EXOR_FRIDGE_TICK, this, "ExorFridgeTick", null, true);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (m_ExorFridgeTimer)
			m_ExorFridgeTimer.Stop();
		super.EEDelete(parent);
	}

	// La lee Edible_Base::CanProcessDecay() -> la comida real dentro de una nevera
	// con energia NO se pudre.
	bool ExorIsPowered()
	{
		return m_ExorPowered;
	}

	protected CarBattery ExorGetBattery()
	{
		return CarBattery.Cast(FindAttachmentBySlotName("CarBattery"));
	}

	// hay comida perecedera (no podrida) en el cargo?
	bool ExorHasPerishableFood()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return false;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return false;
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			Edible_Base food = Edible_Base.Cast(cargo.GetItem(i));
			if (food && food.GetFoodStageType() != FoodStageType.ROTTEN)
				return true;
		}
		return false;
	}

	void ExorFridgeTick()
	{
		if (!GetGame().IsServer())
			return;

		CarBattery battery = ExorGetBattery();
		bool powered = false;

		if (battery && battery.GetCompEM())
		{
			float energy = battery.GetCompEM().GetEnergy();
			if (energy > 0)
			{
				powered = true;
				// Drenaje para que una bateria LLENA dure EXOR_FRIDGE_DAYS (usa su max real).
				float maxEnergy = battery.GetCompEM().GetEnergyMax();
				float drain = maxEnergy * EXOR_FRIDGE_TICK / (EXOR_FRIDGE_DAYS * 86400.0);
				float left = energy - drain;
				if (left < 0)
					left = 0;
				battery.GetCompEM().SetEnergy(left);
			}
		}

		m_ExorPowered = powered;

		// Si la bateria se AGOTO mientras la comida estaba virtualizada, restaurarla para
		// que se pudra real (si no, quedaria congelada fuera del mundo para siempre).
		if (m_ExorPoweredPrev && !powered && ExorIsVirtualized())
			ExorDoRestore();

		m_ExorPoweredPrev = powered;
	}

	// Al restaurar del JSON: si hay bateria, la comida aparece FRIA (no congelada).
	// (Si no hay bateria la comida perecedera ni se virtualiza, asi que aca ya tiene
	// bateria o es agua/bebida -> ponerla fria no molesta.)
	override void ExorOnItemsRestored(ExorVO_ContainerFile f)
	{
		if (!GetGame().IsServer())
			return;
		if (!m_ExorPowered)
			return;

		CargoBase cargo = GetInventory().GetCargo();
		if (!cargo)
			return;
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			ItemBase it = ItemBase.Cast(cargo.GetItem(i));
			if (it && it.GetTemperature() > EXOR_FRIDGE_COLD_TEMP)
				it.SetTemperature(EXOR_FRIDGE_COLD_TEMP);
		}
	}
}

// ============================================================================
//  Refrigerador EMPACADO (item transportable, se coloca con HOLOGRAMA)
// ============================================================================
class Exor_Refrigerador_Packed : ItemBase
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			SetAllowDamage(false);	// indestructible, consistente con el mod
	}

	override bool IsDeployable()
	{
		return true;
	}

	// ExorTerritory_Items.c ya modea CanBePlaced para bloquear territorio ajeno /
	// zonas no-build; devolvemos true y ese mod decide el resto.
	override bool CanBePlaced(Man player, vector position)
	{
		return true;
	}

	override string CanBePlacedFailMessage(Man player, vector position)
	{
		return "";
	}

	override void SetActions()
	{
		super.SetActions();
		AddAction(ActionTogglePlaceObject);
		AddAction(ActionDeployObject);
	}

	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);
		if (!GetGame().IsServer())
			return;

		// Colocacion: crear el objeto y apoyar su BASE (origen del modelo, Y=0) EXACTO
		// sobre la superficie del terreno. El holograma reportaba una Y por debajo del
		// piso (por el punto bbox del modelo empacado) -> la nevera se hundia. Forzamos
		// Y = altura del terreno en ese XZ. create_local=false -> objeto persistente.
		// RAYCAST a la superficie REAL bajo el punto de colocacion (piso de base O terreno).
		// IMPORTANTE: ignorar el HOLOGRAMA del preview (sigue ahi al setear) -> si no, el
		// raycast lo golpea a el en vez del piso (en el pasto daba +1.5m). Fallback: SurfaceY.
		PlayerBase pb = PlayerBase.Cast(player);
		Object ignoreObj = pb;
		if (pb)
		{
			Hologram holo = pb.GetHologramServer();
			if (holo && holo.GetProjectionEntity())
				ignoreObj = holo.GetProjectionEntity();
		}
		vector rayStart = Vector(position[0], position[1] + 2.5, position[2]);
		vector rayEnd   = Vector(position[0], position[1] - 2.5, position[2]);
		vector hitPos, hitNorm;
		int hitComp;
		float surfaceY;
		if (DayZPhysics.RaycastRV(rayStart, rayEnd, hitPos, hitNorm, hitComp, null, null, ignoreObj, true, false))
			surfaceY = hitPos[1];
		else
			surfaceY = GetGame().SurfaceY(position[0], position[2]);

		// Offset entre el origen del modelo y sus patas (calibrado in-game). Se resta para
		// apoyar las patas justo sobre la superficie del raycast.
		float baseOffset = 0.65;
		vector pos = position;
		pos[1] = surfaceY - baseOffset;

		// create_physics=FALSE -> ESTATICO (no se asienta). Colision del jugador = geometria.
		EntityAI fridge = EntityAI.Cast(GetGame().CreateObject("Exor_Fridge", pos, false, false, false));
		if (!fridge)
		{
			Print("[3xorStorage] ERROR: no se pudo crear Exor_Fridge al setear el refrigerador");
			return;
		}
		fridge.SetPosition(pos);
		fridge.SetOrientation(orientation);
		fridge.SetHealth01("", "", GetHealth01("", ""));
		Delete();
	}
}
