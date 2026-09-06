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
		units[] = {"Exor_Barrel_500", "Exor_Barrel_500_Packed", "Exor_OpenableStorage", "Exor_Fridge", "Exor_Refrigerador_Packed", "Exor_Refrigerador_Ghost", "Exor_Locker", "Exor_Locker_Packed", "Exor_Locker_Ghost", "Exor_LockerRojo", "Exor_LockerRojo_Packed", "Exor_LockerRojo_Ghost", "Exor_MuebleArmas", "Exor_MuebleArmas_Packed", "Exor_BodyBag", "Exor_KothCrate_1", "Exor_KothCrate_2", "Exor_KothCrate_3", "Exor_Cofre_Azul_Packed", "Exor_Cofre_Verde_Packed", "Exor_Cofre_Rojo_Packed", "Exor_Cofre_Azul", "Exor_Cofre_Verde", "Exor_Cofre_Rojo", "Exor_CofreLight", "Exor_Parking", "Exor_Parking_Packed", "Exor_Parking_Ghost", "Exor_GorkaJacket_Rosa", "Exor_GorkaPants_Rosa", "Exor_BallisticHelmet_Rosa", "Exor_Mich2001Helmet_Rosa", "Exor_GorkaHelmet_Rosa", "Exor_BalaclavaMask_Rosa", "Exor_CombatBoots_Rosa", "Exor_TacticalGloves_Rosa", "Exor_PressVest_Rosa", "Exor_PlateCarrierVest_Rosa", "Exor_PlateCarrierHolster_Rosa", "Exor_PlateCarrierPouches_Rosa", "Exor_TortillaBag_Rosa", "Exor_GorkaJacket_Arido", "Exor_GorkaPants_Arido", "Exor_BallisticHelmet_Arido", "Exor_Mich2001Helmet_Arido", "Exor_GorkaHelmet_Arido", "Exor_BalaclavaMask_Arido", "Exor_CombatBoots_Arido", "Exor_TacticalGloves_Arido", "Exor_PressVest_Arido", "Exor_PlateCarrierVest_Arido", "Exor_PlateCarrierHolster_Arido", "Exor_PlateCarrierPouches_Arido", "Exor_TortillaBag_Arido", "Exor_GorkaJacket_Urbano", "Exor_GorkaPants_Urbano", "Exor_BallisticHelmet_Urbano", "Exor_Mich2001Helmet_Urbano", "Exor_GorkaHelmet_Urbano", "Exor_BalaclavaMask_Urbano", "Exor_CombatBoots_Urbano", "Exor_TacticalGloves_Urbano", "Exor_PressVest_Urbano", "Exor_PlateCarrierVest_Urbano", "Exor_PlateCarrierHolster_Urbano", "Exor_PlateCarrierPouches_Urbano", "Exor_TortillaBag_Urbano", "Exor_GorkaJacket_Nieve", "Exor_GorkaPants_Nieve", "Exor_BallisticHelmet_Nieve", "Exor_Mich2001Helmet_Nieve", "Exor_GorkaHelmet_Nieve", "Exor_BalaclavaMask_Nieve", "Exor_CombatBoots_Nieve", "Exor_TacticalGloves_Nieve", "Exor_PressVest_Nieve", "Exor_PlateCarrierVest_Nieve", "Exor_PlateCarrierHolster_Nieve", "Exor_PlateCarrierPouches_Nieve", "Exor_TortillaBag_Nieve", "Exor_GorkaJacket_Negro", "Exor_GorkaPants_Negro", "Exor_BallisticHelmet_Negro", "Exor_Mich2001Helmet_Negro", "Exor_GorkaHelmet_Negro", "Exor_BalaclavaMask_Negro", "Exor_CombatBoots_Negro", "Exor_TacticalGloves_Negro", "Exor_PressVest_Negro", "Exor_PlateCarrierVest_Negro", "Exor_PlateCarrierHolster_Negro", "Exor_PlateCarrierPouches_Negro", "Exor_TortillaBag_Negro"};
		weapons[] = {};
		requiredVersion = 0.1;
		// DZ_Gear_Camping = TerritoryFlag/Kit + SeaChest. DZ_Characters_Backpacks =
		// GhillieSuit vanilla. DZ_Characters = modelo del cuerpo (bolsa de cadaver).
		// Los DZ_Characters_* de abajo son los que traen las bases de los sets de ropa
		// retexturizados (tops/pants/headgear/shoes/gloves/vests): sin ellos las clases
		// nuevas no encuentran su padre y el juego las descarta en silencio.
		requiredAddons[] = {"DZ_Data", "DZ_Scripts", "DZ_Gear_Containers", "DZ_Weapons_Ammunition", "DZ_Gear_Camping", "DZ_Characters_Backpacks", "DZ_Characters", "DZ_Gear_Consumables", "DZ_Structures_Furniture", "DZ_Characters_Tops", "DZ_Characters_Pants", "DZ_Characters_Headgear", "DZ_Characters_Shoes", "DZ_Characters_Gloves", "DZ_Characters_Vests", "DZ_Characters_Masks"};
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

	// >>> SETS DE ROPA 3xor (generado por tools/gen_ropa_config.py) >>>
	// ==================================================================
	//  SETS DE ROPA 3xor (retexturizados)
	// ------------------------------------------------------------------
	//  Cinco colores x trece piezas. Son ITEMS NUEVOS: cada clase HEREDA de la base vanilla
	//  y solo cambia 'hiddenSelectionsTextures'. Las bases se declaran sin cuerpo (forward
	//  declaration), que NO modifica la clase vanilla: los items originales del juego quedan
	//  intactos, igual que los modelos, que se reusan tal cual. No hay ni un 'modded class'.
	//
	//  Las texturas son el _co (color) vanilla recoloreado en HSV conservando la luminancia
	//  -o sea costuras, correas, sombras y desgaste-. El _nohq (relieve) y el _smdi (brillo)
	//  NO se tocan: siguen siendo los de vanilla, heredados. Ver tools/recolor_ropa.py, que
	//  documenta cada paleta y por que esta donde esta.
	//
	//  ESTE BLOQUE ES GENERADO. Para agregar un color o una pieza, editar las tablas de
	//  tools/gen_ropa_config.py y correrlo; reescribe entre los marcadores >>> y <<<.
	//  Para volver atras: borrar el bloque entero, sus entradas en CfgPatches.units, la
	//  carpeta data\ropa y las lineas de types.xml. Nada mas depende de esto.
	// ==================================================================

	class Clothing;				// externa (DZ_Data)
	class GorkaEJacket_ColorBase;	// externa (DZ_Characters_Tops)
	class GorkaPants_ColorBase;	// externa (DZ_Characters_Pants)
	class BallisticHelmet_ColorBase;	// externa (DZ_Characters_Headgear)
	class Mich2001Helmet;	// externa (DZ_Characters_Headgear)
	class GorkaHelmet;	// externa (DZ_Characters_Headgear)
	class BalaclavaMask_ColorBase;	// externa (DZ_Characters_Masks)
	class CombatBoots_ColorBase;	// externa (DZ_Characters_Shoes)
	class TacticalGloves_ColorBase;	// externa (DZ_Characters_Gloves)
	class PressVest_ColorBase;	// externa (DZ_Characters_Vests)
	class PlateCarrierVest;	// externa (DZ_Characters_Vests)
	class PlateCarrierHolster;	// externa (DZ_Characters_Vests)
	class PlateCarrierPouches;	// externa (DZ_Gear_Containers)
	class TortillaBag;	// externa (DZ_Characters_Backpacks)

	// ---------------- SET ROSA ----------------

	class Exor_GorkaJacket_Rosa: GorkaEJacket_ColorBase
	{
		scope = 2;
		displayName = "Camisa Gorka Rosa";
		descriptionShort = "Camisa Gorka del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_jacket_ground_co.paa",
			"ExorStorage\data\ropa\exor_rosa_jacket_worn_co.paa",
			"ExorStorage\data\ropa\exor_rosa_jacket_worn_co.paa"
		};
	};

	class Exor_GorkaPants_Rosa: GorkaPants_ColorBase
	{
		scope = 2;
		displayName = "Pantalon Gorka Rosa";
		descriptionShort = "Pantalon Gorka del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_pants_ground_co.paa",
			"ExorStorage\data\ropa\exor_rosa_pants_worn_co.paa",
			"ExorStorage\data\ropa\exor_rosa_pants_worn_co.paa"
		};
	};

	class Exor_BallisticHelmet_Rosa: BallisticHelmet_ColorBase
	{
		scope = 2;
		displayName = "Casco balistico Rosa";
		descriptionShort = "Casco balistico del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_helmet_co.paa",
			"ExorStorage\data\ropa\exor_rosa_helmet_co.paa",
			"ExorStorage\data\ropa\exor_rosa_helmet_co.paa"
		};
	};

	class Exor_Mich2001Helmet_Rosa: Mich2001Helmet
	{
		scope = 2;
		displayName = "Casco MICH 2001 Rosa";
		descriptionShort = "Casco MICH 2001 del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_mich_co.paa",
			"ExorStorage\data\ropa\exor_rosa_mich_co.paa",
			"ExorStorage\data\ropa\exor_rosa_mich_co.paa"
		};
	};

	class Exor_GorkaHelmet_Rosa: GorkaHelmet
	{
		scope = 2;
		displayName = "Casco Gorka Rosa";
		descriptionShort = "Casco Gorka del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_rosa_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_rosa_gorkahelmet_co.paa"
		};
	};

	class Exor_BalaclavaMask_Rosa: BalaclavaMask_ColorBase
	{
		scope = 2;
		displayName = "Balaclava Rosa";
		descriptionShort = "Balaclava del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_rosa_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_rosa_balaclava_co.paa"
		};
	};

	class Exor_CombatBoots_Rosa: CombatBoots_ColorBase
	{
		scope = 2;
		displayName = "Botas de combate Rosa";
		descriptionShort = "Botas de combate del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_boots_co.paa",
			"ExorStorage\data\ropa\exor_rosa_boots_co.paa",
			"ExorStorage\data\ropa\exor_rosa_boots_co.paa"
		};
	};

	class Exor_TacticalGloves_Rosa: TacticalGloves_ColorBase
	{
		scope = 2;
		displayName = "Guantes tacticos Rosa";
		descriptionShort = "Guantes tacticos del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_gloves_co.paa",
			"ExorStorage\data\ropa\exor_rosa_gloves_co.paa",
			"ExorStorage\data\ropa\exor_rosa_gloves_co.paa"
		};
	};

	class Exor_PressVest_Rosa: PressVest_ColorBase
	{
		scope = 2;
		displayName = "Chaleco de prensa Rosa";
		descriptionShort = "Chaleco de prensa del set Rosa de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_press_co.paa",
			"ExorStorage\data\ropa\exor_rosa_press_co.paa",
			"ExorStorage\data\ropa\exor_rosa_press_co.paa"
		};
	};

	class Exor_PlateCarrierVest_Rosa: PlateCarrierVest
	{
		scope = 2;
		displayName = "Chaleco balistico Rosa";
		descriptionShort = "Chaleco balistico del set Rosa de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa",
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa",
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa"
		};
	};

	class Exor_PlateCarrierHolster_Rosa: PlateCarrierHolster
	{
		scope = 2;
		displayName = "Pistolera de chaleco Rosa";
		descriptionShort = "Pistolera de chaleco del set Rosa de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa",
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa",
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa"
		};
	};

	class Exor_PlateCarrierPouches_Rosa: PlateCarrierPouches
	{
		scope = 2;
		displayName = "Bolsillos de chaleco Rosa";
		descriptionShort = "Bolsillos de chaleco del set Rosa de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa",
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa",
			"ExorStorage\data\ropa\exor_rosa_plate_co.paa"
		};
	};

	class Exor_TortillaBag_Rosa: TortillaBag
	{
		scope = 2;
		displayName = "Mochila tactica Rosa";
		descriptionShort = "Mochila tactica del set Rosa de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_rosa_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_rosa_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_rosa_tortilla_co.paa"
		};
		itemsCargoSize[] = {10, 12};
		attachments[] += {"Shoulder", "Melee"};
	};

	// ---------------- SET ARIDO ----------------

	class Exor_GorkaJacket_Arido: GorkaEJacket_ColorBase
	{
		scope = 2;
		displayName = "Camisa Gorka Arido";
		descriptionShort = "Camisa Gorka del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_jacket_ground_co.paa",
			"ExorStorage\data\ropa\exor_arido_jacket_worn_co.paa",
			"ExorStorage\data\ropa\exor_arido_jacket_worn_co.paa"
		};
	};

	class Exor_GorkaPants_Arido: GorkaPants_ColorBase
	{
		scope = 2;
		displayName = "Pantalon Gorka Arido";
		descriptionShort = "Pantalon Gorka del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_pants_ground_co.paa",
			"ExorStorage\data\ropa\exor_arido_pants_worn_co.paa",
			"ExorStorage\data\ropa\exor_arido_pants_worn_co.paa"
		};
	};

	class Exor_BallisticHelmet_Arido: BallisticHelmet_ColorBase
	{
		scope = 2;
		displayName = "Casco balistico Arido";
		descriptionShort = "Casco balistico del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_helmet_co.paa",
			"ExorStorage\data\ropa\exor_arido_helmet_co.paa",
			"ExorStorage\data\ropa\exor_arido_helmet_co.paa"
		};
	};

	class Exor_Mich2001Helmet_Arido: Mich2001Helmet
	{
		scope = 2;
		displayName = "Casco MICH 2001 Arido";
		descriptionShort = "Casco MICH 2001 del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_mich_co.paa",
			"ExorStorage\data\ropa\exor_arido_mich_co.paa",
			"ExorStorage\data\ropa\exor_arido_mich_co.paa"
		};
	};

	class Exor_GorkaHelmet_Arido: GorkaHelmet
	{
		scope = 2;
		displayName = "Casco Gorka Arido";
		descriptionShort = "Casco Gorka del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_arido_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_arido_gorkahelmet_co.paa"
		};
	};

	class Exor_BalaclavaMask_Arido: BalaclavaMask_ColorBase
	{
		scope = 2;
		displayName = "Balaclava Arido";
		descriptionShort = "Balaclava del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_arido_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_arido_balaclava_co.paa"
		};
	};

	class Exor_CombatBoots_Arido: CombatBoots_ColorBase
	{
		scope = 2;
		displayName = "Botas de combate Arido";
		descriptionShort = "Botas de combate del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_boots_co.paa",
			"ExorStorage\data\ropa\exor_arido_boots_co.paa",
			"ExorStorage\data\ropa\exor_arido_boots_co.paa"
		};
	};

	class Exor_TacticalGloves_Arido: TacticalGloves_ColorBase
	{
		scope = 2;
		displayName = "Guantes tacticos Arido";
		descriptionShort = "Guantes tacticos del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_gloves_co.paa",
			"ExorStorage\data\ropa\exor_arido_gloves_co.paa",
			"ExorStorage\data\ropa\exor_arido_gloves_co.paa"
		};
	};

	class Exor_PressVest_Arido: PressVest_ColorBase
	{
		scope = 2;
		displayName = "Chaleco de prensa Arido";
		descriptionShort = "Chaleco de prensa del set Arido de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_press_co.paa",
			"ExorStorage\data\ropa\exor_arido_press_co.paa",
			"ExorStorage\data\ropa\exor_arido_press_co.paa"
		};
	};

	class Exor_PlateCarrierVest_Arido: PlateCarrierVest
	{
		scope = 2;
		displayName = "Chaleco balistico Arido";
		descriptionShort = "Chaleco balistico del set Arido de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_plate_co.paa",
			"ExorStorage\data\ropa\exor_arido_plate_co.paa",
			"ExorStorage\data\ropa\exor_arido_plate_co.paa"
		};
	};

	class Exor_PlateCarrierHolster_Arido: PlateCarrierHolster
	{
		scope = 2;
		displayName = "Pistolera de chaleco Arido";
		descriptionShort = "Pistolera de chaleco del set Arido de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_plate_co.paa",
			"ExorStorage\data\ropa\exor_arido_plate_co.paa",
			"ExorStorage\data\ropa\exor_arido_plate_co.paa"
		};
	};

	class Exor_PlateCarrierPouches_Arido: PlateCarrierPouches
	{
		scope = 2;
		displayName = "Bolsillos de chaleco Arido";
		descriptionShort = "Bolsillos de chaleco del set Arido de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_plate_co.paa",
			"ExorStorage\data\ropa\exor_arido_plate_co.paa",
			"ExorStorage\data\ropa\exor_arido_plate_co.paa"
		};
	};

	class Exor_TortillaBag_Arido: TortillaBag
	{
		scope = 2;
		displayName = "Mochila tactica Arido";
		descriptionShort = "Mochila tactica del set Arido de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_arido_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_arido_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_arido_tortilla_co.paa"
		};
		itemsCargoSize[] = {10, 12};
		attachments[] += {"Shoulder", "Melee"};
	};

	// ---------------- SET URBANO ----------------

	class Exor_GorkaJacket_Urbano: GorkaEJacket_ColorBase
	{
		scope = 2;
		displayName = "Camisa Gorka Urbano";
		descriptionShort = "Camisa Gorka del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_jacket_ground_co.paa",
			"ExorStorage\data\ropa\exor_urbano_jacket_worn_co.paa",
			"ExorStorage\data\ropa\exor_urbano_jacket_worn_co.paa"
		};
	};

	class Exor_GorkaPants_Urbano: GorkaPants_ColorBase
	{
		scope = 2;
		displayName = "Pantalon Gorka Urbano";
		descriptionShort = "Pantalon Gorka del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_pants_ground_co.paa",
			"ExorStorage\data\ropa\exor_urbano_pants_worn_co.paa",
			"ExorStorage\data\ropa\exor_urbano_pants_worn_co.paa"
		};
	};

	class Exor_BallisticHelmet_Urbano: BallisticHelmet_ColorBase
	{
		scope = 2;
		displayName = "Casco balistico Urbano";
		descriptionShort = "Casco balistico del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_helmet_co.paa",
			"ExorStorage\data\ropa\exor_urbano_helmet_co.paa",
			"ExorStorage\data\ropa\exor_urbano_helmet_co.paa"
		};
	};

	class Exor_Mich2001Helmet_Urbano: Mich2001Helmet
	{
		scope = 2;
		displayName = "Casco MICH 2001 Urbano";
		descriptionShort = "Casco MICH 2001 del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_mich_co.paa",
			"ExorStorage\data\ropa\exor_urbano_mich_co.paa",
			"ExorStorage\data\ropa\exor_urbano_mich_co.paa"
		};
	};

	class Exor_GorkaHelmet_Urbano: GorkaHelmet
	{
		scope = 2;
		displayName = "Casco Gorka Urbano";
		descriptionShort = "Casco Gorka del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_urbano_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_urbano_gorkahelmet_co.paa"
		};
	};

	class Exor_BalaclavaMask_Urbano: BalaclavaMask_ColorBase
	{
		scope = 2;
		displayName = "Balaclava Urbano";
		descriptionShort = "Balaclava del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_urbano_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_urbano_balaclava_co.paa"
		};
	};

	class Exor_CombatBoots_Urbano: CombatBoots_ColorBase
	{
		scope = 2;
		displayName = "Botas de combate Urbano";
		descriptionShort = "Botas de combate del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_boots_co.paa",
			"ExorStorage\data\ropa\exor_urbano_boots_co.paa",
			"ExorStorage\data\ropa\exor_urbano_boots_co.paa"
		};
	};

	class Exor_TacticalGloves_Urbano: TacticalGloves_ColorBase
	{
		scope = 2;
		displayName = "Guantes tacticos Urbano";
		descriptionShort = "Guantes tacticos del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_gloves_co.paa",
			"ExorStorage\data\ropa\exor_urbano_gloves_co.paa",
			"ExorStorage\data\ropa\exor_urbano_gloves_co.paa"
		};
	};

	class Exor_PressVest_Urbano: PressVest_ColorBase
	{
		scope = 2;
		displayName = "Chaleco de prensa Urbano";
		descriptionShort = "Chaleco de prensa del set Urbano de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_press_co.paa",
			"ExorStorage\data\ropa\exor_urbano_press_co.paa",
			"ExorStorage\data\ropa\exor_urbano_press_co.paa"
		};
	};

	class Exor_PlateCarrierVest_Urbano: PlateCarrierVest
	{
		scope = 2;
		displayName = "Chaleco balistico Urbano";
		descriptionShort = "Chaleco balistico del set Urbano de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa",
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa",
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa"
		};
	};

	class Exor_PlateCarrierHolster_Urbano: PlateCarrierHolster
	{
		scope = 2;
		displayName = "Pistolera de chaleco Urbano";
		descriptionShort = "Pistolera de chaleco del set Urbano de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa",
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa",
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa"
		};
	};

	class Exor_PlateCarrierPouches_Urbano: PlateCarrierPouches
	{
		scope = 2;
		displayName = "Bolsillos de chaleco Urbano";
		descriptionShort = "Bolsillos de chaleco del set Urbano de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa",
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa",
			"ExorStorage\data\ropa\exor_urbano_plate_co.paa"
		};
	};

	class Exor_TortillaBag_Urbano: TortillaBag
	{
		scope = 2;
		displayName = "Mochila tactica Urbano";
		descriptionShort = "Mochila tactica del set Urbano de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_urbano_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_urbano_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_urbano_tortilla_co.paa"
		};
		itemsCargoSize[] = {10, 12};
		attachments[] += {"Shoulder", "Melee"};
	};

	// ---------------- SET NIEVE ----------------

	class Exor_GorkaJacket_Nieve: GorkaEJacket_ColorBase
	{
		scope = 2;
		displayName = "Camisa Gorka Nieve";
		descriptionShort = "Camisa Gorka del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_jacket_ground_co.paa",
			"ExorStorage\data\ropa\exor_nieve_jacket_worn_co.paa",
			"ExorStorage\data\ropa\exor_nieve_jacket_worn_co.paa"
		};
	};

	class Exor_GorkaPants_Nieve: GorkaPants_ColorBase
	{
		scope = 2;
		displayName = "Pantalon Gorka Nieve";
		descriptionShort = "Pantalon Gorka del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_pants_ground_co.paa",
			"ExorStorage\data\ropa\exor_nieve_pants_worn_co.paa",
			"ExorStorage\data\ropa\exor_nieve_pants_worn_co.paa"
		};
	};

	class Exor_BallisticHelmet_Nieve: BallisticHelmet_ColorBase
	{
		scope = 2;
		displayName = "Casco balistico Nieve";
		descriptionShort = "Casco balistico del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_helmet_co.paa",
			"ExorStorage\data\ropa\exor_nieve_helmet_co.paa",
			"ExorStorage\data\ropa\exor_nieve_helmet_co.paa"
		};
	};

	class Exor_Mich2001Helmet_Nieve: Mich2001Helmet
	{
		scope = 2;
		displayName = "Casco MICH 2001 Nieve";
		descriptionShort = "Casco MICH 2001 del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_mich_co.paa",
			"ExorStorage\data\ropa\exor_nieve_mich_co.paa",
			"ExorStorage\data\ropa\exor_nieve_mich_co.paa"
		};
	};

	class Exor_GorkaHelmet_Nieve: GorkaHelmet
	{
		scope = 2;
		displayName = "Casco Gorka Nieve";
		descriptionShort = "Casco Gorka del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_nieve_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_nieve_gorkahelmet_co.paa"
		};
	};

	class Exor_BalaclavaMask_Nieve: BalaclavaMask_ColorBase
	{
		scope = 2;
		displayName = "Balaclava Nieve";
		descriptionShort = "Balaclava del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_nieve_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_nieve_balaclava_co.paa"
		};
	};

	class Exor_CombatBoots_Nieve: CombatBoots_ColorBase
	{
		scope = 2;
		displayName = "Botas de combate Nieve";
		descriptionShort = "Botas de combate del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_boots_co.paa",
			"ExorStorage\data\ropa\exor_nieve_boots_co.paa",
			"ExorStorage\data\ropa\exor_nieve_boots_co.paa"
		};
	};

	class Exor_TacticalGloves_Nieve: TacticalGloves_ColorBase
	{
		scope = 2;
		displayName = "Guantes tacticos Nieve";
		descriptionShort = "Guantes tacticos del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_gloves_co.paa",
			"ExorStorage\data\ropa\exor_nieve_gloves_co.paa",
			"ExorStorage\data\ropa\exor_nieve_gloves_co.paa"
		};
	};

	class Exor_PressVest_Nieve: PressVest_ColorBase
	{
		scope = 2;
		displayName = "Chaleco de prensa Nieve";
		descriptionShort = "Chaleco de prensa del set Nieve de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_press_co.paa",
			"ExorStorage\data\ropa\exor_nieve_press_co.paa",
			"ExorStorage\data\ropa\exor_nieve_press_co.paa"
		};
	};

	class Exor_PlateCarrierVest_Nieve: PlateCarrierVest
	{
		scope = 2;
		displayName = "Chaleco balistico Nieve";
		descriptionShort = "Chaleco balistico del set Nieve de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa",
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa",
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa"
		};
	};

	class Exor_PlateCarrierHolster_Nieve: PlateCarrierHolster
	{
		scope = 2;
		displayName = "Pistolera de chaleco Nieve";
		descriptionShort = "Pistolera de chaleco del set Nieve de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa",
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa",
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa"
		};
	};

	class Exor_PlateCarrierPouches_Nieve: PlateCarrierPouches
	{
		scope = 2;
		displayName = "Bolsillos de chaleco Nieve";
		descriptionShort = "Bolsillos de chaleco del set Nieve de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa",
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa",
			"ExorStorage\data\ropa\exor_nieve_plate_co.paa"
		};
	};

	class Exor_TortillaBag_Nieve: TortillaBag
	{
		scope = 2;
		displayName = "Mochila tactica Nieve";
		descriptionShort = "Mochila tactica del set Nieve de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_nieve_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_nieve_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_nieve_tortilla_co.paa"
		};
		itemsCargoSize[] = {10, 12};
		attachments[] += {"Shoulder", "Melee"};
	};

	// ---------------- SET NEGRO ----------------

	class Exor_GorkaJacket_Negro: GorkaEJacket_ColorBase
	{
		scope = 2;
		displayName = "Camisa Gorka Negro";
		descriptionShort = "Camisa Gorka del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_jacket_ground_co.paa",
			"ExorStorage\data\ropa\exor_negro_jacket_worn_co.paa",
			"ExorStorage\data\ropa\exor_negro_jacket_worn_co.paa"
		};
	};

	class Exor_GorkaPants_Negro: GorkaPants_ColorBase
	{
		scope = 2;
		displayName = "Pantalon Gorka Negro";
		descriptionShort = "Pantalon Gorka del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_pants_ground_co.paa",
			"ExorStorage\data\ropa\exor_negro_pants_worn_co.paa",
			"ExorStorage\data\ropa\exor_negro_pants_worn_co.paa"
		};
	};

	class Exor_BallisticHelmet_Negro: BallisticHelmet_ColorBase
	{
		scope = 2;
		displayName = "Casco balistico Negro";
		descriptionShort = "Casco balistico del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_helmet_co.paa",
			"ExorStorage\data\ropa\exor_negro_helmet_co.paa",
			"ExorStorage\data\ropa\exor_negro_helmet_co.paa"
		};
	};

	class Exor_Mich2001Helmet_Negro: Mich2001Helmet
	{
		scope = 2;
		displayName = "Casco MICH 2001 Negro";
		descriptionShort = "Casco MICH 2001 del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_mich_co.paa",
			"ExorStorage\data\ropa\exor_negro_mich_co.paa",
			"ExorStorage\data\ropa\exor_negro_mich_co.paa"
		};
	};

	class Exor_GorkaHelmet_Negro: GorkaHelmet
	{
		scope = 2;
		displayName = "Casco Gorka Negro";
		descriptionShort = "Casco Gorka del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_negro_gorkahelmet_co.paa",
			"ExorStorage\data\ropa\exor_negro_gorkahelmet_co.paa"
		};
	};

	class Exor_BalaclavaMask_Negro: BalaclavaMask_ColorBase
	{
		scope = 2;
		displayName = "Balaclava Negro";
		descriptionShort = "Balaclava del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_negro_balaclava_co.paa",
			"ExorStorage\data\ropa\exor_negro_balaclava_co.paa"
		};
	};

	class Exor_CombatBoots_Negro: CombatBoots_ColorBase
	{
		scope = 2;
		displayName = "Botas de combate Negro";
		descriptionShort = "Botas de combate del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_boots_co.paa",
			"ExorStorage\data\ropa\exor_negro_boots_co.paa",
			"ExorStorage\data\ropa\exor_negro_boots_co.paa"
		};
	};

	class Exor_TacticalGloves_Negro: TacticalGloves_ColorBase
	{
		scope = 2;
		displayName = "Guantes tacticos Negro";
		descriptionShort = "Guantes tacticos del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_gloves_co.paa",
			"ExorStorage\data\ropa\exor_negro_gloves_co.paa",
			"ExorStorage\data\ropa\exor_negro_gloves_co.paa"
		};
	};

	class Exor_PressVest_Negro: PressVest_ColorBase
	{
		scope = 2;
		displayName = "Chaleco de prensa Negro";
		descriptionShort = "Chaleco de prensa del set Negro de 3xor.";
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_press_co.paa",
			"ExorStorage\data\ropa\exor_negro_press_co.paa",
			"ExorStorage\data\ropa\exor_negro_press_co.paa"
		};
	};

	class Exor_PlateCarrierVest_Negro: PlateCarrierVest
	{
		scope = 2;
		displayName = "Chaleco balistico Negro";
		descriptionShort = "Chaleco balistico del set Negro de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_plate_co.paa",
			"ExorStorage\data\ropa\exor_negro_plate_co.paa",
			"ExorStorage\data\ropa\exor_negro_plate_co.paa"
		};
	};

	class Exor_PlateCarrierHolster_Negro: PlateCarrierHolster
	{
		scope = 2;
		displayName = "Pistolera de chaleco Negro";
		descriptionShort = "Pistolera de chaleco del set Negro de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_plate_co.paa",
			"ExorStorage\data\ropa\exor_negro_plate_co.paa",
			"ExorStorage\data\ropa\exor_negro_plate_co.paa"
		};
	};

	class Exor_PlateCarrierPouches_Negro: PlateCarrierPouches
	{
		scope = 2;
		displayName = "Bolsillos de chaleco Negro";
		descriptionShort = "Bolsillos de chaleco del set Negro de 3xor.";
		hiddenSelections[] = {"camoGround"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_plate_co.paa",
			"ExorStorage\data\ropa\exor_negro_plate_co.paa",
			"ExorStorage\data\ropa\exor_negro_plate_co.paa"
		};
	};

	class Exor_TortillaBag_Negro: TortillaBag
	{
		scope = 2;
		displayName = "Mochila tactica Negro";
		descriptionShort = "Mochila tactica del set Negro de 3xor.";
		hiddenSelections[] = {"camoGround", "camoMale", "camoFemale"};
		hiddenSelectionsTextures[] = {
			"ExorStorage\data\ropa\exor_negro_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_negro_tortilla_co.paa",
			"ExorStorage\data\ropa\exor_negro_tortilla_co.paa"
		};
		itemsCargoSize[] = {10, 12};
		attachments[] += {"Shoulder", "Melee"};
	};

	// <<< SETS DE ROPA 3xor <<<

	// ------------------------------------------------------------------
	// Barril 3xor desplegado (funcional)
	// ------------------------------------------------------------------
	class Exor_Barrel_Base: Barrel_ColorBase
	{
		scope = 0;
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		hiddenSelections[] = {"camoGround"};
		// NO es un balde: solo guarda loot. Barrel_ColorBase declara un liquidContainerType
		// con media mascara de bits puesta, y eso es lo que hace que el motor lo acepte como
		// recipiente de agua/gasolina -por accion de inventario, no solo por receta, que es
		// por lo que el guard de recetas no alcanzaba-. En 0 deja de ser recipiente y se
		// cierra esa via entera de una.
		liquidContainerType = 0;
		varQuantityMax = 0;
		quantityBar = 0;
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
	// PARKING: maquina estatica que se setea en la base. Al interactuar abre el menu de
	// autos del clan (virtualizar / desvirtualizar). Por ahora config MINIMO para spawnear
	// y verificar que el modelo se ve bien (no rosa). El comportamiento (colocacion en
	// territorio, permisos, menu, virtualizacion) se agrega en codigo despues.
	// DESPLEGADO (estatico en la base). Se crea al setear el _Packed. No se levanta.
	class Exor_Parking: Inventory_Base
	{
		scope = 2;
		displayName = "Parking";
		descriptionShort = "Máquina de parking: administra los autos de tu clan (virtualizar/desvirtualizar).";
		model = "\ExorStorage\data\models\parking\parking.p3d";
		weight = 45000;
		itemBehaviour = 0;			// pesado, estatico (no se lleva en la mano)
		physLayer = "item_large";	// colision solida: el jugador no lo atraviesa
		carveNavmesh = 0;
		class DamageSystem
		{
			class GlobalHealth
			{
				class Health
				{
					hitpoints = 5000;
					healthLevels[] = { {1.0, {}}, {0.0, {}} };
				};
			};
			class DamageZones {};
		};
	};

	// EMPACADO (item que se lleva en la mano; se setea con la accion Deploy). Modelo = caja
	// de carton como los otros muebles; el holograma del preview usa el modelo del parking.
	class Exor_Parking_Packed: Inventory_Base
	{
		scope = 2;
		displayName = "Parking";
		descriptionShort = "Máquina de parking: setealo en tu base para administrar los autos del clan. Guardalo con un destornillador.";
		model = "DZ\structures\furniture\Cases\PaperBox\PaperBox_01_small_closed.p3d";
		projectionTypename = "Exor_Parking_Ghost";	// holograma = modelo del parking
		rotationFlags = 17;
		itemSize[] = {5, 5};
		weight = 10000;
		itemBehaviour = 0;
		hiddenSelections[] = {};
	};

	// CANDADO DE AUTOS (keypad): item de inventario. Se lo lleva en la mano y se "Coloca
	// candado" mirando un auto del clan. Modelo propio (keypad de seguridad).
	class Exor_CarCodeLock: Inventory_Base
	{
		scope = 2;
		displayName = "Candado de Auto";
		descriptionShort = "Keypad para ponerle clave a un auto de tu clan. Tenelo en la mano y mirá el auto para colocarlo.";
		model = "\ExorStorage\data\models\carcodelock\keypad.p3d";
		rotationFlags = 17;
		itemSize[] = {2, 2};
		weight = 400;
		itemBehaviour = 0;
		hiddenSelections[] = {};
	};

	// HOLOGRAMA del preview (se ve el parking al apuntar donde colocarlo).
	// scope=1: usable como proyeccion por codigo pero NO aparece en el spawn de admin
	// (evita el "tercer item fantasma" en la lista).
	class Exor_Parking_Ghost: Inventory_Base
	{
		scope = 1;
		displayName = "Parking";
		model = "\ExorStorage\data\models\parking\parking.p3d";
		rotationFlags = 17;
		weight = 10000;
		itemBehaviour = 0;
		hiddenSelections[] = {};
	};

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
		// carveNavmesh = 0: cada objeto que lo activa obliga al motor a recortar la navmesh
		// en su footprint, al crearse Y al cargar la persistencia. Con el plan de pasar de
		// decenas a cientos de muebles, son cientos de recortes concentrados en pocas bases
		// mas un pathfinding de zombies mas pesado ahi adentro. Los barriles (657 en
		// produccion) NUNCA lo usaron y no dan problema.
		// Lo que se pierde: los zombies dejan de "saber" esquivar el mueble antes de tocarlo;
		// igual chocan por la colision solida (physLayer item_large). Si aparecen zombies
		// trabados contra muebles, volver a 1.
		carveNavmesh = 0;
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
			// DamageZones vacio pero PRESENTE: sin esta clase el motor escupe
			// "No entry '...Exor_OpenableStorage/DamageSystem.DamageZones'" por cada
			// mueble que la hereda. Es solo ruido (el mueble es indestructible por
			// codigo), pero lo heredan TODOS los muebles -> se multiplica.
			class DamageZones {};
		};
	};

	class Exor_Fridge: Exor_OpenableStorage
	{
		scope = 2;
		displayName = "Refrigerador";
		descriptionShort = "Guarda comida, bebida y agua. Con una bateria de coche puesta (dura ~3 dias) conserva la comida y no se pudre. Vacio y cerrado se empaqueta con un destornillador.";
		model = "\ExorStorage\data\models\fridge\fridge.p3d";
		hiddenSelections[] = {};
		// UN SOLO slot de bateria: se le mete UNA bateria, de AUTO o de CAMION (ambas
		// aceptan el slot "ExorBattery", patcheado en CfgVehicles mas abajo). La de
		// camion dura el DOBLE (ver ExorStorage_Fridge.c ExorPeriodicTick).
		attachments[] = {"ExorBattery"};
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
	// EMPAQUETADO estilo MMG: caja de herramientas en mano, holograma = refri (ver
	// Exor_LockerRojo_Packed para la explicacion del truco projectionTypename).
	class Exor_Refrigerador_Packed: Exor_Barrel_Packed_Base
	{
		scope = 2;
		displayName = "Refrigerador";
		descriptionShort = "Guarda comida, bebida y agua (necesita bateria de coche para conservar). Setealo en tu base. Guardalo con un destornillador.";
		model = "DZ\structures\furniture\Cases\PaperBox\PaperBox_01_small_closed.p3d";	// caja de carton (item en mano)
		projectionTypename = "Exor_Refrigerador_Ghost";	// holograma = modelo del refri
		hiddenSelections[] = {};
	};

	// ------------------------------------------------------------------
	//  BATERIAS vanilla re-slotteadas: se les AGREGA el slot "ExorBattery" (el slot
	//  unico de la nevera) SIN tocar sus slots vanilla. Asi una sola bocacha de la
	//  nevera acepta bateria de auto O de camion.
	//  MISMAS REGLAS CRITICAS que las armas (ver bloque CfgWeapons abajo):
	//   1) DECLARAR el padre en el merge (`: Inventory_Base` / `: CarBattery`), sino
	//      el motor resetea la herencia y CRASHEA.
	//   2) `+=` NO `=`: `=` borraria el slot vanilla ("CarBattery"/"TruckBattery") y
	//      la bateria ya no entraria en los autos. `+=` conserva lo vanilla y agrega.
	// ------------------------------------------------------------------
	class CarBattery: Inventory_Base
	{
		inventorySlot[] += {"ExorBattery"};
	};
	class TruckBattery: CarBattery
	{
		inventorySlot[] += {"ExorBattery"};
	};

	// HOLOGRAMA del refri (solo proyeccion, scope=0). model = fridge_packed.p3d (LOD visual
	// con punto bbox abajo -> holograma al ras). Props de holograma se leen de ESTA clase.
	class Exor_Refrigerador_Ghost: Exor_Barrel_Packed_Base
	{
		scope = 1;	// protected: la crea el holograma por codigo, pero no aparece en menus
		displayName = "Refrigerador";
		model = "\ExorStorage\data\models\fridge\fridge_packed.p3d";
		hiddenSelections[] = {};
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		slopeTolerance = 0.2;
		yawPitchRollLimit[] = {45, 45, 45};
	};

	// ------------------------------------------------------------------
	//  LOCKER de equipo (2 puertas). Guarda ropa/armas/gear (NO comida).
	//  Hereda de Exor_OpenableStorage. Ver ExorStorage_Locker.c.
	// ------------------------------------------------------------------
	class Exor_Locker: Exor_OpenableStorage
	{
		scope = 2;
		displayName = "Locker de Equipo";
		descriptionShort = "Armario de 2 puertas. Guarda ropa, armas y equipo (no comida). Vacio y cerrado se empaqueta con un destornillador.";
		model = "\ExorStorage\data\models\locker\locker.p3d";
		hiddenSelections[] = {};
		// SLOTS DE EQUIPO (como el cuerpo/tumba): el player puede COLGAR su ropa/gear
		// como attachments (casco, chaleco, mochila, pantalon, botas, etc.) ademas del
		// cargo. Solo accesibles con el locker ABIERTO (el openable bloquea el inventario
		// cerrado). Cada slot solo acepta su tipo (el motor filtra).
		// Slots de equipo + HILERA DE 8 ARMAS (Exor_Gun1..8, en CfgSlots; las armas los
		// aceptan via el patch CfgWeapons +=). Guardar armas aparte del cargo de cosas.
		attachments[] = {"Headgear", "Mask", "Eyewear", "Gloves", "Armband", "Vest", "Body", "Hips", "Back", "Legs", "Feet", "Shoulder", "Melee", "Exor_Gun1", "Exor_Gun2", "Exor_Gun3", "Exor_Gun4", "Exor_Gun5", "Exor_Gun6", "Exor_Gun7", "Exor_Gun8", "Exor_Gun9", "Exor_Gun10", "Exor_Gun11"};
		class Cargo
		{
			itemsCargoSize[] = {10, 50};	// 500 slots
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
		// DOS puertas: sources L_Door y R_Door (el model.cfg liga cada uno a su seleccion).
		class AnimationSources
		{
			class L_Door
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.5;
			};
			class R_Door
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.5;
			};
		};
	};

	// Locker EMPAQUETADO: se coloca con HOLOGRAMA. Usa locker_packed.p3d = SOLO el LOD
	// visual (sin Geometry LOD) -> el holograma NO tiene colision y no se cancela al colocar
	// (el deployado locker.p3d si tiene colision).
	// EMPAQUETADO estilo MMG: caja de herramientas en mano, holograma = locker (ver
	// Exor_LockerRojo_Packed para la explicacion del truco projectionTypename).
	class Exor_Locker_Packed: Exor_Barrel_Packed_Base
	{
		scope = 2;
		displayName = "Locker de Equipo";
		descriptionShort = "Armario de 2 puertas. Setealo en tu base para guardar ropa, armas y equipo. Guardalo con un destornillador.";
		model = "DZ\structures\furniture\Cases\PaperBox\PaperBox_01_small_closed.p3d";	// caja de carton (item en mano)
		projectionTypename = "Exor_Locker_Ghost";	// holograma = modelo del locker
		hiddenSelections[] = {};
	};

	// HOLOGRAMA del locker (solo proyeccion, scope=0). model = locker_packed.p3d (LOD visual).
	class Exor_Locker_Ghost: Exor_Barrel_Packed_Base
	{
		scope = 1;	// protected: la crea el holograma por codigo, pero no aparece en menus
		displayName = "Locker de Equipo";
		model = "\ExorStorage\data\models\locker\locker_packed.p3d";
		hiddenSelections[] = {};
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		slopeTolerance = 0.2;
		yawPitchRollLimit[] = {45, 45, 45};
	};

	// ------------------------------------------------------------------
	//  LOCKER ROJO (closet scifi de 2 puertas) - clon del locker de equipo con
	//  estetica distinta. ETAPA 2a: modelo estatico (sin AnimationSources aun).
	// ------------------------------------------------------------------
	class Exor_LockerRojo: Exor_OpenableStorage
	{
		scope = 2;
		displayName = "Locker Rojo";
		descriptionShort = "Armario scifi de 2 puertas. Guarda ropa, armas y equipo (no comida). Vacio y cerrado se empaqueta con un destornillador.";
		model = "\ExorStorage\data\models\locker_rojo\locker_rojo.p3d";
		hiddenSelections[] = {};
		// Slots de equipo + HILERA DE 8 ARMAS (Exor_Gun1..8, en CfgSlots; las armas los
		// aceptan via el patch CfgWeapons +=). Guardar armas aparte del cargo de cosas.
		attachments[] = {"Headgear", "Mask", "Eyewear", "Gloves", "Armband", "Vest", "Body", "Hips", "Back", "Legs", "Feet", "Shoulder", "Melee", "Exor_Gun1", "Exor_Gun2", "Exor_Gun3", "Exor_Gun4", "Exor_Gun5", "Exor_Gun6", "Exor_Gun7", "Exor_Gun8", "Exor_Gun9", "Exor_Gun10", "Exor_Gun11"};
		class Cargo
		{
			itemsCargoSize[] = {10, 50};	// 500 slots
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
		// Etapa 2b: DOS puertas animadas. sources L_Door/R_Door (el model.cfg liga cada
		// uno a su seleccion L_door/R_door). El script hace SetAnimationPhase al abrir/cerrar.
		class AnimationSources
		{
			class L_Door
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.5;
			};
			class R_Door
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.5;
			};
		};
	};

	// EMPAQUETADO estilo MMG: EN MANO/INVENTARIO se ve como una CAJA DE HERRAMIENTAS
	// (item chico), pero el HOLOGRAMA de colocacion muestra el MUEBLE. El truco es
	// projectionTypename: DayZ (Hologram.c) proyecta ESE tipo (el _Ghost con el modelo
	// visual del mueble) en vez del modelo del item. Nombre/descripcion = del mueble.
	class Exor_LockerRojo_Packed: Exor_Barrel_Packed_Base
	{
		scope = 2;
		displayName = "Locker Rojo";
		descriptionShort = "Armario scifi de 2 puertas. Setealo en tu base para guardar ropa, armas y equipo. Guardalo con un destornillador.";
		model = "DZ\structures\furniture\Cases\PaperBox\PaperBox_01_small_closed.p3d";	// caja de carton (item en mano)
		projectionTypename = "Exor_LockerRojo_Ghost";	// holograma = modelo del mueble
		hiddenSelections[] = {};
	};

	// HOLOGRAMA del Locker Rojo (solo proyeccion, scope=0). model = LOD visual del mueble
	// (sin geometry -> no se cancela al colocar). Las props de holograma se leen de ESTA clase
	// (m_Projection.GetType()), no del item empacado.
	class Exor_LockerRojo_Ghost: Exor_Barrel_Packed_Base
	{
		scope = 1;	// protected: la crea el holograma por codigo, pero no aparece en menus
		displayName = "Locker Rojo";
		model = "\ExorStorage\data\models\locker_rojo\locker_rojo_packed.p3d";
		hiddenSelections[] = {};
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		slopeTolerance = 0.2;
		yawPitchRollLimit[] = {45, 45, 45};
	};

	// ------------------------------------------------------------------
	//  MUEBLE DE ARMAS (gun cabinet): 1 puerta con ventana + 2 cajones, los
	//  tres animados. 300 slots de cargo. Mostrara las armas en el estante
	//  (proxies) - se agrega en la fase de proxies. Ver ExorStorage_GunCab.c.
	// ------------------------------------------------------------------
	class Exor_MuebleArmas: Exor_OpenableStorage
	{
		scope = 0;	// MUEBLE DE ARMAS DESACTIVADO POR AHORA (el display de armas queda pendiente
					// de hacerse bien con Buldozer - ver memoria). scope=0 = no spawnea/no aparece.
					// Reactivar = scope=2 (+ resolver el display en sesion dedicada).
		displayName = "Mueble de Armas";
		descriptionShort = "Vitrina para armas: puerta con ventana + 2 cajones. Guarda armas y equipo. Vacio y cerrado se empaqueta con un destornillador.";
		model = "\ExorStorage\data\models\guncab\guncab.p3d";
		hiddenSelections[] = {};
		hasProxiesToHide = 0;
		// 8 SLOTS DE ARMAS (como los slots de la tumba): el player cuelga sus armas.
		// Los slots Exor_Gun1..8 estan en CfgSlots; las armas (Rifle_Base/Pistol_Base)
		// aceptan estos slots via inventorySlot (patch en CfgWeapons). Solo accesibles
		// con el mueble ABIERTO (el openable oculta el inventario cerrado).
		attachments[] = {"Exor_Gun1", "Exor_Gun2", "Exor_Gun3", "Exor_Gun4", "Exor_Gun5", "Exor_Gun6", "Exor_Gun7", "Exor_Gun8"};
		class Cargo
		{
			itemsCargoSize[] = {15, 20};	// 300 slots
			openable = 0;
			allowOwnedCargoManipulation = 1;
		};
		// TRES partes moviles: puerta principal (rotacion) + 2 cajones (traslacion).
		class AnimationSources
		{
			class MainDoor
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.6;
			};
			class Drawer01
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.5;
			};
			class Drawer02
			{
				source = "user";
				initPhase = 0;
				animPeriod = 0.5;
			};
		};
	};

	class Exor_MuebleArmas_Packed: Exor_Barrel_Packed_Base
	{
		scope = 0;	// item empacado del mueble de armas: DESACTIVADO por ahora (ver Exor_MuebleArmas).
		displayName = "Mueble de Armas";
		descriptionShort = "Setealo en tu base para guardar y exhibir tus armas.";
		model = "\ExorStorage\data\models\guncab\guncab_packed.p3d";
		hiddenSelections[] = {};
		hologramMaterial = "barrel";
		hologramMaterialPath = "dz\gear\containers\data";
		slopeTolerance = 0.2;
		yawPitchRollLimit[] = {45, 45, 45};
		// sin carveNavmesh: esto es el item EMPACADO, se lleva en la mano. Recortar la
		// navmesh por un item que el jugador carga encima no tiene sentido.
		carveNavmesh = 0;
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
		// SLOTS DEL EQUIPO DEL PLAYER, y SOLO esos: al lootear la bolsa se ve igual que un
		// cadaver (chaleco/mochila/bolsillos con sus items, cada uno en su slot), incluidos
		// los slots de arma que ya tiene el personaje vanilla (Shoulder / Melee).
		// Al morir, la ropa puesta se recrea en estos slots (ExorBodyBag.c).
		// NO van los Exor_Gun1..11: esa hilera de armas es de los LOCKERS, no de la tumba.
		// Lo que no entra en un slot cae al Cargo de abajo, que es el comportamiento buscado.
		attachments[] = {"Headgear", "Mask", "Eyewear", "Gloves", "Armband", "Vest", "Body", "Hips", "Back", "Legs", "Feet", "Shoulder", "Melee"};
		class Cargo
		{
			// Grande (300) para que SIEMPRE entre todo el loot del muerto + el sobrante
			// que la red de seguridad reubica aca en vez de tirarlo al piso (ExorVO_Serializer).
			// Antes {7,5}=35 -> con un jugador full equipado se desbordaba y caia loot.
			itemsCargoSize[] = {10, 30};	// 300 slots
			openable = 0;
		};
		// Mismo caso que Exor_OpenableStorage: SeaChest (la clase padre, vanilla) tampoco
		// declara DamageZones, asi que el motor tira
		// "No entry '...SeaChest/DamageSystem.DamageZones'" cada vez que se crea una tumba.
		// Visto en el test local del 20-jul, una vez por muerte.
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
			class DamageZones {};
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
		// NO es un balde: solo guarda loot. Barrel_ColorBase declara un liquidContainerType
		// con media mascara de bits puesta, y eso es lo que hace que el motor lo acepte como
		// recipiente de agua/gasolina -por accion de inventario, no solo por receta, que es
		// por lo que el guard de recetas no alcanzaba-. En 0 deja de ser recipiente y se
		// cierra esa via entera de una.
		liquidContainerType = 0;
		varQuantityMax = 0;
		quantityBar = 0;
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
		// NO es un balde: solo guarda loot. Barrel_ColorBase declara un liquidContainerType
		// con media mascara de bits puesta, y eso es lo que hace que el motor lo acepte como
		// recipiente de agua/gasolina -por accion de inventario, no solo por receta, que es
		// por lo que el guard de recetas no alcanzaba-. En 0 deja de ser recipiente y se
		// cierra esa via entera de una.
		liquidContainerType = 0;
		varQuantityMax = 0;
		quantityBar = 0;
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

// ============================================================================
//  SLOTS DE ARMAS del Mueble de Armas (Exor_Gun1..8). Cada slot acepta cualquier
//  arma. Se muestran como attachments en la UI del mueble (solo abierto).
// ============================================================================
class CfgSlots
{
	class Slot_Exor_Gun1 { name = "Exor_Gun1"; displayName = "Arma 1"; ghostIcon = "missing"; selection = "Exor_Gun1"; };
	class Slot_Exor_Gun2 { name = "Exor_Gun2"; displayName = "Arma 2"; ghostIcon = "missing"; selection = "Exor_Gun2"; };
	class Slot_Exor_Gun3 { name = "Exor_Gun3"; displayName = "Arma 3"; ghostIcon = "missing"; selection = "Exor_Gun3"; };
	class Slot_Exor_Gun4 { name = "Exor_Gun4"; displayName = "Arma 4"; ghostIcon = "missing"; selection = "Exor_Gun4"; };
	class Slot_Exor_Gun5 { name = "Exor_Gun5"; displayName = "Arma 5"; ghostIcon = "missing"; selection = "Exor_Gun5"; };
	class Slot_Exor_Gun6 { name = "Exor_Gun6"; displayName = "Arma 6"; ghostIcon = "missing"; selection = "Exor_Gun6"; };
	class Slot_Exor_Gun7 { name = "Exor_Gun7"; displayName = "Arma 7"; ghostIcon = "missing"; selection = "Exor_Gun7"; };
	class Slot_Exor_Gun8 { name = "Exor_Gun8"; displayName = "Arma 8"; ghostIcon = "missing"; selection = "Exor_Gun8"; };
	class Slot_Exor_Gun9 { name = "Exor_Gun9"; displayName = "Arma 9"; ghostIcon = "missing"; selection = "Exor_Gun9"; };
	class Slot_Exor_Gun10 { name = "Exor_Gun10"; displayName = "Arma 10"; ghostIcon = "missing"; selection = "Exor_Gun10"; };
	class Slot_Exor_Gun11 { name = "Exor_Gun11"; displayName = "Arma 11"; ghostIcon = "missing"; selection = "Exor_Gun11"; };
	// UN SOLO slot de bateria para la nevera: acepta bateria de AUTO o de CAMION (ambas
	// se le agregan como inventorySlot mas abajo, en CfgVehicles). La de camion dura x2.
	class Slot_ExorBattery { name = "ExorBattery"; displayName = "Batería"; ghostIcon = "battery"; selection = "ExorBattery"; };
};

// Habilitar que TODAS las armas (rifles + pistolas) puedan colgarse en los 8 slots.
// REGLAS CRITICAS (ver crash+bug 18-jul):
//  1) DECLARAR la clase padre en el merge (`: RifleCore`), sino el motor resetea la
//     herencia y CRASHEA (RifleCore-> vacio).
//  2) USAR `+=` NO `=`: `=` BORRA los inventorySlot vanilla {Shoulder, Melee} del arma
//     -> el player NO puede ponerse el arma al hombro. `+=` conserva los del player y
//     solo AGREGA los slots del mueble. NUNCA romper mecanicas del personaje.
class CfgWeapons
{
	class RifleCore;
	class PistolCore;
	class Rifle_Base: RifleCore
	{
		inventorySlot[] += {"Exor_Gun1", "Exor_Gun2", "Exor_Gun3", "Exor_Gun4", "Exor_Gun5", "Exor_Gun6", "Exor_Gun7", "Exor_Gun8", "Exor_Gun9", "Exor_Gun10", "Exor_Gun11"};
	};
	class Pistol_Base: PistolCore
	{
		inventorySlot[] += {"Exor_Gun1", "Exor_Gun2", "Exor_Gun3", "Exor_Gun4", "Exor_Gun5", "Exor_Gun6", "Exor_Gun7", "Exor_Gun8", "Exor_Gun9", "Exor_Gun10", "Exor_Gun11"};
	};
};

// PROXIES de las armas del mueble: DESACTIVADO (mueble entero en scope=0 por ahora; el display
// se hace con Buldozer en sesion aparte - ver memoria weapon-proxy-display-mechanism).
/*
class CfgNonAIVehicles
{
	class ProxyAttachment;
	class Exor_GunProxy1: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun1"}; model = "\ExorStorage\data\proxies\exor_gunproxy1.p3d"; };
	class Exor_GunProxy2: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun2"}; model = "\ExorStorage\data\proxies\exor_gunproxy2.p3d"; };
	class Exor_GunProxy3: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun3"}; model = "\ExorStorage\data\proxies\exor_gunproxy3.p3d"; };
	class Exor_GunProxy4: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun4"}; model = "\ExorStorage\data\proxies\exor_gunproxy4.p3d"; };
	class Exor_GunProxy5: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun5"}; model = "\ExorStorage\data\proxies\exor_gunproxy5.p3d"; };
	class Exor_GunProxy6: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun6"}; model = "\ExorStorage\data\proxies\exor_gunproxy6.p3d"; };
	class Exor_GunProxy7: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun7"}; model = "\ExorStorage\data\proxies\exor_gunproxy7.p3d"; };
	class Exor_GunProxy8: ProxyAttachment { scope = 2; inventorySlot[] = {"Exor_Gun8"}; model = "\ExorStorage\data\proxies\exor_gunproxy8.p3d"; };
};
*/
