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

		ExorConfig cfg = GetExorConfig();
		ExorVO_AutoPopulateAmmo(cfg.municion);
		ExorVO_Manager.Start();
		ExorGroupManager.Init();
		ExorPartyLive.Start();

		// OJO: este compilador no acepta expresiones partidas en varias lineas (ni ternarios)
		Print(string.Format("%1 %2 v%3 inicializado", ExorStorageConstants.LOG, ExorStorageConstants.MOD_NAME, ExorStorageConstants.MOD_VERSION));
		Print(string.Format("%1 virtualizar_minutos=%2 auto_cerrar_minutos=%3 multiplicador_comida=%4", ExorStorageConstants.LOG, cfg.storage.virtualizar_minutos, cfg.storage.auto_cerrar_minutos, cfg.storage.multiplicador_comida));
		Print(string.Format("%1 vehiculos_dormir=%2 dormir_minutos=%3 despertar_metros=%4", ExorStorageConstants.LOG, cfg.vehiculos.vehiculos_dormir, cfg.vehiculos.vehiculos_dormir_minutos, cfg.vehiculos.vehiculos_despertar_metros));
		Print(string.Format("%1 auto_stack=%2 spawn_municion=%3 tipos registrados", ExorStorageConstants.LOG, cfg.municion.auto_stack, cfg.municion.spawn_municion.Count()));
	}

	// Al estar el cliente listo: sincronizar su roster de party + tocar last_seen
	override void OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReadyEvent(identity, player);
		ExorGroupManager.Get().OnPlayerConnected(player);
		ExorTerritoryManager.Get().SyncToPlayer(player);

		// Enviar al cliente la config relevante (toggles party/mapa/items) para
		// que respete los JSON del admin aunque no los tenga en su perfil local.
		if (player && identity)
		{
			string cfgJson = GetExorConfig().BuildClientJson();
			player.RPCSingleParam(ExorRPC.CONFIG_SYNC, new Param1<string>(cfgJson), true, identity);
		}
	}

	// Personaje NUEVO (primer login O respawn por muerte): abrir la pantalla de
	// seleccion de spawn unos segundos despues (cuando el cliente termino de cargar).
	override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
	{
		PlayerBase player = super.OnClientNewEvent(identity, pos, ctx);
		Print(string.Format("%1 OnClientNewEvent %2", ExorStorageConstants.LOG, identity.GetPlainId()));
		ExorCfgSpawns spawns = GetExorConfig().spawns;
		if (player && spawns.habilitado && spawns.puntos.Count() > 0)
		{
			Print(string.Format("%1 programando SPAWN_OPEN (5s) - puntos=%2", ExorStorageConstants.LOG, spawns.puntos.Count()));
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDelayedSendOpen, 5000, false, player);
		}
		return player;
	}

	void ExorDelayedSendOpen(PlayerBase player)
	{
		if (player)
			ExorSpawn.SendOpen(player);
	}

	void ExorVO_AutoPopulateAmmo(ExorCfgMunicion settings)
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
			JsonFileLoader<ExorCfgMunicion>.JsonSaveFile(ExorStorageConstants.CFG_MUNICION, settings);
			Print(string.Format("%1 municion.json: %2 entradas de municion auto-agregadas", ExorStorageConstants.LOG, agregadas));
		}
	}
}
