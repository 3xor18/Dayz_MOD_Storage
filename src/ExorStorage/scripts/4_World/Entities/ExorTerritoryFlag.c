// ============================================================================
// 3xor_Vanilla_Optimization - Bandera vanilla = mastil de territorio (Fase C/D)
// Se moddea la TerritoryFlag vanilla: al colocar el kit de bandera VANILLA
// (rope + palos, sin recetas custom), la bandera:
//   - se ARMA al instante (auto-construye el poste, sin troncos/rocas)
//   - RECLAMA territorio + crea el party (el que la puso = lider)
// Tambien lleva el estado de bandera izada/bajada (Fase D), persistencia, y el
// despawn cuando el party se disuelve. NO hay Exor_MastKit ni acciones custom.
// ============================================================================
modded class TerritoryFlag
{
	protected string m_ExorGroupId;
	protected bool m_ExorDisbanding;	// lo borra el disband (evita recursion)
	protected bool m_ExorFlagRaised;	// bandera izada (sincronizado)
	protected int m_ExorClaimMinute;	// minuto de reclamo (ventana bandera blanca, granularidad de minutos)
	protected bool m_ExorInitDone;
	protected bool m_ExorInviteOpen;	// hay invitacion abierta (sincronizado a clientes)
	protected int m_ExorInviteUntilMin;	// server: minuto limite de la invitacion (auto-cierra a los 10 min)

	// ---- KOTH (mastil de evento King of the Hill) ----
	protected bool m_ExorIsKothMast;	// este mastil es de un KOTH: NO reclama territorio
	protected int m_ExorKothColorIdx;	// humo del color al capturar (1 amarillo / 2 verde / 3 morado)
	protected int m_ExorKothSmoke;		// NETSYNC: humo actual -1 ninguno / 0 blanco / 1..3 color
	protected Particle m_ExorSmokeFx;	// cliente: particula de humo activa (SIN ref: Particle es Object, gestionado por el motor)

	void TerritoryFlag()
	{
		RegisterNetSyncVariableBool("m_ExorFlagRaised");
		RegisterNetSyncVariableBool("m_ExorInviteOpen");
		RegisterNetSyncVariableInt("m_ExorKothSmoke", -1, 3);
		m_ExorKothSmoke = -1;
	}

	// ------------------------- estado de grupo/bandera -------------------------
	void ExorSetGroupId(string id) { m_ExorGroupId = id; }
	string ExorGetGroupId() { return m_ExorGroupId; }
	void ExorMarkDisbanding() { m_ExorDisbanding = true; }
	bool ExorIsFlagRaised() { return m_ExorFlagRaised; }

	void ExorServerSetFlagRaised(bool v)
	{
		m_ExorFlagRaised = v;
		ExorAnimateCloth(v);
		SetSynchDirty();
	}

	// Lee la animacion de la tela y actualiza el estado izada/bajada (para el
	// bloqueo de respawn). phase 0 = arriba, 1 = abajo; "izada" si esta mayormente
	// arriba. Lo llaman las acciones vanilla modded de izar/bajar.
	void ExorSyncRaisedFromCloth()
	{
		if (!GetGame().IsServer())
			return;
		float ph = GetAnimationPhase("flag_mast");
		bool raised = (ph < 0.5);
		if (raised != m_ExorFlagRaised)
		{
			m_ExorFlagRaised = raised;
			SetSynchDirty();
		}
	}

	// Re-aplica la animacion de la tela segun el estado izada/bajada persistido.
	// La usa ExorPostInit tras cargar (con reintentos por si la tela tarda en colgar).
	void ExorReapplyFlagVisual()
	{
		if (!GetGame().IsServer())
			return;
		ExorAnimateCloth(m_ExorFlagRaised);
	}

	// Anima la tela fisica (si hay una colgada) para que coincida con el estado.
	// phase 0 = arriba (izada), phase 1 = abajo (bajada) [vanilla esta invertido].
	void ExorAnimateCloth(bool raised)
	{
		if (!FindAttachmentBySlotName("Material_FPole_Flag"))
			return;
		float ph = 1;
		if (raised)
			ph = 0;
		AnimateFlagEx(ph);
	}

	// ------------------------- invitacion abierta (en el mastil) -------------------------
	bool ExorIsInviteOpen() { return m_ExorInviteOpen; }

	void ExorOpenInvite()
	{
		m_ExorInviteOpen = true;
		m_ExorInviteUntilMin = ExorTimeUtil.NowMinutes() + 10;
		SetSynchDirty();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoCloseInvite, 600000, false);
	}

	void ExorCloseInvite()
	{
		m_ExorInviteOpen = false;
		SetSynchDirty();
	}

	void ExorAutoCloseInvite()
	{
		if (!m_ExorInviteOpen)
			return;
		if (ExorTimeUtil.NowMinutes() >= m_ExorInviteUntilMin)
			ExorCloseInvite();
	}

	void ExorOnClaimed(int minute)
	{
		m_ExorClaimMinute = minute;
		m_ExorFlagRaised = true;
		SetSynchDirty();
	}

	bool ExorInWhiteFlagWindow()
	{
		ExorCfgPartyBandera b = GetExorConfig().party.bandera;
		if (!b.bandera_blanca)
			return false;
		int now = ExorTimeUtil.NowMinutes();
		return (now - m_ExorClaimMinute) < b.bandera_blanca_minutos;
	}

	// Proteccion de bandera blanca: durante la ventana SOLO se permite Flag_White
	// en el poste (no se puede poner una bandera de pais). Pasada la ventana, todo
	// vuelve a ser vanilla y se pueden colgar banderas normales.
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (!super.CanReceiveAttachment(attachment, slotId))
			return false;
		string slot_name = InventorySlots.GetSlotName(slotId);
		if (slot_name == "Material_FPole_Flag" && ExorInWhiteFlagWindow())
		{
			if (attachment && attachment.IsKindOf("Flag_White"))
				return true;
			return false;
		}
		return true;
	}

	// Durante la ventana NO se puede sacar la tela (para no eludir la proteccion).
	// Pasada la ventana, se libera (los miembros pueden cambiarla por su bandera).
	override bool CanReleaseAttachment(EntityAI attachment)
	{
		if (attachment && attachment.IsKindOf("Flag_White") && ExorInWhiteFlagWindow())
			return false;
		return super.CanReleaseAttachment(attachment);
	}

	// Al expirar la proteccion: destrabar el slot de la bandera para que se pueda
	// cambiar la tela blanca por una de pais (sin tener que bajarla 40s).
	void ExorWhiteFlagExpire()
	{
		if (!GetGame().IsServer())
			return;
		if (ExorInWhiteFlagWindow())
			return;	// todavia protegida (p.ej. se reprogramo)
		int slotId = InventorySlots.GetSlotIdFromString("Material_FPole_Flag");
		GetInventory().SetSlotLock(slotId, false);

		// Auto-cambiar la blanca por la bandera configurada (si hay una seteada).
		// Borramos la blanca y creamos la nueva diferido (que se libere el slot).
		ExorCfgPartyBandera bcfg = GetExorConfig().party.bandera;
		EntityAI actual = FindAttachmentBySlotName("Material_FPole_Flag");
		if (bcfg.bandera_blanca_cambiar_a != "" && actual && actual.IsKindOf("Flag_White"))
		{
			GetGame().ObjectDelete(actual);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCreateConfiguredFlag, 300, false);
		}

		// avisar a los miembros online
		ExorGroup g = ExorGroupManager.Get().FindById(m_ExorGroupId);
		if (g)
		{
			int i;
			for (i = 0; i < g.members.Count(); i++)
			{
				PlayerBase pb = ExorGroupManager.Get().FindOnline(g.members.Get(i).steamid);
				if (pb)
					pb.MessageImportant("Proteccion de bandera blanca terminada: ya pueden cambiarla por su bandera.");
			}
		}
	}

	// Crea la bandera configurada en el slot (tras haber borrado la blanca).
	void ExorCreateConfiguredFlag()
	{
		if (!GetGame().IsServer())
			return;
		if (FindAttachmentBySlotName("Material_FPole_Flag"))
			return;	// ya hay una bandera colgada
		ExorCfgPartyBandera b = GetExorConfig().party.bandera;
		if (b.bandera_blanca_cambiar_a == "")
			return;
		int slotId = InventorySlots.GetSlotIdFromString("Material_FPole_Flag");
		GetInventory().SetSlotLock(slotId, false);
		EntityAI fl = GetInventory().CreateAttachment(b.bandera_blanca_cambiar_a);
		if (fl)
			ExorAnimateCloth(m_ExorFlagRaised);	// respetar estado izada/bajada
	}

	// Programa el desbloqueo para cuando termine la ventana (o lo hace ya si vencio).
	void ExorScheduleWhiteFlagExpiry()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgPartyBandera b = GetExorConfig().party.bandera;
		if (!b.bandera_blanca)
			return;
		int restanteMin = (m_ExorClaimMinute + b.bandera_blanca_minutos) - ExorTimeUtil.NowMinutes();
		if (restanteMin <= 0)
		{
			ExorWhiteFlagExpire();
			return;
		}
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorWhiteFlagExpire, restanteMin * 60000, false);
	}

	// Si bandera_blanca esta on y estamos en la ventana, cuelga una Flag_White
	// (visual de "bandera blanca"). Solo en el reclamo inicial; corre cuando el
	// poste ya esta construido.
	void ExorEnsureWhiteFlag()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgPartyBandera b = GetExorConfig().party.bandera;
		if (!b.bandera_blanca)
			return;
		if (!ExorInWhiteFlagWindow())
			return;
		if (FindAttachmentBySlotName("Material_FPole_Flag"))
			return;	// ya hay una bandera colgada
		if (!GetConstruction() || !GetConstruction().IsPartConstructed("pole"))
			return;	// el poste todavia no esta armado
		// El slot de la bandera esta LOCKEADO salvo en la fase "bajada" (vanilla:
		// SetSlotLock(..., phase != 1)). Lo destrabamos para poder colgar la blanca.
		int slotId = InventorySlots.GetSlotIdFromString("Material_FPole_Flag");
		GetInventory().SetSlotLock(slotId, false);
		EntityAI fl = GetInventory().CreateAttachment("Flag_White");
		if (fl)
			ExorAnimateCloth(m_ExorFlagRaised);	// dejarla acorde al estado (izada al reclamar)
		Print(string.Format("%1 bandera blanca -> %2", ExorStorageConstants.LOG, fl));
	}

	// ------------------------- init / claim / auto-build -------------------------
	override void EEInit()
	{
		super.EEInit();
		if (!GetGame().IsServer())
			return;
		// KOTH: si este mastil lo esta creando el manager de koth, NO reclama territorio
		// ni se registra como mastil de party (lo configura ExorKoth directamente).
		if (ExorKoth.s_SpawningKothMast)
		{
			m_ExorIsKothMast = true;
			ExorKoth.s_SpawningKothMast = false;
			return;
		}
		if (!GetExorConfig().party.territorio.habilitado)
			return;	// sistema de territorio desactivado: la bandera queda 100% vanilla
		ExorTerritoryManager.Get().RegisterMast(this);
		// Diferido: corre despues de OnStoreLoad. Si es nuevo (sin grupo), reclama.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorPostInit, 1200, false);
	}

	void ExorPostInit()
	{
		if (!GetGame().IsServer())
			return;
		if (m_ExorInitDone)
			return;
		m_ExorInitDone = true;

		// KOTH huerfano (mastil de koth persistido de una sesion anterior tras crash/reinicio):
		// borrarlo. El estado del koth se reinicia en cada arranque, asi que no debe quedar.
		if (m_ExorIsKothMast)
		{
			ExorMarkDisbanding();
			GetGame().ObjectDelete(this);
			return;
		}

		if (m_ExorGroupId != "")
		{
			// Bandera persistida con grupo: re-sincronizar + reprogramar el fin
			// de la proteccion de bandera blanca (o destrabar si ya vencio).
			ExorTerritoryManager.Get().SyncToAll();
			SetSynchDirty();
			ExorScheduleWhiteFlagExpiry();
			// Re-aplicar la animacion de la tela para que coincida con el estado
			// IZADA/BAJADA persistido (si no, tras reiniciar se ve siempre abajo).
			// Reintentos por si la tela todavia no cargo como attachment.
			ExorReapplyFlagVisual();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorReapplyFlagVisual, 2000, false);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorReapplyFlagVisual, 5000, false);
			return;
		}

		// Bandera recien colocada: el que la puso es el jugador mas cercano
		PlayerBase placer = ExorNearestPlayer(10.0);
		if (!placer)
			return;

		// El kit a veces cae DESPUES del deploy (no esta al reclamar), asi que
		// barremos: ahora (manos + piso) y de nuevo a los 2.5 s, 6 s y 10 s.
		ExorDeleteNearbyKits(placer);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanupKitGround, 2500, false);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanupKitGround, 6000, false);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanupKitGround, 10000, false);

		string sid = ExorGroupManager.SteamId(placer);
		if (ExorGroupManager.Get().FindByPlayer(sid))
		{
			placer.MessageImportant("Ya tenés un party/territorio. Borrá la bandera anterior primero.");
			ExorMarkDisbanding();
			GetGame().ObjectDelete(this);
			return;
		}

		ExorAutoBuild(placer);

		ExorGroup g = ExorGroupManager.Get().CreateGroup(placer);
		if (!g)
			return;
		m_ExorGroupId = g.id;
		ExorOnClaimed(ExorTimeUtil.NowMinutes());
		vector p = GetPosition();
		g.mast_x = p[0]; g.mast_y = p[1]; g.mast_z = p[2];
		ExorGroupManager.Get().SaveGroup(g);
		ExorTerritoryManager.Get().SyncToAll();
		// Bandera blanca (si esta activada): colgar Flag_White cuando el poste este listo.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorEnsureWhiteFlag, 1500, false);
		ExorScheduleWhiteFlagExpiry();	// destrabar el slot al terminar la proteccion
		placer.MessageImportant("Territorio reclamado y party creado. Apuntá a un jugador y usá 'Invitar a mi party'.");
	}

	// El item es el kit que arma la bandera (vanilla lo deja como FenceKit/TerritoryFlagKit)
	bool ExorIsMastKit(EntityAI e)
	{
		if (!e)
			return false;
		return e.IsKindOf("TerritoryFlagKit") || e.IsKindOf("FenceKit");
	}

	// Borra el kit que queda al colocar la bandera (inventario del placer + piso).
	void ExorDeleteNearbyKits(PlayerBase placer)
	{
		Print("[3xorVO] === limpieza de kit ===");

		// 1) cualquier kit en TODO el inventario del que la puso (manos incluidas)
		if (placer && placer.GetInventory())
		{
			array<EntityAI> inv = new array<EntityAI>;
			placer.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inv);
			int k;
			for (k = 0; k < inv.Count(); k++)
			{
				EntityAI it = inv.Get(k);
				if (ExorIsMastKit(it))
				{
					Print("[3xorVO] kit en inventario del placer: " + it.GetType() + " -> borrando");
					it.Delete();
				}
			}
		}

		// 2) kit tirado cerca de la bandera (radio amplio)
		array<Object> objects = new array<Object>;
		array<CargoBase> cargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D(GetPosition(), 15.0, objects, cargos);
		int i;
		for (i = 0; i < objects.Count(); i++)
		{
			ItemBase ib = ItemBase.Cast(objects.Get(i));
			if (!ib)
				continue;
			if (ExorIsMastKit(ib))
				GetGame().ObjectDelete(ib);
		}
	}

	// Barrido post-deploy (varias veces): borra el kit del piso Y del inventario
	// del dueno (por si lo levanto antes de que se barriera del piso).
	void ExorCleanupKitGround()
	{
		if (!GetGame().IsServer())
			return;

		// piso
		array<Object> objects = new array<Object>;
		array<CargoBase> cargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D(GetPosition(), 25.0, objects, cargos);
		int i;
		for (i = 0; i < objects.Count(); i++)
		{
			ItemBase ib = ItemBase.Cast(objects.Get(i));
			if (ExorIsMastKit(ib))
			{
				Print("[3xorVO] barrido: borrando kit del piso " + ib.GetType());
				GetGame().ObjectDelete(ib);
			}
		}

		// inventario del dueno (por si lo levanto)
		if (m_ExorGroupId != "")
		{
			ExorGroup g = ExorGroupManager.Get().FindById(m_ExorGroupId);
			if (g)
			{
				PlayerBase owner = ExorGroupManager.Get().FindOnline(g.owner_id);
				if (owner && owner.GetInventory())
				{
					array<EntityAI> inv = new array<EntityAI>;
					owner.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, inv);
					int k;
					for (k = 0; k < inv.Count(); k++)
					{
						if (ExorIsMastKit(inv.Get(k)))
						{
							Print("[3xorVO] barrido: borrando kit del inventario del dueno");
							inv.Get(k).Delete();
						}
					}
				}
			}
		}
	}

	PlayerBase ExorNearestPlayer(float radius)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		PlayerBase best;
		float bestD = radius;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb)
				continue;
			float d = vector.Distance(pb.GetPosition(), GetPosition());
			if (d < bestD)
			{
				bestD = d;
				best = pb;
			}
		}
		return best;
	}

	// Auto-construye todas las partes SIN pedir materiales. Se usa OnPartBuiltServer
	// (no TakeMaterialsServer) asi no intenta consumir troncos/clavos inexistentes.
	// Varias pasadas por las dependencias (base antes que poste).
	void ExorAutoBuild(Man player)
	{
		Construction construction = GetConstruction();
		if (!construction)
			return;
		int pass;
		for (pass = 0; pass < 5; pass++)
		{
			map<string, ref ConstructionPart> parts = construction.GetConstructionParts();
			if (!parts)
				return;
			foreach (string name, ConstructionPart part : parts)
			{
				if (part && !part.IsBuilt())
					OnPartBuiltServer(player, name, AT_BUILD_PART);
			}
		}
		construction.UpdateVisuals();
	}

	// ------------------------- KOTH (mastil de evento) -------------------------
	bool ExorIsKothMast() { return m_ExorIsKothMast; }
	int ExorKothCapturedSmokeIdx() { return m_ExorKothColorIdx; }

	// Prepara este mastil como KOTH: arma el poste (sin materiales), cuelga la bandera
	// (abajo) y deja humo BLANCO (idle). capturedSmokeIdx = humo del color al capturar.
	void ExorKothSetup(int capturedSmokeIdx)
	{
		if (!GetGame().IsServer())
			return;
		m_ExorIsKothMast = true;
		m_ExorKothColorIdx = capturedSmokeIdx;

		// armar el poste. OnPartBuiltServer necesita un Man; con koth puede no haber nadie
		// al lado, asi que se usa el mas cercano o, si no, cualquiera conectado.
		PlayerBase builder = ExorNearestPlayer(3000);
		if (!builder)
			builder = ExorAnyPlayer();
		if (builder)
			ExorAutoBuild(builder);

		// colgar la bandera (destrabar el slot primero)
		if (!FindAttachmentBySlotName("Material_FPole_Flag"))
		{
			int slotId = InventorySlots.GetSlotIdFromString("Material_FPole_Flag");
			GetInventory().SetSlotLock(slotId, false);
			string flagCls = GetExorConfig().koth.clase_bandera;
			if (flagCls != "")
				GetInventory().CreateAttachment(flagCls);
		}

		// bandera abajo al empezar (progreso 0) + humo blanco (idle)
		m_ExorFlagRaised = false;
		ExorAnimateCloth(false);
		ExorKothSetSmoke(0);

		// borrar el kit (TerritoryFlagKit/FenceKit) que puede quedar en el piso al
		// auto-construir el mastil. Varias pasadas por si aparece un tick despues.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanupKitGround, 1000, false);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanupKitGround, 4000, false);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanupKitGround, 9000, false);
	}

	// setea el humo (server) y lo sincroniza a los clientes cercanos
	void ExorKothSetSmoke(int s)
	{
		if (!GetGame().IsServer())
			return;
		m_ExorKothSmoke = s;
		SetSynchDirty();
	}

	// iza la bandera segun el progreso 0..1 (0 = abajo, 1 = arriba del todo)
	void ExorKothRaise(float progress)
	{
		if (!GetGame().IsServer())
			return;
		float ph = 1.0 - progress;
		if (ph < 0)
			ph = 0;
		if (ph > 1)
			ph = 1;
		AnimateFlagEx(ph);
	}

	// primer jugador conectado (fallback para armar el poste del koth)
	PlayerBase ExorAnyPlayer()
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		if (players.Count() > 0)
			return PlayerBase.Cast(players.Get(0));
		return null;
	}

	// CLIENTE: al cambiar el humo sincronizado, (re)crear la particula del color.
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		ExorKothUpdateSmokeFx();
	}

	void ExorKothUpdateSmokeFx()
	{
		if (!GetGame().IsClient())
			return;
		if (m_ExorSmokeFx)
		{
			m_ExorSmokeFx.Stop();
			m_ExorSmokeFx = null;
		}
		if (m_ExorKothSmoke < 0)
			return;
		int pid = ExorKothParticleId(m_ExorKothSmoke);
		vector p = GetPosition();
		p[1] = p[1] + 2.0;
		m_ExorSmokeFx = Particle.PlayInWorld(pid, p);
	}

	int ExorKothParticleId(int s)
	{
		if (s == 1)
			return ParticleList.GRENADE_M18_YELLOW_LOOP;
		if (s == 2)
			return ParticleList.GRENADE_M18_GREEN_LOOP;
		if (s == 3)
			return ParticleList.GRENADE_M18_PURPLE_LOOP;
		return ParticleList.GRENADE_M18_WHITE_LOOP;
	}

	// ------------------------- despawn al disolverse -------------------------
	override void EEDelete(EntityAI parent)
	{
		// cliente: apagar el humo si estaba activo
		if (m_ExorSmokeFx)
		{
			m_ExorSmokeFx.Stop();
			m_ExorSmokeFx = null;
		}
		if (GetGame().IsServer())
		{
			ExorTerritoryManager.Get().UnregisterMast(this);
			// KOTH: nunca disolver party (no tiene grupo). Solo banderas de territorio.
			if (!m_ExorDisbanding && m_ExorGroupId != "" && !m_ExorIsKothMast)
			{
				ExorGroup g = ExorGroupManager.Get().FindById(m_ExorGroupId);
				if (g)
					ExorGroupManager.Get().DisbandGroup(g, "La bandera fue removida: el party se disolvió.");
			}
		}
		super.EEDelete(parent);
	}

	// ------------------------- persistencia -------------------------
	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorGroupId);
		ctx.Write(m_ExorFlagRaised);
		ctx.Write(m_ExorClaimMinute);
		ctx.Write(m_ExorIsKothMast);	// para poder borrar mastiles de koth huerfanos al recargar
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string gid;
		if (!ctx.Read(gid)) return false;
		m_ExorGroupId = gid;
		bool raised;
		if (!ctx.Read(raised)) return false;
		m_ExorFlagRaised = raised;
		int claimMinute;
		if (!ctx.Read(claimMinute)) return false;
		m_ExorClaimMinute = claimMinute;
		// campo NUEVO al final: lectura tolerante (saves viejos no lo tienen -> queda false).
		bool kf;
		if (ctx.Read(kf))
			m_ExorIsKothMast = kf;
		return true;
	}
}
