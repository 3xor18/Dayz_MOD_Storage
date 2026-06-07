// ============================================================================
// 3xorStorage - config.cpp
// Barriles de almacenamiento empaquetables con virtualizacion de contenido.
// Modelo base: barril vanilla (55galDrum.p3d) retexturizado via camoGround.
// ============================================================================

class CfgPatches
{
	class ExorStorage
	{
		units[] = {"Exor_Barrel_500", "Exor_Barrel_500_Packed"};
		weapons[] = {};
		requiredVersion = 0.1;
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Gear_Containers"};
	};
};

class CfgMods
{
	class ExorStorage
	{
		dir = "3xorStorage";
		name = "3xorStorage";
		author = "3xor";
		version = "0.1.0";
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
	// Barriles desplegados (funcionales)
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
	// Barriles empaquetados (transportables, sin cargo)
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
