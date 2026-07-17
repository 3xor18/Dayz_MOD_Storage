// ============================================================================
// 3xorStorage - REFRIGERADOR (contenedor abrible estilo "openable container")
// ----------------------------------------------------------------------------
// REESCRITO 17-jul con el patron de MMG Base Storage (Container_Base openable):
// antes heredaba del BARRIL 3xor, pero su maquinaria (auto-cierre + virtualizacion
// + cooldown anti-dupe) desincronizaba el abrir/cerrar y ocultaba el inventario.
// Ahora es un Container_Base LIMPIO:
//   - Estado abierto/cerrado propio (m_ExorFridgeOpen), net-sincronizado.
//   - Open()/Close() bloquean/desbloquean el inventario (Lock/UnlockInventory) ->
//     con la nevera CERRADA no se accede al cargo; ABIERTA si. Igual que MMG.
//   - PUERTA animada: SetAnimationPhase("Lid", abierto?1:0) ligado a ESE estado.
//   - FILTRO de cargo: solo comida/bebida/agua (Edible_Base) y solo si esta ABIERTA.
//   - COLISION SOLIDA: via config (physLayer="item_large" + weight) + la caja del
//     Geometry LOD del modelo. Ya NO se atraviesa.
//   - BATERIA de coche (attachment "CarBattery"): con carga conserva la comida y se
//     descarga con el tiempo. Empaca sin bateria.
//   - Se COLOCA con holograma (item empacado) y se puede RE-EMPACAR (accion propia).
// Persistencia: nativa de Container_Base (el cargo sobrevive reinicios) + guardamos
// el estado abierto/cerrado.
// ============================================================================

class Exor_Fridge : Container_Base
{
	// --- estado ABIERTO/CERRADO (net-sync propio, patron MMG) ---
	protected bool		m_ExorFridgeOpen;		// true = puerta abierta / inventario accesible
	protected bool		m_ExorDoorAnimApplied;	// ultimo estado aplicado a la animacion
	protected bool		m_ExorDoorAnimInit;

	// --- BATERIA / energia ---
	protected ref Timer	m_ExorFridgeTimer;
	protected bool		m_ExorPowered;			// true = bateria puesta y con carga
	protected const float	EXOR_FRIDGE_TICK = 30.0;	// cada cuanto tickea la bateria (s)
	protected const float	EXOR_FRIDGE_DRAIN = 5.0;	// energia consumida por tick

	void Exor_Fridge()
	{
		RegisterNetSyncVariableBool("m_ExorFridgeOpen");
	}

	override void EEInit()
	{
		super.EEInit();

		if (GetGame().IsServer())
		{
			SetAllowDamage(false);		// indestructible (consistente con el mod)
			m_ExorFridgeOpen = false;	// arranca CERRADA
			if (GetInventory())
				GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);	// cerrada = sin acceso
			SetSynchDirty();

			// Timer server-side de bateria/conservacion.
			m_ExorFridgeTimer = new Timer(CALL_CATEGORY_SYSTEM);
			m_ExorFridgeTimer.Run(EXOR_FRIDGE_TICK, this, "ExorFridgeTick", null, true);
		}

