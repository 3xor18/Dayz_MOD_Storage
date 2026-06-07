// ============================================================================
// 3xor_Vanilla_Optimization - Init del lado server
// - Carga settings.json del profile
// - Auto-completa stack_municion/spawn_municion con TODAS las municiones
//   detectadas en config (vanilla + mods), usando los defaults como relleno
// - Arranca el manager (virtualizacion / auto-cierre / vehiculos)
// ============================================================================
modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		ExorStorageSettings settings = GetExorStorageSettings();
		ExorVO_AutoPopulateAmmo(settings);
		ExorVO_Manager.Start();

		// OJO: este compilador no acepta expresiones partidas en varias lineas (ni ternarios)
		Print(string.Format("%1 %2 v%3 inicializado", ExorStorageConstants.LOG, ExorStorageConstants.MOD_NAME, ExorStorageConstants.MOD_VERSION));
		Print(string.Format("%1 virtualizar_minutos=%2 auto_cerrar_minutos=%3 multiplicador_comida=%4", ExorStorageConstants.LOG, settings.virtualizar_minutos, settings.auto_cerrar_minutos, settings.multiplicador_comida));
		Print(string.Format("%1 vehiculos_dormir=%2 dormir_minutos=%3 despertar_metros=%4", ExorStorageConstants.LOG, settings.vehiculos_dormir, settings.vehiculos_dormir_minutos, settings.vehiculos_despertar_metros));
		Print(string.Format("%1 auto_stack=%2 spawn_municion=%3 tipos registrados", ExorStorageConstants.LOG, settings.auto_stack, settings.spawn_municion.Count()));
	}

	void ExorVO_AutoPopulateAmmo(ExorStorageSettings settings)
	{
		string cfg = "CfgMagazines";
		int total = GetGame().ConfigGetChildrenCount(cfg);
		int agregadas = 0;
		int i;
		for (i = 0; i < total; i++)
		{
			string name;
			GetGame().ConfigGetChildName(cfg, i, name);
			if (GetGame().ConfigGetInt(string.Format("%1 %2 scope", cfg, name)) != 2)
				continue;
			if (!GetGame().IsKindOf(name, "Ammunition_Base"))
				continue;
			if (settings.municion_excluida.Find(name) != -1)
				continue;

			if (!settings.stack_municion.Contains(name))
			{
				settings.stack_municion.Set(name, settings.stack_municion_default);
				agregadas++;
			}
			if (!settings.spawn_municion.Contains(name))
			{
				ExorMunicionSpawnRango rango = new ExorMunicionSpawnRango();
				rango.min = settings.spawn_municion_min_default;
				rango.max = settings.spawn_municion_max_default;
				settings.spawn_municion.Set(name, rango);
				agregadas++;
			}
		}

		if (agregadas > 0)
		{
			JsonFileLoader<ExorStorageSettings>.JsonSaveFile(ExorStorageConstants.CONFIG_PATH, settings);
			Print(string.Format("%1 settings.json: %2 entradas de municion auto-agregadas", ExorStorageConstants.LOG, agregadas));
		}
	}
}
