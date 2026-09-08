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

		// EXCEPCION: las RETEXTURAS 3xor (ropa y armas de color). No son contenedores ni
		// guardan nada en disco -son la misma prenda/arma vanilla pintada de otro color-,
		// asi que no hay loot que perder y tienen que comportarse EXACTAMENTE igual que su
		// original: repararse con el kit de limpieza (receta CleanWeapon, ingrediente
		// "DefaultWeapon") o con la masilla epoxi (RepairEpoxy, ingrediente
		// "Inventory_Base", que es la que cubre culatas y guardamanos).
		//
		// La ropa se reconoce por herencia. Las armas y sus partes NO: del lado del script
		// una culata es un Inventory_Base pelado, igual que medio juego, asi que no hay tipo
		// propio contra el cual preguntar y van por prefijo de classname.
		//
		// OJO - AL AGREGAR UNA RETEXTURA NUEVA hay que sumar su prefijo abajo. Si se olvida,
		// el sintoma es "esta arma no se puede reparar con el kit": la receta directamente no
		// se ofrece. El default sigue siendo BLOQUEAR porque es el lado seguro -olvidarse de
		// exceptuar una retextura molesta; olvidarse de bloquear un contenedor evapora loot-.
		if (it.IsInherited(Clothing))
			return false;
		if (ExorEsRetexturaDeArma(tipo))
			return false;

		return true;
	}

	// Las 20 armas de color (Exor_M4A1_/AKM_/Aug_/M14_/SV98_ + Rosa|Azul|Dorado|Camo) y sus
	// 48 culatas y guardamanos (Exor_M4_*, Exor_AK_*). Ningun contenedor 3xor empieza con
	// estos prefijos: los muebles son Exor_Barrel_, Exor_Locker, Exor_LockerRojo,
	// Exor_MuebleArmas, Exor_Fridge, Exor_Refrigerador_, Exor_Cofre, Exor_KothCrate_,
	// Exor_Parking, Exor_BodyBag y Exor_CarCodeLock.
	static bool ExorEsRetexturaDeArma(string tipo)
	{
		if (tipo.IndexOf("Exor_M4A1_") == 0) return true;
		if (tipo.IndexOf("Exor_AKM_")  == 0) return true;
		if (tipo.IndexOf("Exor_Aug_")  == 0) return true;
		if (tipo.IndexOf("Exor_M14_")  == 0) return true;
		if (tipo.IndexOf("Exor_SV98_") == 0) return true;
		if (tipo.IndexOf("Exor_M4_")   == 0) return true;
		if (tipo.IndexOf("Exor_AK_")   == 0) return true;
		return false;
	}
}
