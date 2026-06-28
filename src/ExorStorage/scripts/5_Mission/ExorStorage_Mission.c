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
		ExorRaidLog.Init();	// crea raidlog\ y purga archivos mas viejos que log_dias_retener
		ExorGroupManager.Get().ScanInactiveClans();	// avisa al raidlog de clanes sin conexion hace 'inactividad_dias'
		ExorAnticheat.Start();	// anti-cheat heuristico: tick de watchlist (~1 Hz) + detector por kill

		// OJO: este compilador no acepta expresiones partidas en varias lineas (ni ternarios)
		Print(string.Format("%1 %2 v%3 inicializado", ExorStorageConstants.LOG, ExorStorageConstants.MOD_NAME, ExorStorageConstants.MOD_VERSION));
		Print(string.Format("%1 virtualizar_segundos=%2 auto_cerrar_segundos=%3 multiplicador_comida=%4", ExorStorageConstants.LOG, cfg.storage.virtualizar_segundos, cfg.storage.auto_cerrar_segundos, cfg.storage.multiplicador_comida));
		Print(string.Format("%1 vehiculos_dormir=%2 dormir_minutos=%3 despertar_metros=%4", ExorStorageConstants.LOG, cfg.vehiculos.vehiculos_dormir, cfg.vehiculos.vehiculos_dormir_minutos, cfg.vehiculos.vehiculos_despertar_metros));
		Print(string.Format("%1 auto_stack=%2 spawn_municion=%3 tipos registrados", ExorStorageConstants.LOG, cfg.municion.auto_stack, cfg.municion.spawn_municion.Count()));
	}

	// Al APAGAR el server: virtualizar todos los barriles/bodybags con contenido, asi
	// su loot anidado pasa al JSON antes del guardado de persistencia y NO se cae al
	// piso al reiniciar (limitacion vanilla con anidado profundo bag-in-barril).
	override void OnMissionFinish()
	{
		ExorVO_Manager.VirtualizeAll();
		ExorStats.Get().FlushIfDirty();	// volcar stats pendientes antes de apagar
		super.OnMissionFinish();
	}

	// Al estar el cliente listo: sincronizar su roster de party + tocar last_seen
	override void OnClientReadyEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientReadyEvent(identity, player);
		ExorAfterClientSpawned(player);
	}

	// RESPAWN (tras morir con un personaje EXISTENTE): re-sincronizar igual que al
	// conectar (OnClientReadyEvent no se vuelve a disparar en respawn).
	override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
	{
		super.OnClientRespawnEvent(identity, player);
		ExorAfterClientSpawned(player);
	}

	// Sincronizacion comun al estar el personaje listo (conexion / respawn / nuevo).
	// Deriva la identidad del propio player para poder llamarse tambien diferido.
	void ExorAfterClientSpawned(PlayerBase player)
	{
		if (!player)
			return;
		PlayerIdentity identity = player.GetIdentity();
		ExorGroupManager.Get().OnPlayerConnected(player);
		ExorTerritoryManager.Get().SyncToPlayer(player);

		// BUGFIX (relog con lag): el roster (lista de miembros / menu P) se mandaba UNA
		// sola vez aca. En un relog laggy ese envio fragmentado puede llegar antes de que
		// el cliente este listo y se PIERDE -> el jugador queda sin ver a su party hasta el
		// proximo relog. Lo re-mandamos a los 3s y 8s (idempotente: si no esta en grupo,
		// SyncToPlayer manda un roster vacio = correcto).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResyncRoster, 3000, false, player);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResyncRoster, 8000, false, player);

		// #4b: si reaparece dentro de territorio ajeno, sacarlo al borde (diferido).
		ExorAntiRaid.KickFromEnemyBaseIfNeeded(player);

		// Enviar al cliente la config relevante (toggles party/mapa/items) para
		// que respete los JSON del admin aunque no los tenga en su perfil local.
		if (identity)
		{
			// CONFIG_SYNC en TROZOS: el bundle (toggles party/mapa/items/veh/reparacion/autorun)
			// puede crecer (ej. items.rareza_tabla grande) y pasarse de ~2KB -> "String CORRUPTED".
			string cfgJson = GetExorConfig().BuildClientJson();
			ExorNetChunk.Send(player, identity, ExorRPC.CONFIG_SYNC, cfgJson);

			// serverinfo (texto del panel): el texto puede ser largo (reglas/territorio/etc.)
			// y un solo string de RPC > ~2KB revienta la VM del cliente ("String CORRUPTED")
			// -> se manda en TROZOS y el cliente lo reensambla (ver ExorNetChunk/ExorBigStringRx).
			string siJson = GetExorConfig().BuildServerInfoJson();
			ExorNetChunk.Send(player, identity, ExorRPC.SERVERINFO_SYNC, siJson);

			// avisar al cliente si es VIP (para features VIP client-side, ej. distancia en marcas)
			bool isVip = GetExorConfig().vip.IsVip(identity.GetPlainId());
			player.RPCSingleParam(ExorRPC.VIP_STATUS, new Param1<bool>(isVip), true, identity);
		}
	}

	// #4a: al desconectarse, si esta dentro de territorio ajeno, dejar rastro forense.
	// #6: si se desloguea en pleno PvP (combat-log), dejar rastro con ambos steamids.
	override void OnClientDisconnectedEvent(PlayerIdentity identity, PlayerBase player, int logoutTime, bool authFailed)
	{
		ExorAntiRaid.OnDisconnectInEnemyTerritory(player, identity);
		ExorAntiRaid.OnCombatLogout(player, identity);
		super.OnClientDisconnectedEvent(identity, player, logoutTime, authFailed);
	}

	// Personaje NUEVO (primer login O respawn por muerte): abrir la pantalla de
	// seleccion de spawn unos segundos despues (cuando el cliente termino de cargar).
	override PlayerBase OnClientNewEvent(PlayerIdentity identity, vector pos, ParamsReadContext ctx)
	{
		PlayerBase player = super.OnClientNewEvent(identity, pos, ctx);
		Print(string.Format("%1 OnClientNewEvent %2", ExorStorageConstants.LOG, identity.GetPlainId()));

		// TEST: cuchillo al spawnear (suicidio/kills faciles al testear). Toggle en spawns.json.
		if (player && GetExorConfig().spawns.dar_cuchillo_al_spawnear)
		{
			EntityAI cuchillo = player.GetInventory().CreateInInventory("CombatKnife");
			if (!cuchillo)
				cuchillo = player.GetHumanInventory().CreateInHands("CombatKnife");
		}
		ExorCfgSpawns spawns = GetExorConfig().spawns;
		if (player && spawns.habilitado && spawns.puntos.Count() > 0)
		{
			Print(string.Format("%1 programando SPAWN_OPEN (5s) - puntos=%2", ExorStorageConstants.LOG, spawns.puntos.Count()));
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDelayedSendOpen, 5000, false, player);
		}

		// Personaje NUEVO: OnClientReadyEvent NO se dispara, asi que sincronizamos
		// el roster/territorio/config diferido (cuando el cliente ya cargo). Si no,
		// el cliente queda sin roster y no aparecen "Invitar"/"Administrar party".
		if (player)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAfterClientSpawned, 5000, false, player);

		return player;
	}

	void ExorDelayedSendOpen(PlayerBase player)
	{
		if (player)
			ExorSpawn.SendOpen(player);
	}

	// Re-envia el roster de party (y el cache de territorio) a un jugador ya conectado.
	// Diferido tras conectar para tapar la perdida del envio inicial en relogs con lag.
	void ExorResyncRoster(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		ExorGroupManager.Get().SyncToPlayer(player, g);	// g==null -> roster vacio (correcto si no esta en grupo)
		ExorTerritoryManager.Get().SyncToPlayer(player);
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
