// ============================================================================
// 3xor_Vanilla_Optimization - Municion (Fase 3)
// - Cantidad aleatoria {min,max} al spawnear como loot (spawn_municion)
// - Auto-stack al recoger con la accion "Take" (auto_stack)
// El stack maximo (100) es fijo en config.cpp (CfgMagazines count).
// Alcance: SOLO Ammunition_Base (balas sueltas/bolts/flechas), menos
// los classnames de municion_excluida.
// ============================================================================

class ExorVO_Ammo
{
	static bool IsManagedAmmo(EntityAI e)
	{
		if (!e)
			return false;
		if (!e.IsInherited(Ammunition_Base))
			return false;
		ExorCfgMunicion settings = GetExorConfig().municion;
		if (settings.municion_excluida.Find(e.GetType()) != -1)
			return false;
		return true;
	}

	// Cantidad aleatoria al spawnear por el CE (loot del mapa)
	static void ApplySpawnQuantity(Magazine mag)
	{
		if (!GetGame().IsServer())
			return;
		if (!IsManagedAmmo(mag))
			return;

		ExorCfgMunicion settings = GetExorConfig().municion;
		int mn = settings.spawn_municion_min_default;
		int mx = settings.spawn_municion_max_default;
		ExorMunicionSpawnRango rango;
		if (settings.spawn_municion.Find(mag.GetType(), rango))
		{
			mn = rango.min;
			mx = rango.max;
		}
		if (mn <= 0 && mx <= 0)
			return;	// 0/0 = no tocar, queda vanilla
		if (mx < mn)
			mx = mn;

		int q = Math.RandomIntInclusive(mn, mx);
		int maxAmmo = mag.GetAmmoMax();
		if (q > maxAmmo)
			q = maxAmmo;
		if (q < 1)
			q = 1;
		mag.ServerSetAmmoCount(q);
	}

	// Esta el item en las manos? (las manos no se tocan: permite dividir
	// balas para darselas a otro jugador)
	static bool IsInHands(EntityAI e)
	{
		InventoryLocation loc = new InventoryLocation();
		if (e.GetInventory() && e.GetInventory().GetCurrentInventoryLocation(loc))
		{
			if (loc.GetType() == InventoryLocationType.HANDS)
				return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------
	//  AUTO-STACK COALESCIDO (un solo pase por jugador, no uno por pila recogida)
	// ------------------------------------------------------------------------------
	// ANTES: cada pila de balas que entraba al inventario programaba SU PROPIO AutoStack a
	// los 400 ms, y cada uno enumeraba el inventario ENTERO del jugador. Vaciar un locker
	// son decenas de pilas en pocos segundos -> decenas de enumeraciones completas por
	// jugador, todas mirando lo mismo. Con 70 jugadores looteando en un raid eso es puro
	// trabajo repetido en el hilo principal.
	//
	// AHORA: la primera pila agenda el pase y las demas se suben al que ya esta agendado
	// (patron de "trailing debounce"). Un solo pase consolida TODOS los tipos de una, con
	// UNA sola enumeracion del inventario.
	static void AutoStackAll(PlayerBase player)
	{
		if (!GetGame().IsServer() || !player)
			return;
		player.ExorSetStackPendiente(false);
		if (!ExorHotFlags.AutoStackMunicion())
			return;

		array<EntityAI> items = new array<EntityAI>;
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);

		// agrupar las pilas PARCIALES por tipo en una sola pasada
		map<string, ref array<Magazine>> porTipo = new map<string, ref array<Magazine>>;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Magazine m = Magazine.Cast(items.Get(i));
			if (!m || !IsManagedAmmo(m))
				continue;
			if (IsInHands(m))
				continue;
			int maxA = m.GetAmmoMax();
			int cnt = m.GetAmmoCount();
			// Solo participan las pilas PARCIALES. Las LLENAS no se pueden fusionar y mutarlas
			// solo genera manipulacion de inventario inutil (= mas ventana de desync).
			if (maxA <= 0 || cnt <= 0 || cnt >= maxA)
				continue;
			array<Magazine> lista;
			if (!porTipo.Find(m.GetType(), lista))
			{
				lista = new array<Magazine>;
				porTipo.Set(m.GetType(), lista);
			}
			lista.Insert(m);
		}

		int t;
		for (t = 0; t < porTipo.Count(); t++)
			ConsolidarTipo(porTipo.GetKey(t), porTipo.GetElement(t), player);
	}

