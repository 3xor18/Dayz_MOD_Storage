// ============================================================================
// 3xor_Vanilla_Optimization - config.cpp
// Optimizacion vanilla del server: storage virtualizado (barril 3xor),
// cobertura de vehiculos inactivos y stacks de municion.
// Modelo del barril: 55galDrum vanilla retexturizado via camoGround.
// ============================================================================

class CfgPatches
{
	class ExorStorage
	{
		units[] = {"Exor_Barrel_500", "Exor_Barrel_500_Packed", "Exor_OpenableStorage", "Exor_Fridge", "Exor_Refrigerador_Packed", "Exor_BodyBag", "Exor_KothCrate_1", "Exor_KothCrate_2", "Exor_KothCrate_3", "Exor_Cofre_Azul_Packed", "Exor_Cofre_Verde_Packed", "Exor_Cofre_Rojo_Packed", "Exor_Cofre_Azul", "Exor_Cofre_Verde", "Exor_Cofre_Rojo", "Exor_CofreLight"};
		weapons[] = {};
		requiredVersion = 0.1;
		// DZ_Gear_Camping = TerritoryFlag/Kit + SeaChest. DZ_Characters_Backpacks =
		// GhillieSuit vanilla. DZ_Characters = modelo del cuerpo (bolsa de cadaver).
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Gear_Containers", "DZ_Weapons_Ammunition", "DZ_Gear_Camping", "DZ_Characters_Backpacks", "DZ_Characters", "DZ_Gear_Consumables"};
	};
};

class CfgMods
{
	class ExorStorage
	{
		dir = "3xor_Vanilla_Optimization";
		name = "3xor_Vanilla_Optimization";
		author = "3xor";
		version = "0.4.0";
		type = "mod";
		dependencies[] = {"Game", "World", "Mission"};
		class defs
		{
			class gameScriptModule
			{
				value = "";
				files[] = {"ExorStorage/scripts/3_Game"};
			};
			class worldScriptModule
			{
				value = "";
				files[] = {"ExorStorage/scripts/4_World"};
			};
			class missionScriptModule
			{
				value = "";
				files[] = {"ExorStorage/scripts/5_Mission"};
			};
		};
	};
};

class CfgVehicles
{
	class Barrel_ColorBase;	// externa (DZ_Gear_Containers)
	class Container_Base;	// externa (DZ_Gear_Containers) - base del refrigerador openable
	class Inventory_Base;	// externa (DZ_Data)

	// ------------------------------------------------------------------
	//  Ghillies VANILLA re-slotteados (replica de @ghilliefix, el mod que el server
	//  del amigo usaba antes). Se cambia SOLO inventorySlot (NO el model) en cada
	//  variante vanilla, para no romper el modelo.
	//   - Cuerpo entero (GhillieSuit) y la capa/espalda (GhillieBushrag) -> BRAZALETE
	//     (Armband): slot vanilla con proxy (renderiza) y casi sin uso; deja el slot
	//     Back libre -> podes llevar bolso + ghillie a la vez.
	//   - Capucha de la cabeza (GhillieHood) -> CADERA/CINTURON (Hips).
	//  CLAVE: usar los MISMOS slots que @ghilliefix hace que los ghillies ya guardados
	//  de los players ENCAJEN al cambiar de mod -> NO se caen al piso al loguear.
	// ------------------------------------------------------------------
	class GhillieSuit_ColorBase;	// base externa (DZ_Characters_Backpacks)
	class GhillieSuit_Mossy: GhillieSuit_ColorBase { inventorySlot[] = {"Armband"}; };
	class GhillieSuit_Woodland: GhillieSuit_ColorBase { inventorySlot[] = {"Armband"}; };
	class GhillieSuit_Winter: GhillieSuit_ColorBase { inventorySlot[] = {"Armband"}; };
	class GhillieSuit_Tan: GhillieSuit_ColorBase { inventorySlot[] = {"Armband"}; };

	class GhillieBushrag_ColorBase;	// la "capa" / solo espalda
	class GhillieBushrag_Mossy: GhillieBushrag_ColorBase { inventorySlot[] = {"Armband"}; };
	class GhillieBushrag_Woodland: GhillieBushrag_ColorBase { inventorySlot[] = {"Armband"}; };
	class GhillieBushrag_Winter: GhillieBushrag_ColorBase { inventorySlot[] = {"Armband"}; };
	class GhillieBushrag_Tan: GhillieBushrag_ColorBase { inventorySlot[] = {"Armband"}; };

