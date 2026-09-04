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
	// No hay estado de bateria cacheado ni timers: todo se deriva de la carga real de la
	// bateria puesta y del tiempo transcurrido. Ver ExorMinutosDeCarga / ExorDrenarBateria.
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
	//   - La CARGA SEGURA por tipo (BootRepair + la valvula de config saltear_carga_neveras) ya
	//     esta en Exor_OpenableStorage.OnStoreLoad, que corre en el super de abajo... salvo que
	//     el canary tiene que decidir ANTES, asi que se consulta aca tambien.
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
			// carga segura de esta clase (config saltear_carga_neveras o rastro en el RPT del
			// arranque anterior): ni canary ni stream, se delega en el super, que corta solo.
			// Se chequea aca para no gastar el numero de secuencia del canary al pedo.
			if (ExorBootRepair.SaltearTipo(GetType()))
				return super.OnStoreLoad(ctx, version);
			int myseq = ExorFridgeCanary.NextSeq();		// soy la nevera N-esima en cargar
			if (ExorFridgeCanary.ReadSeq() == myseq)
			{
				ExorFridgeCanary.Clear();
				// el texto lleva CARGA-SEGURA a proposito: marca este arranque como "cura
				// aplicada" para ExorBootRepair.ExorAcusadosEnRpt, asi las lineas de corrupcion
				// que el engine loguee por esta nevera descartada no se leen como una
				// acusacion nueva en el arranque siguiente (evita el bucle).
				Print(string.Format("%1 CARGA-SEGURA: nevera #%2 con cargo CORRUPTO descartada -> se recrea VACIA", ExorStorageConstants.LOG, myseq));
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

	// FILTRO: comida (vegetales, carnes, latas, sodas = Edible_Base) + agua (cantimplora,
	// botella, water pouch = Bottle_Base). NO acepta CONTENEDORES (ollas/Pot y cualquier item
	// con cargo propio): un contenedor anidado dentro de la nevera es la fuente probable del
	// inventario corrupto que rompia el arranque (y del NULL de Pot sin food-stage visto
	// 20-jul). Solo comida/bebida SUELTA, sin items que metan items adentro.
	override bool ExorCanStore(EntityAI item)
	{
		if (!item)
			return false;
		// rechazar cualquier cosa que tenga cargo propio (olla/Pot, bidones, etc.)
		if (item.GetInventory() && item.GetInventory().GetCargo())
			return false;
		return item.IsInherited(Edible_Base) || item.IsInherited(Bottle_Base);
	}

	// VIRTUALIZAR SIEMPRE (se hereda el default de la base, que ahora es true).
	// CAUSA RAIZ del "el server no arranca por la nevera": esta era la UNICA clase del mod
	// que a proposito dejaba items REALES en el cargo del motor (sin bateria, para que la
	// comida se pudriera sola). Todo lo demas virtualiza a JSON y llega al guardado con el
	// cargo VACIO, asi que el motor no tiene nada suyo que serializar ni que corromper.
	// La regla de juego NO cambia: sin bateria la comida se sigue pudriendo, solo que el
	// tiempo se le cobra al restaurarla (ver ExorOnItemsRestored).

	// La nevera YA NO tiene tick propio: el drenaje de bateria y el envejecimiento de la
	// comida se calculan POR TIEMPO TRANSCURRIDO cuando hacen falta (ver ExorDrenarBateria y
	// ExorEnvejecerComida), no tickeando. Por eso puede ser idle como cualquier otro mueble y
	// el fast-skip del manager vuelve a servir para ella.
	//
	// POR QUE IMPORTA: ExorIsIdle() devolvia SIEMPRE false para poder drenar la bateria, o sea
	// que CADA nevera del mapa entraba a ExorTick + ExorPeriodicTick cada 5 segundos aunque
	// estuviera cerrada, virtualizada y sin tocar hace dias. Con cientos de neveras eso anulaba
	// justo la optimizacion que hace que el tick escale.
	//
	// Un estado que solo depende del tiempo no necesita que nadie lo actualice: se deriva
	// cuando alguien lo mira. Es la diferencia entre O(neveras) por tick y O(1) por apertura.

	// --- BATERIA (calculo perezoso) ---
	// Minutos de carga que le quedan a la bateria puesta (0 = sin bateria o agotada).
	int ExorMinutosDeCarga()
	{
		CarBattery battery = ExorGetBattery();
		if (!battery || !battery.GetCompEM())
			return 0;
		float energy = battery.GetCompEM().GetEnergy();
		if (energy <= 0)
			return 0;
		float maxEnergy = battery.GetCompEM().GetEnergyMax();
		if (maxEnergy <= 0)
			return 0;
		float days = GetExorConfig().storage.nevera_bateria_dias;
		if (battery.IsInherited(TruckBattery))
			days = days * 2.0;	// bateria de CAMION dura el DOBLE que la de auto
		if (days <= 0)
			return 999999;		// 0 en config = la bateria no se descarga nunca
		float minutosLlena = days * 1440.0;
		return (int)(minutosLlena * (energy / maxEnergy));
	}

	// Le descuenta a la bateria los 'minutos' que pasaron. Devuelve cuantos de esos minutos
	// quedaron SIN energia (los que la comida tiene que envejecer).
	int ExorDrenarBateria(int minutos)
	{
		if (minutos <= 0)
			return 0;
		CarBattery battery = ExorGetBattery();
		if (!battery || !battery.GetCompEM())
			return minutos;		// sin bateria: todo el tiempo fue sin energia

		float days = GetExorConfig().storage.nevera_bateria_dias;
		if (battery.IsInherited(TruckBattery))
			days = days * 2.0;
		if (days <= 0)
			return 0;			// config: la bateria no se descarga -> siempre refrigerado

		float maxEnergy = battery.GetCompEM().GetEnergyMax();
		float energy = battery.GetCompEM().GetEnergy();
		if (maxEnergy <= 0 || energy <= 0)
			return minutos;

		float minutosLlena = days * 1440.0;
		float minutosRestantes = minutosLlena * (energy / maxEnergy);
		if (minutosRestantes >= minutos)
		{
			// alcanzo para todo el periodo: descontar lo consumido
			float gasto = maxEnergy * (minutos / minutosLlena);
			float left = energy - gasto;
			if (left < 0)
				left = 0;
			battery.GetCompEM().SetEnergy(left);
			return 0;
		}
		// se agoto en el medio: el resto del periodo fue sin energia
		battery.GetCompEM().SetEnergy(0);
		return minutos - (int)minutosRestantes;
	}

	// Estado de energia AHORA (lo lee Edible_Base.CanProcessDecay para la comida real que
	// hay adentro mientras la nevera esta abierta).
	bool ExorIsPowered()
	{
		return ExorMinutosDeCarga() > 0;
	}

	protected CarBattery ExorGetBattery()
	{
		// UN SOLO slot ("ExorBattery", en config): acepta bateria de AUTO o de CAMION.
		// TruckBattery hereda CarBattery -> el Cast funciona para ambas.
		return CarBattery.Cast(FindAttachmentBySlotName("ExorBattery"));
	}

	// Al guardar el JSON queda anotado si la nevera tenia ENERGIA en ese momento: con bateria
	// el contenido se conserva y NO se le cobra el tiempo; sin bateria si.
	override void ExorOnSnapshotWrite(ExorVO_ContainerFile f)
	{
		int carga = ExorMinutosDeCarga();
		f.vbat = carga;
		if (carga > 0)
			f.vpow = 1;
		else
			f.vpow = 0;
	}

	// Al restaurar del JSON, la nevera pone al dia DOS cosas que mientras estuvo virtualizada
	// nadie pudo ir actualizando (el contenido no existia y no hay tick por nevera):
	//
	//   1) la BATERIA se descarga por los minutos transcurridos;
	//   2) la comida ENVEJECE solo por los minutos en que NO hubo energia.
	//
	// El "cuanto hubo energia" sale del propio archivo: al guardarlo se anoto cuantos minutos
	// de carga le quedaban (vbat). Si estuvo virtualizada 3 dias y tenia 1 dia de bateria,
	// se refrigero 1 dia y se pudrio 2. Es exacto y no cuesta ningun tick.
	//
	// Ademas, si al final quedo energia, la comida aparece FRIA (no congelada).
	override void ExorOnItemsRestored(ExorVO_ContainerFile f)
	{
		if (!GetGame().IsServer())
			return;

		int minutos = 0;
		if (f && f.vmin > 0)
		{
			minutos = ExorTimeUtil.NowMinutes() - f.vmin;
			if (minutos < 0)
				minutos = 0;
			if (minutos > EXOR_FRIDGE_MAX_ENVEJ_MIN)
				minutos = EXOR_FRIDGE_MAX_ENVEJ_MIN;
		}

		// 1) bateria al dia. Devuelve los minutos que quedaron SIN energia.
		int sinEnergia = ExorDrenarBateria(minutos);

		// 2) envejecer la comida solo por esos minutos
		if (sinEnergia > 0)
			ExorEnvejecerComida(sinEnergia);

		// 3) si todavia hay carga, la comida sale fria
		if (ExorMinutosDeCarga() <= 0)
			return;
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		CargoBase cargo = inv.GetCargo();
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

	// Tope de lo que se le cobra a la comida de una sola vez. 7 dias alcanzan de sobra: el
	// timer vanilla mas largo (carne seca) son 8 dias y cualquier cosa perecedera ya se
	// pudrio mucho antes. Evita que un sello de tiempo raro (JSON viejo, reloj del host
	// movido, borde de mes) le pegue un salto absurdo.
	protected const int EXOR_FRIDGE_MAX_ENVEJ_MIN = 10080;

	// Le cobra a la comida REAL del cargo los 'minutos' que la nevera estuvo sin energia.
	void ExorEnvejecerComida(int minutos)
	{
		if (minutos <= 0)
			return;
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return;

		float segundos = minutos * 60.0;
		int envejecidos = 0;
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			Edible_Base food = Edible_Base.Cast(cargo.GetItem(i));
			// GUARD: un Edible SIN food stage tira NULL pointer en la pudricion vanilla.
			// (crash de produccion del 20-jul: una Pot en la nevera).
			if (!food || !food.GetFoodStage())
				continue;
			food.ExorEnvejecer(segundos);
			envejecidos++;
		}
		if (envejecidos > 0)
			Print(string.Format("%1 nevera %2: %3 comida(s) envejecidas %4 min (tiempo sin bateria)", ExorStorageConstants.LOG, ExorGetID(), envejecidos, minutos));
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
