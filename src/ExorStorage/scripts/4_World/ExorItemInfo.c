// ============================================================================
// 3xor_Vanilla_Optimization - Info de item: durabilidad + rareza (Fase G)
// Se agrega al nombre mostrado del item (ver ItemBase.GetDisplayName en
// ExorTerritory_Items.c). Barra de durabilidad (100% pristine, 0% ruined) + %.
// RAREZA: el "tier" del loot (types.xml) NO esta en el item en runtime, asi que
// se usa una HEURISTICA por clase (arma=Epico, optica/silenciador=Raro, etc.).
// Para rareza exacta por tier haria falta una tabla classname->tier (futuro).
// ============================================================================
class ExorItemInfo
{
	// Barra ASCII de 5 segmentos segun la salud 0..1
	static string Bar(float h)
	{
		int filled = Math.Round(h * 5);
		if (filled < 0) filled = 0;
		if (filled > 5) filled = 5;
		string s = "";
		int i;
		for (i = 0; i < 5; i++)
		{
			if (i < filled)
				s = s + "|";
			else
				s = s + ".";
		}
		return s;
	}

	static string DurabilityTag(ItemBase it)
	{
		// GetHealth01 es SOLO-server. En cliente (esto es UI) usamos el nivel de
		// salud sincronizado: 0=pristine, 1=worn, 2=damaged, 3=badly, 4=ruined.
		int lvl = it.GetHealthLevel();
		if (lvl < 0)
			return "";	// item sin sistema de dano (no mostrar barra)

		float h = 1.0;
		if (lvl == 1)
			h = 0.8;
		else if (lvl == 2)
			h = 0.55;
		else if (lvl == 3)
			h = 0.3;
		else if (lvl >= 4)
			h = 0.0;

		int pct = Math.Round(h * 100);
		return "[" + Bar(h) + " " + pct.ToString() + "%]";
	}

	// Tier del item: si rareza_usar_tabla esta activo, busca el classname en la
	// tabla (items.json); si no esta o la tabla esta off, usa la heuristica por clase.
	static string ResolveTier(ItemBase it)
	{
		ExorCfgItems cfg = GetExorConfig().items;
		if (cfg.rareza_usar_tabla && cfg.rareza_tabla)
		{
			string cn = it.GetType();
			if (cfg.rareza_tabla.Contains(cn))
				return cfg.rareza_tabla.Get(cn);
			return "comun";	// no esta en la tabla -> comun
		}
		// heuristica por clase
		if (it.IsInherited(Weapon_Base))
			return "epico";
		if (it.IsInherited(ItemOptics) || it.IsInherited(ItemSuppressor))
			return "raro";
		if (it.IsInherited(Magazine))
			return "poco_comun";
		return "comun";
	}

	// Etiqueta visible (mayuscula) para un tier (minuscula).
	static string TierLabel(string tier)
	{
		if (tier == "legendario")
			return "LEGENDARIO";
		if (tier == "epico")
			return "EPICO";
		if (tier == "raro")
			return "RARO";
		if (tier == "poco_comun")
			return "POCO COMUN";
		return "COMUN";
	}

	// Color (ARGB) de la pastilla para un tier (minuscula).
	static int TierColor(string tier)
	{
		if (tier == "legendario")
			return ARGB(255, 235, 150, 30);    // legendario = naranja/dorado
		if (tier == "epico")
			return ARGB(255, 150, 70, 200);    // epico = morado
		if (tier == "raro")
			return ARGB(255, 60, 120, 220);    // raro = azul
		if (tier == "poco_comun")
			return ARGB(255, 60, 180, 80);     // poco comun = verde
		return ARGB(255, 130, 130, 130);       // comun = gris
	}

	static string RarityLabel(ItemBase it)
	{
		return TierLabel(ResolveTier(it));
	}

	static string RarityTag(ItemBase it)
	{
		return "{" + RarityLabel(it) + "}";
	}

	// Color de la pastilla de rareza (ARGB).
	static int RarityColor(ItemBase it)
	{
		return TierColor(ResolveTier(it));
	}

	// Fraccion de durabilidad 0..1 (cliente, via GetHealthLevel). -1 = sin sistema.
	static float DurabilityFraction(ItemBase it)
	{
		int lvl = it.GetHealthLevel();
		if (lvl < 0)
			return -1;
		if (lvl == 0)
			return 1.0;
		if (lvl == 1)
			return 0.8;
		if (lvl == 2)
			return 0.55;
		if (lvl == 3)
			return 0.3;
		return 0.0;
	}

	// Color de la barra de durabilidad segun la fraccion.
	static int DurabilityColor(float h)
	{
		if (h >= 0.7)
			return ARGB(255, 40, 210, 40);     // verde
		if (h >= 0.4)
			return ARGB(255, 230, 210, 30);    // amarillo
		return ARGB(255, 220, 40, 40);         // rojo
	}
}
