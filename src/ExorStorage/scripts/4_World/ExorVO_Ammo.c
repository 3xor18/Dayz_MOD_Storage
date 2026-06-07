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
		ExorStorageSettings settings = GetExorStorageSettings();
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

		ExorStorageSettings settings = GetExorStorageSettings();
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

	// Fusiona la pila recogida con las pilas existentes del jugador
	static void AutoStack(Magazine mag, PlayerBase player)
	{
		if (!GetGame().IsServer())
			return;
		if (!mag || !player)
			return;
		if (!IsManagedAmmo(mag))
			return;

		ExorStorageSettings settings = GetExorStorageSettings();
		if (!settings.auto_stack)
			return;
		// Solo si la pila realmente quedo en el inventario del jugador
		if (mag.GetHierarchyRootPlayer() != player)
			return;

		array<EntityAI> items = new array<EntityAI>;
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, items);

		int restante = mag.GetAmmoCount();
		int original = restante;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			if (restante <= 0)
				break;
			Magazine other = Magazine.Cast(items.Get(i));
			if (!other)
				continue;
			if (other == mag)
				continue;
			if (other.GetType() != mag.GetType())
				continue;
			int espacio = other.GetAmmoMax() - other.GetAmmoCount();
			if (espacio <= 0)
				continue;
			int mover = restante;
			if (mover > espacio)
				mover = espacio;
			other.ServerSetAmmoCount(other.GetAmmoCount() + mover);
			restante = restante - mover;
		}

		if (restante != original)
		{
			if (restante <= 0)
			{
				GetGame().ObjectDelete(mag);
			}
			else
			{
				mag.ServerSetAmmoCount(restante);
			}
		}
	}

	// Diferido: la accion Take termina de mover el item de forma asincronica
	static void AutoStackLater(Magazine mag, PlayerBase player)
	{
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorVO_Ammo.AutoStack, 400, false, mag, player);
	}
}

// Cantidad aleatoria cuando el CE spawnea la pila como loot
modded class Ammunition_Base
{
	override void EEOnCECreate()
	{
		super.EEOnCECreate();
		ExorVO_Ammo.ApplySpawnQuantity(this);
	}
}

// Auto-stack al recoger del piso con la accion "Take" (tecla F)
modded class ActionTakeItem
{
	override void OnExecuteServer(ActionData action_data)
	{
		super.OnExecuteServer(action_data);
		if (!action_data || !action_data.m_Target)
			return;
		Magazine mag = Magazine.Cast(action_data.m_Target.GetObject());
		if (mag && action_data.m_Player)
		{
			ExorVO_Ammo.AutoStackLater(mag, action_data.m_Player);
		}
	}
}
