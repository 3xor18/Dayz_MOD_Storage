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

		// PRIMERO DE TODO: auto-reparacion de la persistencia. OnInit corre ANTES de que el
		// CE restaure los dynamic_*.bin, asi que este es el unico momento en que se puede
		// arreglar un archivo podrido que hace morir el arranque. Ver ExorBootRepair.
		ExorBootRepair.Run();

		ExorConfig cfg = GetExorConfig();
		// Banderas de config al camino caliente: se leen UNA vez aca y los hooks que corren
		// por item / por frame las consultan como un simple bool estatico. Ver ExorHotFlags.
		ExorHotFlags.Refresh();
		// Resuelve si el apagado anterior fue limpio (define si los contenedores barren el
		// piso en su 1ra apertura) y deja la marca de esta sesion. Ver ExorApagadoLimpio.
		ExorApagadoLimpio.Init();
		// Deja en el RPT QUE MAPA y de que tamaño cree el mod que esta corriendo. Es la
		// primera cosa que uno quiere ver al cambiar de terreno, y sin esto la deteccion
		// es perezosa: si ningun JSON tiene coordenadas, nunca se evalua y no se entera nadie.
		ExorMapBounds.Size();
		// Banco de pruebas automatico: si hay marcador perf_test.txt, el server monta la
		// carga y se perfila solo, sin necesidad de que nadie entre al juego. Ver ExorAutoPerf.
		ExorAutoPerf.Init();
		// VALVULA DE EMERGENCIA del admin: este arranque no deserializa las neveras. Se traduce
		// aca, en OnInit (que corre ANTES de que el CE cargue la persistencia), y no dentro del
		// OnStoreLoad de la nevera: la carga de entidades es justo el momento en el que no
		// conviene depender de que la config este sana. Ver ExorBootRepair.SaltearTipo.
		if (cfg.storage.saltear_carga_neveras)
		{
			ExorBootRepair.MarcarTipoSalteado("Exor_Fridge");
			Print(string.Format("%1 CARGA-SEGURA: saltear_carga_neveras=true -> las neveras no se deserializan en este arranque", ExorStorageConstants.LOG));
		}
		ExorVO_AutoPopulateAmmo(cfg.municion);
		ExorVO_Manager.Start();
		ExorGroupManager.Init();
		ExorPartyLive.Start();
		ExorRaidLog.Init();	// crea raidlog\ y purga archivos mas viejos que log_dias_retener
		ExorKillFarm.Init();	// carga el ledger anti-farmeo (sobrevive los reinicios cada 4h)
		ExorGroupManager.Get().ScanInactiveClans();	// avisa al raidlog de clanes sin conexion hace 'aviso_tiempo_inactividad_dias'
		// BARRIDO periodico de mastiles perdidos: restaura los que quedaron mast_lost==1
		// (auto-restore no completado por no haber un miembro online en el momento). Diferido
		// 30s para no chocar con la carga de persistencia; luego se re-agenda solo cada 2 min.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorGroupManager.Get().RestoreLostMastsTick, 30000, false);
		ExorKoth.Start();	// KOTH: eventos de captura con recompensa (si koth.json activar=true)
		ExorCofre.Start();	// COFRE: zonas de apertura de cofres por horario (si cofre.json activado=true)
		ExorServerMsg.Start();	// mensajes automaticos del server al chat (mensajes.json: repetibles + agendados)
		ExorTumbaForense.Limpiar();	// borra los JSON forenses de tumbas caducados (retencion en bodycadaver.json)
		// Purga de JSON de bolsas sin dueño. Diferido 60s A PROPOSITO: necesita que la
		// persistencia ya haya cargado y registrado las bolsas vivas, porque lo que decide
		// que un archivo es basura es que su id NO corresponda a ninguna bolsa registrada.
		// Correrlo antes borraria la virtualizacion de tumbas vivas = perdida de loot.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Exor_BodyBag.PurgarHuerfanos, 60000, false);

		// OJO: este compilador no acepta expresiones partidas en varias lineas (ni ternarios)
		// Banner de arranque: version + sello de build, bien visible y facil de grepear en el
		// RPT (grep "3xorVO BUILD"). Confirma QUE PBO esta corriendo el server antes de
		// diagnosticar cualquier regresion.
		Print("[3xorVO] ============================================================");
		Print(string.Format("%1 BUILD -> %2 v%3 (build %4)", ExorStorageConstants.LOG, ExorStorageConstants.MOD_NAME, ExorStorageConstants.MOD_VERSION, ExorStorageConstants.MOD_BUILD));
		Print("[3xorVO] ============================================================");
		Print(string.Format("%1 %2 v%3 inicializado", ExorStorageConstants.LOG, ExorStorageConstants.MOD_NAME, ExorStorageConstants.MOD_VERSION));
		Print(string.Format("%1 virtualizar_segundos=%2 auto_cerrar_segundos=%3 multiplicador_comida=%4", ExorStorageConstants.LOG, cfg.storage.virtualizar_segundos, cfg.storage.auto_cerrar_segundos, cfg.storage.multiplicador_comida));
		Print(string.Format("%1 vehiculos_dormir=%2 dormir_minutos=%3 despertar_metros=%4", ExorStorageConstants.LOG, cfg.vehiculos.vehiculos_dormir, cfg.vehiculos.vehiculos_dormir_minutos, cfg.vehiculos.vehiculos_despertar_metros));
		Print(string.Format("%1 auto_stack=%2 spawn_municion=%3 tipos registrados", ExorStorageConstants.LOG, cfg.municion.auto_stack, cfg.municion.spawn_municion.Count()));
	}

	// Monitor de rendimiento (diagnostico): mide FPS/peor-frame por intervalo. Costo:
	// 2 sumas por frame. Loguea 1 linea cada 30s. Ver ExorPerfMonitor.c.
	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);
		ExorPerfMonitor.Feed(timeslice);
	}

	// Al APAGAR el server: virtualizar todos los barriles/bodybags con contenido, asi
	// su loot anidado pasa al JSON antes del guardado de persistencia y NO se cae al
	// piso al reiniciar (limitacion vanilla con anidado profundo bag-in-barril).
	override void OnMissionFinish()
	{
		ExorVO_Manager.VirtualizeAll();
		ExorStats.Get().FlushIfDirty();	// volcar stats pendientes antes de apagar
		ExorRoboBuffer.FlushAll();		// cerrar sesiones de saqueo abiertas (ANTES del Flush del log)
		ExorTumbaForense.FlushPending();	// volcar el looteo de tumbas pendiente
		ExorRaidLog.Flush();			// volcar el log de auditoria bufferizado antes de apagar
		ExorKillFarm.FlushIfDirty();	// volcar el ledger anti-farmeo antes de apagar (ventana 4h vs reinicio)
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
		// STAFF -> al cliente. El cliente no puede resolverlo solo (GetIdentity() es null ahi),
		// asi que lo decide el server y se lo sincroniza. Lo usan las acciones que solo tiene
		// que ver el staff, ej "Eliminar definitivamente (admin)".
		ExorCfgStorage exorStaffCfg = GetExorConfig().storage;
		bool exorEsStaff = false;
		if (exorStaffCfg && exorStaffCfg.bypass_lootear_steamids)
			exorEsStaff = exorStaffCfg.bypass_lootear_steamids.Find(ExorGroupManager.SteamId(player)) != -1;
		player.ExorSetStaff(exorEsStaff);

		ExorGroupManager.Get().OnPlayerConnected(player);
		ExorTerritoryManager.Get().SyncToPlayer(player);
		// SELF-HEAL del mastil: si el grupo del jugador perdio su mastil (se cayo tras un crash
		// del server y el grupo -guardado aparte- sobrevivio), recrearlo en su posicion guardada.
		// Diferido 8s para que la persistencia vanilla termine de cargar los mastiles que si
		// sobrevivieron (asi FindBuiltMastByGroup no crea un duplicado).
		ExorGroup exorHealG = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		if (exorHealG)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorTerritoryManager.Get().HealGroupMast, 8000, false, exorHealG, player);
		ExorKoth.Get().SyncMarkersToPlayer(player);	// que vea las marcas de los koth activos en su mapa
		ExorCofre.Get().SyncMarkersToPlayer(player);	// marcas de las zonas de cofres abiertas

		// BUGFIX (relog con lag): el roster (lista de miembros / menu P) se mandaba UNA
		// sola vez aca. En un relog laggy ese envio fragmentado puede llegar antes de que
		// el cliente este listo y se PIERDE -> el jugador queda sin ver a su party hasta el
		// proximo relog. Lo re-mandamos a los 3s y 8s (idempotente: si no esta en grupo,
		// SyncToPlayer manda un roster vacio = correcto).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResyncRoster, 3000, false, player);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResyncRoster, 8000, false, player);

		// RE-SYNC candados de auto (s_Member/s_Access del cliente arrancan vacios cada sesion): a los 6s
		// y 10s (idempotente) para que tras reinicio/reconexion el dueño vea sus autos con candado como
		// propios (subir + baul) sin re-ingresar la clave. El del parking lo cubre ExorCarScheduleMemberNotify.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResyncCarLocks, 6000, false, player);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResyncCarLocks, 10000, false, player);

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

			// avisar al cliente si es VIP (para features VIP client-side, ej. distancia en marcas).
			// Se manda AHORA + REENVIADO a los 3s y 8s (igual que el roster): al conectar, el HUD
			// del cliente puede no estar listo y el 1er envio se pierde -> la distancia VIP en las
			// marcas dejaba de verse. Los reenvios diferidos aseguran que el flag llegue.
			bool isVip = GetExorConfig().vip.IsVip(identity.GetPlainId());
			player.RPCSingleParam(ExorRPC.VIP_STATUS, new Param1<bool>(isVip), true, identity);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResendVip, 3000, false, player, isVip);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResendVip, 8000, false, player, isVip);

			// CANDADO DE AUTOS: devolverle el acceso a los autos donde YA ingreso la clave
			// (persistido en el JSON) -> NO se la re-pide tras reconectar/reiniciar. Diferido
			// (como el VIP) para que el cache del cliente ya este listo. El grant lleva el
			// network-id del auto; funciona aunque el auto este lejos (el id calza al acercarse).
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResendCarAccess, 4000, false, player);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorResendCarAccess, 9000, false, player);
		}
	}

	// re-otorga el acceso al baul/manejo de todos los autos con candado donde el player ya
	// habia metido la clave (o es admin). Corre server-side.
	void ExorResendCarAccess(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;
		string sid = ExorGroupManager.SteamId(player);
		ExorVO_Manager m = ExorVO_Manager.Get();
		if (!m || !m.m_Vehicles)
			return;
		int i;
		for (i = 0; i < m.m_Vehicles.Count(); i++)
		{
			CarScript car = m.m_Vehicles.Get(i);
			if (!car || !car.ExorCarHasLock())
				continue;
			// acceso (ya metio la clave o admin) -> baul + manejar + "Cambiar clave"
			if (car.ExorCarIsUnlockedBy(sid) || car.ExorCarIsAdmin(sid))
				player.ExorSendCarAccessGrant(car);
			// miembro del clan dueño -> ve "Ingresar clave" (el ajeno NO, solo raidea)
			if (car.ExorCarIsMemberOfLockGroup(sid))
				player.ExorSendCarMember(car);
		}
	}

	// reenvio diferido del flag VIP (ver arriba: cubre el timing del HUD del cliente al conectar)
	void ExorResendVip(PlayerBase player, bool isVip)
	{
		if (player && player.GetIdentity())
			player.RPCSingleParam(ExorRPC.VIP_STATUS, new Param1<bool>(isVip), true, player.GetIdentity());
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
		// SendOpenTracked (no SendOpen): manda el menu Y reintenta si el jugador no elige,
		// para que un cliente que todavia estaba cargando no se quede sin pantalla de spawn.
		if (player)
			ExorSpawn.SendOpenTracked(player);
	}

	// Re-envia el roster de party (y el cache de territorio) tras conectar/respawnear,
	// para tapar la perdida del envio inicial en relogs con lag.
	// IMPORTANTE: reenvia el roster a TODO el grupo (SyncGroup), no solo a 'player'. Antes
	// solo se lo remandaba a si mismo -> un COMPAÑERO ya online recibia el aviso del recien
	// llegado/revivido UNA sola vez (el envio 0s de OnPlayerConnected/SyncGroup) y si ese
	// paquete se perdia en lag, "no veia al que entro/revivio" hasta el proximo evento.
	// Ahora cada connect/respawn le da a TODOS los miembros online 3 envios (0s + 3s + 8s).
	// re-sync de los candados de auto del jugador al conectar (ver ExorAfterClientSpawned)
	void ExorResyncCarLocks(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;
		ExorVO_Manager.ExorSyncCarLocksForPlayer(player);
	}

	void ExorResyncRoster(PlayerBase player)
	{
		if (!player || !player.GetIdentity())
			return;
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		if (g)
			ExorGroupManager.Get().SyncGroup(g);	// roster a TODO el grupo online (el que entra + sus compañeros)
		else
			ExorGroupManager.Get().SyncToPlayer(player, null);	// sin grupo -> limpiar el roster del cliente
		ExorTerritoryManager.Get().SyncToPlayer(player);		// el cache de territorio si es propio del receptor
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
