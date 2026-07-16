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

	// Consolida TODAS las pilas del mismo tipo del inventario del jugador
	// en la menor cantidad de pilas posible (ej. 50+25+15 -> 90)
	static void AutoStack(Magazine mag, PlayerBase player)
	{
		if (!GetGame().IsServer())
			return;
		if (!mag || !player)
			return;
		if (!IsManagedAmmo(mag))
			return;

		ExorCfgMunicion settings = GetExorConfig().municion;
		if (!settings.auto_stack)
			return;
		// Solo si la pila realmente quedo en el inventario del jugador
		if (mag.GetHierarchyRootPlayer() != player)
			return;
		if (IsInHands(mag))
			return;

		string tipo = mag.GetType();
		int maxA = mag.GetAmmoMax();
		if (maxA <= 0)
			return;

		// Solo participan las pilas PARCIALES (0 < ammo < max). Las pilas LLENAS NO se
		// tocan nunca: no se pueden fusionar y mutarlas solo genera manipulacion de
		// inventario server-side inutil (= mas ventana de desync). Menos churn = menos
		// riesgo de "balas fantasma hasta reloguear".
		array<EntityAI> items = new array<EntityAI>;
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);

		array<Magazine> pilas = new array<Magazine>;
		int total = 0;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			Magazine other = Magazine.Cast(items.Get(i));
			if (!other)
				continue;
			if (other.GetType() != tipo)
				continue;
			if (IsInHands(other))
				continue;
			int cnt = other.GetAmmoCount();
			if (cnt <= 0 || cnt >= maxA)	// vacia o LLENA: no participa
				continue;
			pilas.Insert(other);
			total = total + cnt;
		}

		if (pilas.Count() < 2)
			return;

		// Consolidar SOLO si de verdad REDUCE la cantidad de pilas. Si las parciales ya
		// estan en el minimo posible (ej. 60+40 con max 80 = 2 pilas si o si), no se toca
		// nada: evita mutar el inventario sin ganancia (y su desync asociado).
		int minPiles = (total + maxA - 1) / maxA;
		if (minPiles >= pilas.Count())
			return;

		int antes = pilas.Count();

		// Contenedores padre afectados: hay que forzar su re-sincronizacion tras
		// borrar/reescribir pilas. Sin esto, si el jugador tiene el contenedor ABIERTO
		// (chaleco, mochila, o una TUMBA/barril que esta looteando), la vista del cliente
		// queda stale y "las balas se desaparecen" hasta reloguear (el server siempre
		// quedo correcto -> por eso reaparecen).
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
			pila.SetSynchDirty();   // replicar el nuevo conteo al cliente
			total = total - poner;
		}

		// forzar que los contenedores padre (y el player) re-sincronicen su cargo
		// (slots vaciados) hacia el cliente.
		for (i = 0; i < padres.Count(); i++)
		{
			if (padres.Get(i))
				padres.Get(i).SetSynchDirty();
		}
		player.SetSynchDirty();

		Print(string.Format("%1 Auto-stack: %2 consolidado %3 -> %4 pilas", ExorStorageConstants.LOG, tipo, antes, minPiles));
	}

	// Diferido: el movimiento de inventario termina de forma asincronica
	static void AutoStackLater(Magazine mag, PlayerBase player)
	{
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorVO_Ammo.AutoStack, 400, false, mag, player);
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
		PlayerBase pb = PlayerBase.Cast(player);
		if (pb)
		{
			ExorVO_Ammo.AutoStackLater(this, pb);
		}
	}
}
