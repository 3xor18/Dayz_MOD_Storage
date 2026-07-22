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

	// GUARD AUTO-RECUPERACION (por SECUENCIA): una nevera con cargo CORRUPTO crashea el server
	// al cargar su inventario, y ese crash ocurre DENTRO de super.OnStoreLoad (el motor deserializa
	// el cargo ahi). NO podemos escribir NADA util DESPUES de super (nunca vuelve), asi que hay que
	// identificar la nevera ANTES de super. La POSICION NO sirve (no esta seteada antes de super en
	// una entidad persistida). Lo que SI esta disponible es un contador de secuencia (el N-esimo
	// fridge en cargar, en orden determinista). Ver ExorFridgeCanary.
	//   - VALVULA DE EMERGENCIA (config saltear_carga_neveras=true): descarto TODAS las neveras sin
	//     tocar su cargo -> arranque garantizado (arrancan vacias). Ultimo recurso, sin build nuevo.
	//   - Si el canary tiene MI numero N (crashee el boot pasado cargando la N) -> return false SIN
	//     llamar a super -> el motor NO deserializa mi cargo corrupto -> el server ARRANCA. El
	//     self-heal me recrea VACIA. Aisla SOLO a mi; las demas neveras cargan normal.
	//   - Si no, escribo mi N (marca "cargando la N-esima") ANTES de super. Si super crashea, el
	//     canary queda con N -> el proximo arranque descarta la N-esima. Si la carga completa
	//     sobrevive, el manager borra el canary al 1er tick.
	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (GetGame().IsServer())
		{
			// valvula de emergencia: saltear la carga de TODAS las neveras
			if (GetExorConfig().storage.saltear_carga_neveras)
			{
				Print(string.Format("%1 nevera SALTEADA por config saltear_carga_neveras -> arranca vacia", ExorStorageConstants.LOG));
				return false;
			}
			// AUTO: el arranque anterior murio y el RPT acusaba a una nevera corrupta. Saltear
			// el contenido de TODAS las neveras por este arranque levanta el server sin tocar
			// ningun archivo de persistencia (bases/carpas/loot de la zona quedan intactos).
			// Es mas barato que la cuarentena. Ver ExorBootRepair, etapa 3.
			if (ExorBootRepair.SaltearNeveras())
			{
				Print(string.Format("%1 nevera SALTEADA por BootRepair (nevera corrupta detectada en el arranque anterior) -> arranca vacia", ExorStorageConstants.LOG));
				return false;
			}
			int myseq = ExorFridgeCanary.NextSeq();		// soy la nevera N-esima en cargar
			if (ExorFridgeCanary.ReadSeq() == myseq)
			{
				ExorFridgeCanary.Clear();
				Print(string.Format("%1 nevera #%2 con cargo CORRUPTO descartada -> se recrea VACIA", ExorStorageConstants.LOG, myseq));
				return false;	// NO llamar a super -> el motor no deserializa el cargo corrupto
			}
			ExorFridgeCanary.WriteSeq(myseq);	// marca: cargando la N-esima (antes de super)
		}
		if (!super.OnStoreLoad(ctx, version))	// AQUI puede crashear el motor con el cargo corrupto
			return false;
		// NOTA: NO se borra el canary aca por-nevera. La marca queda con la ultima N cargada; el
		// manager la borra al 1er tick (= carga completa OK). Asi, si el crash ocurre EN medio de
		// la carga, el canary conserva la N de la nevera que lo causo.
		return true;
	}

	// La nevera NUNCA es idle: aunque este cerrada+virtualizada, su ExorPeriodicTick tiene que
	// correr para descargar la bateria. Si se salteara, la bateria no se drenaria.
	override bool ExorIsIdle() { return false; }

	// FILTRO: comida (vegetales, carnes, latas, sodas = Edible_Base) + agua (cantimplora,
	// botella, water pouch = Bottle_Base). NO acepta CONTENEDORES (ollas/Pot y cualquier item
	// con cargo propio): un contenedor anidado dentro de la nevera es la fuente probable del
	// inventario corrupto que crashea el server al cargar (y del NULL de Pot sin food-stage
	// visto 20-jul). Solo comida/bebida SUELTA, sin items que metan items adentro.
	override bool ExorCanStore(EntityAI item)
	{
		if (!item)
			return false;
		// rechazar cualquier cosa que tenga cargo propio (olla/Pot, bidones, etc.)
		if (item.GetInventory() && item.GetInventory().GetCargo())
			return false;
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
		// UN SOLO slot ("ExorBattery", en config): acepta bateria de AUTO o de CAMION.
		// TruckBattery hereda CarBattery -> el Cast funciona para ambas; luego
		// ExorPeriodicTick chequea IsInherited(TruckBattery) para el drenaje x2.
		return CarBattery.Cast(FindAttachmentBySlotName("ExorBattery"));
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
			// GUARD: un Edible SIN food stage (olla vacia, etc.) tira NULL pointer en
			// GetFoodStageType() -> CRASH del server. Visto en produccion 20-jul 17:49
			// (una Pot en la nevera). Chequear GetFoodStage() antes.
			if (!food || !food.GetFoodStage())
				continue;	// sin comida real adentro -> no cuenta como perecedero
			if (food.GetFoodStageType() != FoodStageType.ROTTEN)
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
