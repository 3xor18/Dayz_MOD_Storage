// ============================================================================
// 3xor_Vanilla_Optimization - Pastilla de rareza + barra de durabilidad (Fase G)
// Inyecta widgets graficos en el tooltip del item (hover) y en la pantalla de
// inspeccion (click derecho), dentro de la columna derecha (GridSpacerWidget1).
//   - Pastilla de rareza: barra coloreada con la palabra centrada (epico=morado).
//   - Durabilidad: barra verde/amarillo/rojo que se rellena segun el estado.
// Configurable por items.json (mostrar_durabilidad / mostrar_rareza).
// Hooks por metodo de INSTANCIA (no static): ItemManager.PrepareTooltip +
// InspectMenuNew.SetItem, ambos llaman a ExorTooltipBadge.Apply.
// ============================================================================
class ExorTooltipBadge
{
	// Inserta/actualiza la pastilla + barra dentro del root del tooltip/inspeccion.
	static void Apply(Widget root, EntityAI item)
	{
		if (!root || !item)
			return;
		ItemBase it = ItemBase.Cast(item);
		if (!it)
			return;

		ExorCfgItems cfg = GetExorConfig().items;
		if (!cfg.mostrar_durabilidad && !cfg.mostrar_rareza)
			return;

		// columna derecha del tooltip; si el layout no la tiene, no hacemos nada
		Widget col = root.FindAnyWidget("GridSpacerWidget1");
		if (!col)
			return;

		// crear el badge una sola vez por tooltip (se reutiliza entre items)
		Widget badge = root.FindAnyWidget("ExorBadgeRoot");
		if (!badge)
		{
			badge = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_item_badge.layout", col);
			if (!badge)
				return;
		}

		ImageWidget rarBg  = ImageWidget.Cast(badge.FindAnyWidget("ExorRarityBg"));
		TextWidget  rarTxt = TextWidget.Cast(badge.FindAnyWidget("ExorRarityText"));
		Widget      durBg  = badge.FindAnyWidget("ExorDuraBg");
		Widget      durFil = badge.FindAnyWidget("ExorDuraFill");
		TextWidget  durTxt = TextWidget.Cast(badge.FindAnyWidget("ExorDuraText"));

		// --- Rareza ---
		bool showRar = cfg.mostrar_rareza;
		if (showRar)
		{
			if (rarBg)
			{
				rarBg.SetColor(ExorItemInfo.RarityColor(it));
				rarBg.Show(true);
			}
			if (rarTxt)
			{
				rarTxt.SetText(ExorItemInfo.RarityLabel(it));
				rarTxt.Show(true);
			}
		}
		else
		{
			if (rarBg) rarBg.Show(false);
			if (rarTxt) rarTxt.Show(false);
		}

		// --- Durabilidad ---
		float h = ExorItemInfo.DurabilityFraction(it);
		bool showDur = cfg.mostrar_durabilidad && h >= 0;
		if (showDur)
		{
			int pct = Math.Round(h * 100);
			if (durFil)
			{
				durFil.SetSize(h, 1.0);
				durFil.SetColor(ExorItemInfo.DurabilityColor(h));
			}
			if (durBg) durBg.Show(true);
			if (durTxt)
			{
				durTxt.SetText(pct.ToString() + "%");
				durTxt.Show(true);
			}
		}
		else
		{
			if (durBg) durBg.Show(false);
			if (durTxt) durTxt.Show(false);
		}
	}
}

// Hover en el inventario -> tooltip del item
modded class ItemManager
{
	override void PrepareTooltip(EntityAI item, int x = 0, int y = 0)
	{
		super.PrepareTooltip(item, x, y);
		if (item && item.IsInherited(InventoryItem))
			ExorTooltipBadge.Apply(m_TooltipWidget, item);
	}
}

// Click derecho -> pantalla de inspeccion del item
modded class InspectMenuNew
{
	override void SetItem(EntityAI item)
	{
		super.SetItem(item);
		ExorTooltipBadge.Apply(layoutRoot, item);
	}
}
