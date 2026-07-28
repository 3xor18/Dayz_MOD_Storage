// ============================================================================
// 3xor_Vanilla_Optimization - Guard de "hacer agujeros al barril"
//
// PROBLEMA REAL (logs del server, 26-27 jul, 3 veces):
//     Reason: failed to spawn entity BarrelHoles_Camo, make sure the classname
//             exists and item can be spawned
//     Class: 'PokeHolesBarrel'  ->  RecipeBase::SpawnItems
//
// La receta vanilla PokeHolesBarrel acepta CUALQUIER Barrel_ColorBase y arma el
// classname del resultado como "BarrelHoles_" + el valor de config "color" del
// barril de entrada (m_ResultInheritsColor[0] = 0). Nuestros contenedores heredan
// de Barrel_ColorBase para ser lootables, y el Exor_Barrel_500 declara
// color = "Camo" -> el resultado seria "BarrelHoles_Camo", que no existe.
//
// Y la receta destruye el ingrediente ANTES de crear el resultado
// (m_IngredientDestroy[0] = 1): el jugador se quedaba SIN el barril 3xor y sin
// nada a cambio. Un barril de 500 slots evaporado por acercarle un cuchillo.
//
// Ni siquiera queremos que ande "bien": convertir un contenedor 3xor en un barril
// agujereado vanilla lo sacaria del registro de virtualizacion y su JSON quedaria
// huerfano. La opcion directamente no se ofrece sobre nuestros contenedores.
// ============================================================================

modded class PokeHolesBarrel
{
	// Se llama al armar la lista de recetas posibles (RecipeBase::CheckRecipe), asi que
	// devolver false aca hace que la accion NO APAREZCA, no que falle a mitad de camino.
	override bool CanDo(ItemBase ingredients[], PlayerBase player)
	{
		ItemBase barril = ingredients[0];
		if (!barril)
			return super.CanDo(ingredients, player);

		string tipo = barril.GetType();

		// 1) contenedores 3xor (barril, cofre, cajon del KOTH): nunca.
		//    Por prefijo y no por IsInherited porque el Exor_KothCrate_Base existe solo en
		//    config -del lado del script es un Barrel_ColorBase pelado- y asi tambien queda
		//    cubierto cualquier contenedor 3xor que agreguemos despues.
		if (tipo.IndexOf("Exor_") == 0)
			return false;

		// 2) red generica: si el resultado que armaria la receta no existe, tampoco.
		//    Cubre barriles de OTROS mods con un color que vanilla no tiene: sin esto la
		//    receta se come el barril y no entrega nada (mismo bug, distinto dueño).
		string color = "";
		GetGame().ConfigGetText("CfgVehicles " + tipo + " color", color);
		if (!GetGame().ConfigIsExisting("CfgVehicles BarrelHoles_" + color))
			return false;

		return super.CanDo(ingredients, player);
	}
};