	class GhillieHood_ColorBase;	// la capucha de la cabeza -> cintura (Hips)
	class GhillieHood_Mossy: GhillieHood_ColorBase { inventorySlot[] = {"Hips"}; };
	class GhillieHood_Woodland: GhillieHood_ColorBase { inventorySlot[] = {"Hips"}; };
	class GhillieHood_Winter: GhillieHood_ColorBase { inventorySlot[] = {"Hips"}; };
	class GhillieHood_Tan: GhillieHood_ColorBase { inventorySlot[] = {"Hips"}; };

	// ------------------------------------------------------------------
	// Barril 3xor desplegado (funcional)
	// ------------------------------------------------------------------
	class Exor_Barrel_Base: Barrel_ColorBase
	{
		scope = 0;
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		hiddenSelections[] = {"camoGround"};
	};

	class Exor_Barrel_500: Exor_Barrel_Base
	{
		scope = 2;
		displayName = "3xor Barrel 500";
		descriptionShort = "Barril de almacenamiento 3xor de 500 slots. Vacio y cerrado se puede empaquetar para transportarlo facilmente.";
		color = "Camo";
		hiddenSelectionsTextures[] = {"ExorStorage\data\exor_barrel_500_co.paa"};
		class Cargo
		{
			itemsCargoSize[] = {10, 50};	// 500 slots
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};

	// ------------------------------------------------------------------
	// Barril empaquetado (transportable, sin cargo)
	// ------------------------------------------------------------------
	class Exor_Barrel_Packed_Base: Inventory_Base
	{
		scope = 0;
		model = "\dz\gear\containers\55galDrum.p3d";
		hiddenSelections[] = {"camoGround"};
		rotationFlags = 17;
		itemSize[] = {5, 5};
		weight = 10000;
		itemBehaviour = 0;
	};

	class Exor_Barrel_500_Packed: Exor_Barrel_Packed_Base
	{
		scope = 2;
		displayName = "3xor Barrel 500 (empaquetado)";
		descriptionShort = "Barril 3xor de 500 slots empaquetado. Tenelo en las manos y usa 'Desplegar barril' para colocarlo.";
		hiddenSelectionsTextures[] = {"ExorStorage\data\exor_barrel_500_co.paa"};
	};

	// ==================================================================
	//  REFRIGERADOR retro (МОСКВА) - mueble de guardado SOLO comida/bebida/agua.
	//  Hereda la maquinaria del barril 3xor (virtualizacion, empaque, abrir/cerrar
	//  con animacion de puerta, indestructible, anti-candado). El filtro comida,
	//  la puerta animada y la logica de BATERIA van en ExorStorage_Fridge.c.
	//
	//  BATERIA: slot de CarBattery. Con bateria cargada -> conserva comida + luz
	//  (+ la bateria se descarga con el tiempo). Sin bateria -> abre/cierra igual
	//  pero NO conserva (la comida se pudre normal) y sin luz.
	// ==================================================================
	// ------------------------------------------------------------------
	//  MUEBLE ABRIBLE + VIRTUALIZABLE (base reutilizable). Container_Base con
	//  abrir/cerrar limpio (MMG) + virtualizacion (barril) + colision solida.
	//  Los muebles (nevera y futuros) heredan de aca. Ver ExorStorage_Openable.c.
	// ------------------------------------------------------------------
	class Exor_OpenableStorage: Container_Base
	{
		scope = 0;
		rotationFlags = 17;
		// COLISION SOLIDA (como MMG): physLayer "item_large" + peso -> el jugador NO
		// lo atraviesa. La FORMA de colision sale de la caja del Geometry LOD del modelo.
		weight = 45000;
		itemBehaviour = 0;			// mueble pesado (no se lleva en la mano)
		physLayer = "item_large";
		itemIsOpenable = 1;
		carveNavmesh = 1;
		// Indestructible por codigo (SetAllowDamage(false)); DamageSystem minimo
		// para que el motor no se queje de health.
		class DamageSystem
		{
			class GlobalHealth
			{
				class Health
				{
					hitpoints = 5000;
					healthLevels[] =
					{
						{1.0, {}},
						{0.0, {}}
					};
				};
			};
		};
	};

	class Exor_Fridge: Exor_OpenableStorage
	{
		scope = 2;
		displayName = "Refrigerador";
		descriptionShort = "Guarda comida, bebida y agua. Con una bateria de coche puesta (dura ~3 dias) conserva la comida y no se pudre. Vacio y cerrado se empaqueta con un destornillador.";
		model = "\ExorStorage\data\models\fridge\fridge.p3d";
		hiddenSelections[] = {};
		// Slot de BATERIA DE COCHE (aparece en la UI del contenedor).
		attachments[] = {"CarBattery"};
		class Cargo
		{
			itemsCargoSize[] = {10, 12};	// 120 slots
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
		// ANIMACION PUERTA: source "Lid" controlable por codigo. El model.cfg liga
		// este source a la rotacion de la seleccion "lid" sobre "lid_axis".
		class AnimationSources
		{
			class Lid
			{
				source = "user";
				initPhase = 0;		// arranca CERRADA
				animPeriod = 0.5;	// 0.5s de giro suave
			};
		};
	};

