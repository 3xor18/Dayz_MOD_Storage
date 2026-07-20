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
	protected bool		m_ExorPowered;			// true = bateria puesta y con carga
	protected bool		m_ExorPoweredPrev;		// para detectar el cambio powered->unpowered
	protected int		m_ExorLastBatteryMs;	// ultimo tick de bateria (throttle)
	protected const int	EXOR_FRIDGE_BATTERY_MS = 60000;	// procesar bateria cada 60s
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

	// La lee Edible_Base::CanProcessDecay() -> la comida real dentro de una nevera
	// con energia NO se pudre.
	bool ExorIsPowered()
	{
		return m_ExorPowered;
	}

	protected CarBattery ExorGetBattery()
	{
		// Acepta bateria de AUTO (slot CarBattery) o de CAMION (slot TruckBattery).
		// TruckBattery hereda CarBattery -> el Cast funciona para ambas.
		CarBattery b = CarBattery.Cast(FindAttachmentBySlotName("CarBattery"));
		if (!b)
			b = CarBattery.Cast(FindAttachmentBySlotName("TruckBattery"));
		return b;
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

	// Logica de bateria. La llama el MANAGER en su tick central (cada 5s); throttleamos
	// a cada 60s. Drenaje calculado por el TIEMPO REAL transcurrido -> exacto sin importar
	// la cadencia. Los dias que dura una bateria llena salen del config (nevera_bateria_dias).
	override void ExorPeriodicTick(int now)
	{
		if (!GetGame().IsServer())
			return;
		// DESFASE INICIAL: si todas las neveras arrancan con el contador en 0 (carga del
		// server), sus ventanas de 60s quedan alineadas y se agotan/procesan TODAS en el
		// mismo tick. Al primer paso se les reparte un offset aleatorio dentro de la ventana
		// para que queden escalonadas. Solo afecta CUANDO corre el chequeo, no el drenaje:
		// el consumo se calcula por tiempo real transcurrido.
		if (m_ExorLastBatteryMs == 0)
		{
			m_ExorLastBatteryMs = now - Math.RandomInt(0, EXOR_FRIDGE_BATTERY_MS);
			return;
		}
		// throttle: correr esto cada ~60s (el manager llama cada 5s)
		if (now - m_ExorLastBatteryMs < EXOR_FRIDGE_BATTERY_MS)
			return;
		// tiempo REAL desde el ultimo chequeo (m_ExorLastBatteryMs ya no puede ser 0 aca)
		float elapsedSec = (now - m_ExorLastBatteryMs) / 1000.0;
		m_ExorLastBatteryMs = now;

		CarBattery battery = ExorGetBattery();
		bool powered = false;

		if (battery && battery.GetCompEM())
		{
			float energy = battery.GetCompEM().GetEnergy();
			if (energy > 0)
			{
				powered = true;
				float days = GetExorConfig().storage.nevera_bateria_dias;
				if (battery.IsInherited(TruckBattery))
					days = days * 2.0;	// bateria de CAMION dura el DOBLE que la de auto
				if (days > 0)	// 0 = la bateria no se descarga
				{
					// una bateria LLENA (max real) dura 'days' dias -> drenaje por seg transcurrido.
					float maxEnergy = battery.GetCompEM().GetEnergyMax();
					float drain = maxEnergy * elapsedSec / (days * 86400.0);
					float left = energy - drain;
					if (left < 0)
						left = 0;
					battery.GetCompEM().SetEnergy(left);
				}
			}
		}

		m_ExorPowered = powered;

		// Si la bateria se AGOTO mientras la comida estaba virtualizada, restaurarla para
		// que se pudra real (si no, quedaria congelada fuera del mundo para siempre).
		// Va por ExorRestoreRetry (no ExorDoRestore directo): esto corre DENTRO del loop del
		// manager, sin presupuesto, y un restore completo son cientos de entidades creadas.
		// Si varias neveras se quedan sin bateria en el mismo tick, se reparten en vez de
		// apilarse en un frame.
		if (m_ExorPoweredPrev && !powered && ExorIsVirtualized())
			ExorRestoreRetry();

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

		// Colocacion compartida: mueble estatico + base sobre la superficie real (raycast
		// que ignora el holograma). baseOffset 0.65 = calibrado in-game para la nevera.
		// baseOffset = -altura/2. La nevera mide 1.798m -> base tras el centrado de binarize en
		// -0.899. Antes era 0.65 (afinado para el raycast viejo); con el fix de superficie
		// (usa la Y del holograma) debe ser -0.899 como los lockers, si no se hunde.
		EntityAI fridge = Exor_OpenableStorage.ExorDeployFurniture(player, "Exor_Fridge", position, orientation, -0.899, GetHealth01("", ""));
		if (!fridge)
			return;
		Print("[3xorStorage] Refrigerador seteado en " + fridge.GetPosition().ToString());
		Delete();
	}
}
