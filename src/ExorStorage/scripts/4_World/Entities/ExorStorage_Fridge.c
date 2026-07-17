// ============================================================================
// 3xorStorage - REFRIGERADOR retro (МОСКВА)
// ----------------------------------------------------------------------------
// Mueble de guardado que hereda de Exor_Barrel_Base -> reusa GRATIS toda la
// maquinaria del barril 3xor (virtualizacion "JSON en vivo", registro en
// ExorVO_Manager, abrir/cerrar tapa, auto-cerrado, empaque, indestructibilidad,
// anti-candado). Cambia:
//   - FILTRO de cargo: SOLO comida / bebida / agua (Edible_Base).
//   - PUERTA animada: source "Lid" ligado a la seleccion "lid" sobre "lid_axis"
//     (ver model.cfg). Abre/cierra SIEMPRE, con o sin bateria.
//   - BATERIA de coche (attachment "CarBattery"): con bateria cargada -> conserva
//     la comida + (a futuro) luz; la bateria se DESCARGA con el tiempo. Sin
//     bateria o descargada -> la comida se pudre normal y no hay luz.
//   - Empaque -> "Exor_Refrigerador_Packed" (se coloca con HOLOGRAMA).
// ============================================================================

class Exor_Fridge : Exor_Barrel_Base
{
	// --- BATERIA / energia ---
	protected ref Timer	m_ExorFridgeTimer;
	protected bool		m_ExorPowered;			// true = bateria puesta y con carga

	// --- PUERTA (animacion sincronizada, patron MMG/contenedor-abrible vanilla) ---
	// Estado de la puerta REGISTRADO como net-sync propio. No dependemos del sync
	// interno del barril: el server lo setea + SetSynchDirty(), y el CLIENTE lo
	// recibe en OnVariablesSynchronized y anima. Asi la puerta gira para todos.
	protected bool		m_ExorDoorOpen;
	// Ultimo estado aplicado a la animacion. Evita RE-DISPARAR la animacion en cada
	// OnVariablesSynchronized (el barril sincroniza otras vars seguido: virtualizacion,
	// comida, etc.) -> sin esto la puerta re-animaba "abrir" aunque ya estuviera abierta.
	protected bool		m_ExorDoorAnimApplied;
	protected bool		m_ExorDoorAnimInit;		// true tras la 1ra aplicacion

	void Exor_Fridge()
	{
		RegisterNetSyncVariableBool("m_ExorDoorOpen");
	}
	// cada cuanto tickea la logica de bateria/conservacion (segundos)
	protected const float	EXOR_FRIDGE_TICK = 30.0;
	// energia que consume la nevera por tick (unidades de energia de la CarBattery).
	// La CarBattery vanilla arranca con bastante carga; ajustable tras testear.
	protected const float	EXOR_FRIDGE_DRAIN = 5.0;

	// FILTRO: el refrigerador SOLO acepta comida, bebida y agua en el CARGO.
	// (La bateria NO pasa por aca: va al slot de attachment "CarBattery".)
	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (item && !item.IsInherited(Edible_Base))
			return false;
		return super.CanReceiveItemIntoCargo(item);
	}

	// desplegado -> empacado (al empaquetarlo con la accion del barril, heredada)
	override string ExorGetPackedType()
	{
		return "Exor_Refrigerador_Packed";
	}

	// ----- ANIMACION DE LA PUERTA (control explicito, estilo MMG) --------------
	// La puerta (seleccion "lid") rota sobre "lid_axis" segun la fase del source
	// "Lid": 0 = cerrada (angle0), 1 = abierta (angle1). Ver model.cfg.
	// Lee el estado del net-sync var m_ExorDoorOpen -> mismo valor en server y
	// cliente, se ejecuta en AMBOS lados (server: Open/Close; cliente: OnVarsSync).
	void ExorUpdateDoor()
	{
		// Solo (re)aplicar cuando el estado CAMBIA respecto a lo ya aplicado. Sin este
		// guard, cada sync re-llamaba SetAnimationPhase y la puerta re-animaba "abrir".
		if (m_ExorDoorAnimInit && m_ExorDoorOpen == m_ExorDoorAnimApplied)
			return;
		m_ExorDoorAnimInit = true;
		m_ExorDoorAnimApplied = m_ExorDoorOpen;

		float phase = 0.0;
		if (m_ExorDoorOpen)
			phase = 1.0;
		SetAnimationPhase("Lid", phase);
	}

	override void EEInit()
	{
		super.EEInit();
		m_ExorDoorOpen = IsOpen();	// sincroniza el estado inicial con el del contenedor
		ExorUpdateDoor();	// al aparecer: puerta segun estado (recien desplegada = cerrada)

		// Timer server-side de bateria/conservacion.
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

	// Abrir/cerrar corre en el SERVER: actualiza el estado, lo marca para sync y
	// anima localmente (asi el jugador que la abre la ve al instante).
	override void Open()
	{
		super.Open();
		m_ExorDoorOpen = true;
		SetSynchDirty();		// empuja m_ExorDoorOpen a los clientes
		ExorUpdateDoor();
	}

	override void Close()
	{
		super.Close();
		m_ExorDoorOpen = false;
		SetSynchDirty();
		ExorUpdateDoor();
	}

	// En el CLIENTE la puerta se actualiza al recibir m_ExorDoorOpen sincronizado.
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		ExorUpdateDoor();
	}

	// ----- LOGICA DE BATERIA ---------------------------------------------------
	// Devuelve la CarBattery puesta (o null). Su energia vive en el
	// ComponentEnergyManager (GetCompEM()).
	protected CarBattery ExorGetBattery()
	{
		return CarBattery.Cast(FindAttachmentBySlotName("CarBattery"));
	}

	// Tick server: mira si hay bateria con carga -> "powered". Si esta powered,
	// descarga la bateria y conserva la comida.
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
				// descarga: consume energia por tick (no baja de 0)
				float left = energy - EXOR_FRIDGE_DRAIN;
				if (left < 0)
					left = 0;
				battery.GetCompEM().SetEnergy(left);
			}
		}

		m_ExorPowered = powered;

		if (powered)
			ExorPreserveCargoFood();
		// TODO(luz): cuando el modelo tenga una seleccion "light" emisiva, prenderla
		// aca segun 'powered' (SetEmissive / hidden selection). Fase 2.
	}

	// Conserva la comida del cargo mientras hay bateria.
	// FASE 1 (segura, compila): mantiene la comida SECA (SetWet 0) -> el motor de
	// DayZ pudre mucho mas lento la comida seca+fria. NO congela del todo la
	// pudricion todavia.
	// FASE 2 (pendiente, requiere el source vanilla de Edible_Base): resetear el
	// progreso de decay para que NO avance a ROTTEN mientras esta powered, y cubrir
	// tambien la comida VIRTUALIZADA (el barril virtualiza el cargo).
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
