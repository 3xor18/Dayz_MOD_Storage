// ============================================================================
//  GUARD GENERICO DE CRAFTEO
// ----------------------------------------------------------------------------
//  Ninguna receta -de vanilla o de cualquier mod- puede CONSUMIR un contenedor 3xor: ni
//  agujerearlo para asador, ni cortarlo con la sierra, ni convertirlo en otra cosa.
//
//  EL PROBLEMA TECNICO
//  Nuestros contenedores heredan de las bases vanilla (Barrel_ColorBase, SeaChest,
//  Container_Base) para que el motor los trate como contenedores de verdad. El precio es
//  que cualquier receta que pida "un barril" tambien los acepta. Y una receta BORRA sus
//  ingredientes: el barril desaparece del mundo pero su JSON de virtualizacion queda
//  huerfano en el disco, con todo el loot adentro y sin nadie que lo reclame. Para el
//  jugador es loot evaporado sin ninguna explicacion.
//
//  POR QUE VA EN CheckRecipe Y NO EN CanDo
//  De las 221 recetas de vanilla, 206 hacen su propio 'override CanDo' y muchas no
//  llaman a super. Un 'modded class RecipeBase' que pise CanDo no correria en casi
//  ninguna receta: por eso el guard viejo tuvo que modear PokeHolesBarrel en concreto, y
//  por eso no cubria las recetas de otros mods. CheckRecipe es el chequeo de validez
//  general y NO lo pisa ninguna receta (0 de 221). Enganchado ahi, el bloqueo vale para
//  toda receta presente o futura de cualquier mod sin perseguirlas una por una.
//
//  ALCANCE: TODAS las recetas, no solo las que lo destruyen
//  El barril del mod es un mueble de storage y nada mas, asi que tampoco se usa de balde:
//  ni echarle agua, ni purificar con tabletas, ni lavar trapos, ni cargar la motosierra.
//  Se podia distinguir a las que transforman (marcan m_IngredientDestroy y declaran
//  AddResult) de las que solo mueven liquido, pero no es lo que se quiere.
//
//  OJO: esto solo cubre las RECETAS. Echarle agua a un barril tambien se puede por accion
//  de inventario, sin pasar por ninguna receta, y por eso al probar "el agua igual entraba"
//  aunque el guard estuviera puesto. Esa via se cierra en el config, no aca:
//  liquidContainerType = 0 en Exor_Barrel_Base, Exor_KothCrate_Base y Exor_Cofre_Base hace
//  que el motor deje de considerarlos recipientes de liquido. Las dos cosas juntas son las
//  que dejan el barril como storage puro.
//
//  Devolver false hace que la receta NO SE OFREZCA, no que falle a mitad de camino: el
//  jugador no llega a ver la accion, en vez de verla y perder el barril al usarla.
//
//  Esto no toca nada nuestro: el mod no define recetas propias, y el empaque con
//  destornillador es una ACCION (ActionContinuousBase), no una receta.
// ============================================================================
modded class RecipeBase
{
	override bool CheckRecipe(ItemBase item1, ItemBase item2, PlayerBase player)
	{
		if (ExorEsContenedorPropio(item1) || ExorEsContenedorPropio(item2))
			return false;
		return super.CheckRecipe(item1, item2, player);
	}

	// Se decide por PREFIJO del classname y no por IsInherited: varios contenedores
	// nuestros (el cajon del KOTH, los cofres) existen solo en config y del lado del
	// script son la clase vanilla pelada, asi que no hay tipo propio contra el cual
	// preguntar. El prefijo ademas cubre solo lo que agreguemos despues.
	static bool ExorEsContenedorPropio(ItemBase it)
	{
		if (!it)
			return false;
		string tipo = it.GetType();
		if (tipo.IndexOf("Exor_") != 0)
			return false;

		// EXCEPCION: la ropa 3xor. No es un contenedor ni guarda nada en disco -es una
		// retextura de una prenda vanilla-, asi que no hay loot que perder y puede entrar
		// en las recetas normales de ropa.
		// Si la comprobacion fallara, el resultado es bloquear, que es el lado seguro.
		if (it.IsInherited(Clothing))
			return false;

		return true;
	}
}