	// Refrigerador EMPAQUETADO: se coloca con HOLOGRAMA (placement vanilla). El
	// holograma usa el modelo del empacado -> mostramos la nevera real.
	class Exor_Refrigerador_Packed: Exor_Barrel_Packed_Base
	{
		scope = 2;
		displayName = "Refrigerador";
		descriptionShort = "Setealo en tu base y guarda la comida. Necesita una bateria de coche para conservar.";
		// Modelo del empacado/HOLOGRAMA = fridge_packed.p3d (CON punto bbox invisible abajo)
		// para que el holograma de colocacion NO se vea enterrado (el user pidio "subirlo").
		// TRADE-OFF: el empacado-parado (tras "Empaquetar") puede flotar (el pack usa
		// PLACE_ON_SURFACE=bounding). El desplegado usa fridge.p3d (sin bbox) y apoya bien.
		// FIX REAL pendiente: modelo "caja" (mueble_empacado.glb) como empacado + codigo
		// de holograma que muestre el refri (necesita Blender para GLB->FBX).
		model = "\ExorStorage\data\models\fridge\fridge_packed.p3d";
		hiddenSelections[] = {};
		// Holograma de colocacion (igual que el barril vanilla): sin esto el ghost
		// puede quedar SIEMPRE rojo y no dejar colocar.
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		slopeTolerance = 0.2;
		yawPitchRollLimit[] = {45, 45, 45};
		carveNavmesh = 1;
	};

	// ------------------------------------------------------------------
	//  Bolsa de cadaver: al morir, el cuerpo se convierte en este contenedor
	//  con todo el loot. Hereda de SeaChest (carriable en manos + cargo +
	//  persistencia). El loot/virtualizacion/TTL los maneja ExorBodyBag.c.
	//  INTENTO de modelo = cuerpo del player. Si queda invisible o en T-pose,
	//  BORRAR la linea 'model' -> usa el modelo del SeaChest (visible, funcional).
	// ------------------------------------------------------------------
	class SeaChest;	// externa (DZ_Gear_Camping) - contenedor base carriable
	class Exor_BodyBag: SeaChest
	{
		scope = 2;
		displayName = "Cuerpo";
		descriptionShort = "Cuerpo de un jugador caído. Tiene sus pertenencias en los slots del equipo. No se puede mover.";
		// Modelo = lápida vanilla (tumba). OJO: tiene colision solida -> puede
		// bloquear el paso en pasillos/puertas (pendiente: desactivar colision).
		model = "\DZ\structures\Specific\Cemeteries\Cemetery_Tombstone1.p3d";
		weight = 35000;
		rotationFlags = 17;
		// SLOTS DEL EQUIPO DEL PLAYER: al lootear la bolsa se ve igual que un
		// cadaver (chaleco/mochila/bolsillos con sus items, cada uno en su slot).
		// Al morir, la ropa puesta se recrea en estos slots (ExorBodyBag.c).
		attachments[] = {"Headgear", "Mask", "Eyewear", "Gloves", "Armband", "Vest", "Body", "Hips", "Back", "Legs", "Feet", "Shoulder", "Melee"};
		class Cargo
		{
			// Grande (300) para que SIEMPRE entre todo el loot del muerto + el sobrante
			// que la red de seguridad reubica aca en vez de tirarlo al piso (ExorVO_Serializer).
			// Antes {7,5}=35 -> con un jugador full equipado se desbordaba y caia loot.
			itemsCargoSize[] = {10, 30};	// 300 slots
			openable = 0;
		};
	};

