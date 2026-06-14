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
		units[] = {"Exor_Barrel_500", "Exor_Barrel_500_Packed"};
		weapons[] = {};
		requiredVersion = 0.1;
		// VERIFICAR: DZ_Gear_Camping aporta TerritoryFlag/TerritoryFlagKit (base del mastil)
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Gear_Containers", "DZ_Weapons_Ammunition", "DZ_Gear_Camping"};
	};
};

class CfgMods
{
	class ExorStorage
	{
		dir = "3xor_Vanilla_Optimization";
		name = "3xor_Vanilla_Optimization";
		author = "3xor";
		version = "0.2.0";
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