		ExorUpdateDoor();	// puerta segun estado (cerrada al aparecer)
	}

	override void EEDelete(EntityAI parent)
	{
		if (m_ExorFridgeTimer)
			m_ExorFridgeTimer.Stop();
		super.EEDelete(parent);
	}

	// ---------------------- ABRIR / CERRAR (patron MMG) ------------------------
	override void Open()
	{
		m_ExorFridgeOpen = true;
		SetSynchDirty();		// empuja el estado a los clientes
		if (GetInventory())
			GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);	// abierta = accesible
		ExorUpdateDoor();
	}

	override void Close()
	{
		m_ExorFridgeOpen = false;
		SetSynchDirty();
		if (GetInventory())
			GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
		ExorUpdateDoor();
	}

	override bool IsOpen()
	{
		return m_ExorFridgeOpen;
	}

	// En el CLIENTE el estado llega por sync -> reflejar la puerta.
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		ExorUpdateDoor();
	}

	// Anima la puerta (seleccion "lid" sobre "lid_axis"; ver model.cfg). Solo
	// (re)aplica cuando el estado CAMBIA (evita re-animar en cada sync).
	void ExorUpdateDoor()
	{
		bool open = IsOpen();
		if (m_ExorDoorAnimInit && open == m_ExorDoorAnimApplied)
			return;
		m_ExorDoorAnimInit = true;
		m_ExorDoorAnimApplied = open;

		float phase = 0.0;
		if (open)
			phase = 1.0;
		SetAnimationPhase("Lid", phase);
	}

	// ---------------------- FILTRO DE CARGO ------------------------------------
	// Solo comida/bebida/agua (Edible_Base) y SOLO con la nevera abierta.
	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (!IsOpen())
			return false;
		if (item && !item.IsInherited(Edible_Base))
			return false;
		return super.CanReceiveItemIntoCargo(item);
	}

	override bool CanReleaseCargo(EntityAI cargo)
	{
		return IsOpen();
	}

	// La nevera desplegada NO se levanta a la mano ni entra en otro contenedor
	// (es un mueble colocado; para moverla se RE-EMPACA con la accion propia).
	override bool CanPutInCargo(EntityAI parent)
	{
		return false;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	override bool IsHeavyBehaviour()
	{
		return true;
	}

	override bool IsTwoHandedBehaviour()
	{
		return true;
	}

	// ---------------------- PERSISTENCIA del estado ----------------------------
	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorFridgeOpen);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		if (!ctx.Read(m_ExorFridgeOpen))
			return false;
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		// reflejar el estado guardado tras cargar (bloqueo de inventario + puerta)
		if (GetInventory())
		{
			if (m_ExorFridgeOpen)
				GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);
			else
				GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
		}
		SetSynchDirty();
		ExorUpdateDoor();
	}

	// ---------------------- ACCIONES -------------------------------------------
	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionOpenCloseFridge);
		AddAction(ExorActionPackFridge);
	}

	// ---------------------- RE-EMPAQUE -----------------------------------------
	// La nevera se puede empaquetar si esta CERRADA, sana y VACIA (sin comida ni
	// bateria). Lo usa ExorActionPackFridge.
	bool ExorCanBePacked()
	{
		if (IsOpen())
			return false;
		if (IsDamageDestroyed())
			return false;
		if (GetInventory())
		{
			CargoBase cargo = GetInventory().GetCargo();
			if (cargo && cargo.GetItemCount() > 0)
				return false;
			if (GetInventory().AttachmentCount() > 0)	// bateria puesta
				return false;
		}
		return true;
	}

	string ExorGetPackedType()
	{
		return "Exor_Refrigerador_Packed";
	}

	// ---------------------- LOGICA DE BATERIA ----------------------------------
	protected CarBattery ExorGetBattery()
	{
		return CarBattery.Cast(FindAttachmentBySlotName("CarBattery"));
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
				float left = energy - EXOR_FRIDGE_DRAIN;
				if (left < 0)
					left = 0;
				battery.GetCompEM().SetEnergy(left);
			}
		}

		m_ExorPowered = powered;

		if (powered)
			ExorPreserveCargoFood();
	}

	// Conserva la comida del cargo mientras hay bateria (FASE 1: la mantiene seca,
	// el motor pudre mucho mas lento la comida seca+fria).
	protected void ExorPreserveCargoFood()
	{
		CargoBase cargo = GetInventory().GetCargo();
		if (!cargo)
			return;

		int count = cargo.GetItemCount();
		for (int i = 0; i < count; i++)
		{
			Edible_Base food = Edible_Base.Cast(cargo.GetItem(i));
			if (food)
				food.SetWet(0.0);
		}
	}
}

// ============================================================================
//  Refrigerador EMPACADO (item transportable, se coloca con HOLOGRAMA)
// ----------------------------------------------------------------------------
//  Usa el sistema de PLACEMENT de DayZ: aparece un preview fantasma, elegis
//  donde va y con hold se setea (como plantar una carpa).
// ============================================================================
class Exor_Refrigerador_Packed : ItemBase
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			SetAllowDamage(false);	// indestructible, consistente con el barril/mod
	}

	// Habilita el holograma de colocacion (preview de posicion).
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

	// Placement vanilla: HACEN FALTA LAS DOS acciones.
	//   - ActionTogglePlaceObject: enciende el modo placing y muestra el ghost.
	//   - ActionDeployObject: la colocacion en si (hold); exige IsPlacingLocal().
	override void SetActions()
	{
		super.SetActions();
		AddAction(ActionTogglePlaceObject);
		AddAction(ActionDeployObject);
	}

	// Al confirmar: crear el refri desplegado en la posicion/rotacion elegida y
	// borrar la caja empacada. (Solo server.)
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);
		if (!GetGame().IsServer())
			return;

		EntityAI fridge = EntityAI.Cast(GetGame().CreateObjectEx("Exor_Fridge", position, ECE_PLACE_ON_SURFACE));
		if (!fridge)
		{
			Print("[3xorStorage] ERROR: no se pudo crear Exor_Fridge al setear el refrigerador");
			return;
		}
		fridge.SetOrientation(orientation);
		fridge.SetHealth01("", "", GetHealth01("", ""));	// transfiere salud
		Print("[3xorStorage] Refrigerador seteado en " + position.ToString());
		Delete();	// borra la caja empacada
	}
}