	// ------------------------------------------------------------------
	//  KOTH - pallet de recompensa. Basado en Barrel_ColorBase (contenedor LOOTABLE
	//  que se apoya DERECHO en el piso, igual que los barriles del mod) con el MODELO
	//  del supply crate del airfield. Cargo 1500; el manager corta al llenarse.
	//  openable=0: cargo siempre accesible (sin animacion de tapa).
	// ------------------------------------------------------------------
	class Exor_KothCrate_Base: Barrel_ColorBase
	{
		scope = 0;
		class Cargo
		{
			// ancho 10 (normal, como los barriles) x alto 100 = 1000 slots. NO usar un
			// ancho grande (ej 30): la grilla se va "para el costado" y los items quedan
			// fuera de la vista (parece que no spawnearon aunque si estan).
			itemsCargoSize[] = {10, 100};
			openable = 0;
		};
	};
	class Exor_KothCrate_1: Exor_KothCrate_Base
	{
		scope = 2;
		displayName = "Supply Crate (KOTH)";
		descriptionShort = "Recompensa del KOTH. Se autodestruye pasados los minutos configurados.";
		model = "DZ\structures\Military\Misc\Misc_SupplyBox1.p3d";
	};
	class Exor_KothCrate_2: Exor_KothCrate_Base
	{
		scope = 2;
		displayName = "Supply Crate (KOTH)";
		descriptionShort = "Recompensa del KOTH. Se autodestruye pasados los minutos configurados.";
		model = "DZ\structures\Military\Misc\Misc_SupplyBox2.p3d";
	};
	class Exor_KothCrate_3: Exor_KothCrate_Base
	{
		scope = 2;
		displayName = "Supply Crate (KOTH)";
		descriptionShort = "Recompensa del KOTH. Se autodestruye pasados los minutos configurados.";
		model = "DZ\structures\Military\Misc\Misc_SupplyBox3.p3d";
	};

	// ------------------------------------------------------------------
	//  COFRE - modulo de cofres de recompensa por zonas/horario (ExorCofre.c).
	//  Dos formas por color:
	//   - *_Packed: CAJA CERRADA (item normal del mod, carriable, 5x4, sin cargo).
	//     El admin la spawnea / va en types.xml. Modelo = caja de papel CERRADA.
	//     Al soltarla en una zona activa, tras N min se transforma en el cofre abierto.
	//   - (sin sufijo): COFRE ABIERTO, contenedor de 1000 slots {10,100} (como el
	//     KothCrate: ancho 10 para que NO se salga de la pantalla). Modelo = caja ABIERTA.
	//     Se rellena con un bundle aleatorio de la tabla de loot del color (cofre.json).
	// ------------------------------------------------------------------
	// -- cajas cerradas (item carriable) --
	class Exor_Cofre_Packed_Base: Inventory_Base
	{
		scope = 0;
		model = "\DZ\structures\Furniture\Cases\PaperBox\PaperBox_01_small_closed.p3d";
		rotationFlags = 17;
		itemSize[] = {5, 4};	// 20 casillas en la mochila
		weight = 8000;
		itemBehaviour = 0;
	};
	class Exor_Cofre_Azul_Packed: Exor_Cofre_Packed_Base
	{
		scope = 2;
		displayName = "Cofre Azul (cerrado)";
		descriptionShort = "Debes llevar esta caja a la mesa del evento apertura de cajas y dropearla en el suelo, se coloca sola sobre la mesa, esperar unos minutos y se abre sola y puedes tomar los items dentro";
	};
	class Exor_Cofre_Verde_Packed: Exor_Cofre_Packed_Base
	{
		scope = 2;
		displayName = "Cofre Verde (cerrado)";
		descriptionShort = "Debes llevar esta caja a la mesa del evento apertura de cajas y dropearla en el suelo, se coloca sola sobre la mesa, esperar unos minutos y se abre sola y puedes tomar los items dentro";
	};
	class Exor_Cofre_Rojo_Packed: Exor_Cofre_Packed_Base
	{
		scope = 2;
		displayName = "Cofre Rojo (cerrado)";
		descriptionShort = "Debes llevar esta caja a la mesa del evento apertura de cajas y dropearla en el suelo, se coloca sola sobre la mesa, esperar unos minutos y se abre sola y puedes tomar los items dentro";
	};

	// -- cofres abiertos (contenedor de 1000 slots) --
	class Exor_Cofre_Base: Barrel_ColorBase
	{
		scope = 0;
		model = "\DZ\structures\Furniture\Cases\PaperBox\PaperBox_01_small_open.p3d";
		class Cargo
		{
			// ancho 10 x alto 13 = 130 slots. NO ampliar el ancho: la grilla se va "para
			// el costado" y los items quedan fuera de la vista (igual que el KothCrate).
			itemsCargoSize[] = {10, 13};
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
	};
	class Exor_Cofre_Azul: Exor_Cofre_Base
	{
		scope = 2;
		displayName = "Cofre Azul";
		descriptionShort = "Cofre de recompensa AZUL abierto. Fijo en el suelo.";
	};
	class Exor_Cofre_Verde: Exor_Cofre_Base
	{
		scope = 2;
		displayName = "Cofre Verde";
		descriptionShort = "Cofre de recompensa VERDE abierto. Fijo en el suelo.";
	};
	class Exor_Cofre_Rojo: Exor_Cofre_Base
	{
		scope = 2;
		displayName = "Cofre Rojo";
		descriptionShort = "Cofre de recompensa ROJO abierto. Fijo en el suelo.";
	};