	// Consolida las pilas parciales de UN tipo en la menor cantidad posible (ej 50+25+15 -> 90).
	static void ConsolidarTipo(string tipo, array<Magazine> pilas, PlayerBase player)
	{
		if (!pilas || pilas.Count() < 2)
			return;
		int maxA = pilas.Get(0).GetAmmoMax();
		if (maxA <= 0)
			return;

		int total = 0;
		int i;
		for (i = 0; i < pilas.Count(); i++)
			total = total + pilas.Get(i).GetAmmoCount();

		// Consolidar SOLO si de verdad REDUCE la cantidad de pilas. Si las parciales ya estan
		// en el minimo posible (ej 60+40 con max 80 = 2 pilas si o si), no se toca nada: evita
		// mutar el inventario sin ganancia (y su desync asociado).
		int minPiles = (total + maxA - 1) / maxA;
		if (minPiles >= pilas.Count())
			return;
		int antes = pilas.Count();

		// Contenedores padre afectados: hay que forzar su re-sincronizacion tras borrar o
		// reescribir pilas. Sin esto, si el jugador tiene el contenedor ABIERTO (chaleco,
		// mochila, o una tumba/barril que esta looteando), la vista del cliente queda vieja y
		// "las balas se desaparecen" hasta reloguear (el server siempre quedo correcto).
		array<EntityAI> padres = new array<EntityAI>;
		for (i = 0; i < pilas.Count(); i++)
		{
			EntityAI pad = pilas.Get(i).GetHierarchyParent();
			if (pad && padres.Find(pad) == -1)
				padres.Insert(pad);
		}

		for (i = 0; i < pilas.Count(); i++)
		{
			Magazine pila = pilas.Get(i);
			if (total <= 0)
			{
				GetGame().ObjectDelete(pila);
				continue;
			}
			int poner = total;
			if (poner > maxA)
				poner = maxA;
			pila.ServerSetAmmoCount(poner);
			pila.SetSynchDirty();
			total = total - poner;
		}

		for (i = 0; i < padres.Count(); i++)
		{
			if (padres.Get(i))
				padres.Get(i).SetSynchDirty();
		}
		player.SetSynchDirty();

		Print(string.Format("%1 Auto-stack: %2 consolidado %3 -> %4 pilas", ExorStorageConstants.LOG, tipo, antes, minPiles));
	}

	// Agenda UN pase por jugador. Las llamadas siguientes dentro de la ventana no agendan
	// nada: se suben a la que ya esta pendiente.
	static void AutoStackLater(Magazine mag, PlayerBase player)
	{
		if (!player || player.ExorStackPendiente())
			return;
		player.ExorSetStackPendiente(true);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorVO_Ammo.AutoStackAll, 400, false, player);
	}
}

// Cantidad aleatoria al spawnear por el CE + auto-stack al entrar al inventario
// (cubre Take con F, drag & drop y cualquier otra via de recogida)
modded class Ammunition_Base
{
	override void EEOnCECreate()
	{
		super.EEOnCECreate();
		ExorVO_Ammo.ApplySpawnQuantity(this);
	}

	override void OnInventoryEnter(Man player)
	{
		super.OnInventoryEnter(player);
		if (!GetGame().IsServer())
			return;
		// gate barato primero: bandera ya leida de la config (ver ExorHotFlags)
		if (!ExorHotFlags.AutoStackMunicion())
			return;
		PlayerBase pb = PlayerBase.Cast(player);
		if (pb)
			ExorVO_Ammo.AutoStackLater(this, pb);
	}
}
