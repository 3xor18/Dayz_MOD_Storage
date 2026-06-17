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
		units[] = {"Exor_Barrel_500", "Exor_Barrel_500_Packed", "Exor_BodyBag"};
		weapons[] = {};
		requiredVersion = 0.1;
		// DZ_Gear_Camping = TerritoryFlag/Kit + SeaChest. DZ_Characters_Backpacks =
		// GhillieSuit vanilla. DZ_Characters = modelo del cuerpo (bolsa de cadaver).
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Gear_Containers", "DZ_Weapons_Ammunition", "DZ_Gear_Camping", "DZ_Characters_Backpacks", "DZ_Characters"};
	};
};

class CfgMods
{
	class ExorStorage
	{
		dir = "3xor_Vanilla_Optimization";
		name = "3xor_Vanilla_Optimization";
		author = "3xor";
		version = "0.3.0";
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
	class Inventory_Base;	// externa (DZ_Data)

	// ------------------------------------------------------------------
	//  Ghillies VANILLA al slot de la BANDA DEL BRAZO (Armband).
	//  Truco: Armband es un slot VANILLA (ya tiene proxy en el modelo -> renderiza)
	//  y casi nadie lo usa. Al mover el ghillie del slot Back -> Armband, el slot
	//  Back queda libre -> podes llevar bolso y ghillie a la vez.
	//  Se moddean las VARIANTES vanilla directamente (solo inventorySlot, NO el
	//  model) para no romper el modelo del suit. Costo: no usas banda de brazo real.
	// ------------------------------------------------------------------
	class GhillieSuit_ColorBase;	// base externa (DZ_Characters_Backpacks)
	class GhillieSuit_Mossy: GhillieSuit_ColorBase
	{
		inventorySlot[] = {"Armband"};
	};
	class GhillieSuit_Woodland: GhillieSuit_ColorBase
	{
		inventorySlot[] = {"Armband"};
	};
	class GhillieSuit_Winter: GhillieSuit_ColorBase
	{
		inventorySlot[] = {"Armband"};
	};
	class GhillieSuit_Tan: GhillieSuit_ColorBase
	{
		inventorySlot[] = {"Armband"};
	};

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
			itemsCargoSize[] = {7, 5};	// chico: arma caida / items sueltos
			openable = 0;
		};
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