	// -- luz del evento: Roadflare ENCENDIDA debajo de la mesa (NO agarrable) --
	//  El manager pone 1 (cantidad_luces) debajo de la mesa, encendida; el manager la
	//  re-enciende/re-spawnea periodicamente (una roadflare encendida se autoconsume).
	//  Fija (no se puede tomar), se despawnea al cerrar el evento.
	class Roadflare;	// externa (DZ_Gear_Consumables)
	class Exor_CofreLight: Roadflare
	{
		scope = 2;
		displayName = "Luz del evento";
		descriptionShort = "Luz del evento de cofres. Fija, no se puede tomar.";
	};

	// --------------------------------------------------------------------
	// FASE 1 - Inventario ampliado de vehiculos vanilla: cargo a 600 {10,60}.
	// Override del Cargo (padre = CarScript). Las variantes de color heredan.
	// OJO: el tamano de cargo es de BUILD, NO se togglea por JSON.
	// --------------------------------------------------------------------
	class CarScript;	// base externa de los autos
	class OffroadHatchback: CarScript	// Olga 24
	{
		class Cargo { itemsCargoSize[] = {10, 60}; };
	};
	class CivilianSedan: CarScript		// Sedan (viejo)
	{
		class Cargo { itemsCargoSize[] = {10, 60}; };
	};
	class Hatchback_02: CarScript		// Gunter 2
	{
		class Cargo { itemsCargoSize[] = {10, 60}; };
	};
	class Sedan_02: CarScript
	{
		class Cargo { itemsCargoSize[] = {10, 60}; };
	};
	class Offroad_02: CarScript
	{
		class Cargo { itemsCargoSize[] = {10, 60}; };
	};
	class Truck_01_Base: CarScript {};	// fwd (padre del truck cubierto)
	class Truck_01_Covered: Truck_01_Base
	{
		class Cargo { itemsCargoSize[] = {10, 60}; };
	};

};

// ============================================================================
// Stacks de municion: SOLO balas sueltas, cartuchos, bolts y flechas.
// EXCLUIDOS a proposito: bengalas, 40mm, RPG/LAW, granadas (quedan vanilla).
// El numero es fijo en config (limitacion del motor: 'count' no se puede
// cambiar por JSON en runtime); editar aca y recompilar para cambiarlo.
// ============================================================================
class CfgMagazines
{
	class Ammunition_Base;

	class Ammo_45ACP: Ammunition_Base { count = 100; };
	class Ammo_308Win: Ammunition_Base { count = 100; };
	class Ammo_308WinTracer: Ammunition_Base { count = 100; };
	class Ammo_9x19: Ammunition_Base { count = 100; };
	class Ammo_380: Ammunition_Base { count = 100; };
	class Ammo_556x45: Ammunition_Base { count = 100; };
	class Ammo_556x45Tracer: Ammunition_Base { count = 100; };
	class Ammo_762x54: Ammunition_Base { count = 100; };
	class Ammo_762x54Tracer: Ammunition_Base { count = 100; };
	class Ammo_762x39: Ammunition_Base { count = 100; };
	class Ammo_762x39Tracer: Ammunition_Base { count = 100; };
	class Ammo_9x39AP: Ammunition_Base { count = 100; };
	class Ammo_9x39: Ammunition_Base { count = 100; };
	class Ammo_22: Ammunition_Base { count = 100; };
	class Ammo_12gaPellets: Ammunition_Base { count = 100; };
	class Ammo_12gaSlug: Ammunition_Base { count = 100; };
	class Ammo_12gaRubberSlug: Ammunition_Base { count = 100; };
	class Ammo_12gaBeanbag: Ammunition_Base { count = 100; };
	class Ammo_357: Ammunition_Base { count = 100; };
	class Ammo_545x39: Ammunition_Base { count = 100; };
	class Ammo_545x39Tracer: Ammunition_Base { count = 100; };
	class Ammo_HuntingBolt: Ammunition_Base { count = 100; };
	class Ammo_ImprovisedBolt_1: Ammunition_Base { count = 100; };
	class Ammo_ImprovisedBolt_2: Ammunition_Base { count = 100; };
	class Ammo_CupidsBolt: Ammunition_Base { count = 100; };
};
