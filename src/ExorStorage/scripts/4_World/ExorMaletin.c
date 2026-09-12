// ============================================================================
//  3xor_Vanilla_Optimization - EVENTO DEL MALETIN (manager, SOLO server)
// ----------------------------------------------------------------------------
//  Aparece un maletin con humo en un punto de INICIO (sorteado) y hay que
//  llevarlo hasta un punto de FIN (tambien sorteado). El que lo lleva sale
//  marcado en el mapa de TODO el server, no lo puede soltar, y si muere el
//  maletin vuelve al inicio. Al entregarlo cae un cofre con fuegos artificiales.
//
//  ============================ POR QUE NO METE LAG ==========================
//  El evento es UNA entidad (el maletin) mas el cofre del final. No hay nada por
//  frame y el latido es de 2 s; con el evento apagado o en espera, ese latido es
//  una comparacion de enteros y nada mas.
//
//  Lo unico que se repite mientras alguien lleva el maletin es la marca del mapa,
//  y por eso es lo unico con su propio intervalo (segundos_refrescar_marca_portador,
//  15 s por defecto): mandarla cada 2 s serian 30 RPC por jugador por minuto para
//  mover un punto que en 2 s casi no se movio.
//
//  El chequeo anti-campeo (estar adentro de una base) recorre la lista de mastiles
//  y por eso corre cada 5 s y SOLO mientras el maletin esta en viaje.
// ============================================================================
class ExorMaletinEstado
{
	static const int IDLE       = 0;	// esperando la proxima ventana / cooldown
	static const int EN_PISO    = 1;	// maletin + humo en el punto de inicio
	static const int EN_TRANSITO = 2;	// alguien lo esta llevando
	static const int PREMIO     = 3;	// entregado: el cofre esta en el piso
}

class ExorMaletin
{
	static ref ExorMaletin s_Instance;
	// true SOLO mientras el modulo esta creando el maletin del evento. Lo lee el EEInit de
	// la entidad para distinguir "este es el del evento" de "este es una copia que revivio
	// de una tumba" (misma idea que el guard del mastil del KOTH).
	static bool s_Creando;
	static const string TAG = "MALETIN";
	static const int TICK_MS = 2000;
	// ids de las marcas del mapa (el KOTH usa 0..n y el cofre 5000+)
	static const int MARCA_INICIO   = 6000;
	static const int MARCA_FIN      = 6001;
	static const int MARCA_PORTADOR = 6002;

	int m_Estado;
	int m_ProximoIntentoMs;		// uptime ms del proximo arranque posible
	int m_LimiteMs;				// vencimiento de la fase actual
	int m_UltimaMarcaMs;		// ultimo refresco de la marca del portador
	int m_UltimoCampeoMs;		// ultimo chequeo de anti-campeo
	int m_CampeandoMs;			// cuanto lleva acumulado adentro de una base
	int m_LimpiarPremioMs;		// cuando se borra el cofre del premio

	vector m_Inicio;
	vector m_Fin;
	Exor_MaletinEvento m_Maletin;
	Exor_HumoEvento m_HumoFin;	// baliza con humo en el punto de entrega
	PlayerBase m_Portador;
	Object m_Cofre;
	Object m_Fuegos;

	void ExorMaletin()
	{
		m_Estado = ExorMaletinEstado.IDLE;
	}

	static ExorMaletin Get()
	{
		if (!s_Instance)
			s_Instance = new ExorMaletin();
		return s_Instance;
	}

	static ExorCfgMaletin Cfg()
	{
		return GetExorConfig().maletin;
	}

	static int Argb()
	{
		return ARGB(255, 170, 80, 220);	// morado, como el humo del evento
	}

	// ------------------------------------------------------------------------
	//  ARRANQUE
	// ------------------------------------------------------------------------
	static void Start()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgMaletin c = Cfg();
		if (!c || !c.enable)
		{
			Print(string.Format("%1 %2: desactivado (evento_maletin.json enable=false)", ExorStorageConstants.LOG, TAG));
			return;
		}
		if (!c.recorridos || c.recorridos.Count() == 0)
		{
			Print(string.Format("%1 %2: sin recorridos configurados, no arranca", ExorStorageConstants.LOG, TAG));
			return;
		}
		Get().ValidarClassnames();
		// Diferido 25 s: que la persistencia termine de cargar antes de barrer maletines
		// viejos, si no se borrarian los que el motor todavia no cargo.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().LimpiarYArrancar, 25000, false);
		Print(string.Format("%1 %2: %3 recorridos, %4 cofres de premio", ExorStorageConstants.LOG, TAG, c.recorridos.Count(), c.cofres.Count()));
	}

	void LimpiarYArrancar()
	{
		LimpiarHuerfanos();
		m_ProximoIntentoMs = GetGame().GetTime() + 30000;	// medio minuto de gracia y arranca
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(TickTimed, TICK_MS, true);
	}

	// Un maletin del evento que sobrevivio al reinicio no sirve para nada: el estado del
	// evento vive en RAM, asi que ese maletin ya no pertenece a ningun evento. Se barren
	// los puntos de inicio (que es donde pudo quedar uno sin que nadie lo levantara).
	void LimpiarHuerfanos()
	{
		ExorCfgMaletin c = Cfg();
		int borrados = 0;
		int j;
		for (j = 0; j < c.recorridos.Count(); j++)
		{
			ExorCfgMaletinRecorrido r = c.recorridos.Get(j);
			if (!r)
				continue;
			borrados = borrados + BarrerPunto(r.inicio);
			borrados = borrados + BarrerPunto(r.fin);
		}
		if (borrados > 0)
			Print(string.Format("%1 %2: %3 objetos de la sesion anterior borrados", ExorStorageConstants.LOG, TAG, borrados));
	}

	// borra maletines y balizas que quedaron alrededor de un punto configurado
	int BarrerPunto(ExorCfgMaletinCoord punto)
	{
		if (!punto)
			return 0;
		int borrados = 0;
		int k;
		vector pos = PosDe(punto);
		array<Object> objs = new array<Object>;
		array<CargoBase> cargos = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D(pos, 15.0, objs, cargos);
		for (k = 0; k < objs.Count(); k++)
		{
			Object o = objs.Get(k);
			if (!o)
				continue;
			if (Exor_MaletinEvento.Cast(o) || Exor_HumoEvento.Cast(o))
			{
				GetGame().ObjectDelete(o);
				borrados++;
			}
		}
		return borrados;
	}

	// Mismo criterio que el modulo de cofres: los classnames del premio se revisan UNA vez
	// al arrancar, para que un typo se vea en el RPT y no el dia que alguien complete el
	// evento y el cofre salga medio vacio sin explicacion.
	void ValidarClassnames()
	{
		ExorCfgMaletin c = Cfg();
		int malos = 0;
		int i, k, n;
		if (!ExorCofreLoot.ExisteClase(c.classname_maletin))
		{
			Print(string.Format("%1 %2: OJO, el maletin '%3' NO existe en este server", ExorStorageConstants.LOG, TAG, c.classname_maletin));
			malos++;
		}
		if (!ExorCofreLoot.ExisteClase(c.clase_cofre))
		{
			Print(string.Format("%1 %2: OJO, el cofre '%3' NO existe en este server", ExorStorageConstants.LOG, TAG, c.clase_cofre));
			malos++;
		}
		for (i = 0; i < c.cofres.Count(); i++)
		{
			ExorCfgMaletinCofre co = c.cofres.Get(i);
			if (!co || !co.items)
				continue;
			for (k = 0; k < co.items.Count(); k++)
			{
				ExorCfgMaletinItem it = co.items.Get(k);
				if (!it)
					continue;
				if (!ExorCofreLoot.ExisteClase(it.classname))
				{
					Print(string.Format("%1 %2: OJO, el cofre '%3' pide '%4' y esa clase NO existe", ExorStorageConstants.LOG, TAG, co.nombre, it.classname));
					malos++;
				}
				if (!it.attachments)
					continue;
				for (n = 0; n < it.attachments.Count(); n++)
				{
					if (!ExorCofreLoot.ExisteClase(it.attachments.Get(n)))
					{
						Print(string.Format("%1 %2: OJO, el cofre '%3' engancha '%4' y esa clase NO existe", ExorStorageConstants.LOG, TAG, co.nombre, it.attachments.Get(n)));
						malos++;
					}
				}
			}
		}
		if (malos == 0)
			Print(string.Format("%1 %2: classnames del premio verificados, todos existen", ExorStorageConstants.LOG, TAG));
	}

	// ------------------------------------------------------------------------
	//  LATIDO
	// ------------------------------------------------------------------------
	void TickTimed()
	{
		int t = ExorPerfMonitor.Now();
		Tick();
		ExorPerfMonitor.Fin("maletin", t);
	}

	void Tick()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorCfgMaletin c = Cfg();
		if (!c || !c.enable)
			return;
		int now = GetGame().GetTime();

		if (m_Estado == ExorMaletinEstado.IDLE)
		{
			if (now >= m_ProximoIntentoMs)
				IntentarArrancar(c, now);
			return;
		}

		if (m_Estado == ExorMaletinEstado.EN_PISO)
		{
			TickEnPiso(c, now);
			return;
		}

		if (m_Estado == ExorMaletinEstado.EN_TRANSITO)
		{
			TickEnTransito(c, now);
			return;
		}

		if (m_Estado == ExorMaletinEstado.PREMIO)
		{
			if (now >= m_LimpiarPremioMs)
				LimpiarPremio(c, now);
		}
	}

	// ------------------------------------------------------------------------
	//  ARRANQUE DE UN EVENTO
	// ------------------------------------------------------------------------
	void IntentarArrancar(ExorCfgMaletin c, int now)
	{
		// Se reintenta en un minuto en TODAS las negativas: son condiciones que cambian
		// solas (entra gente, pasa el horario, termina el raid), no errores.
		m_ProximoIntentoMs = now + 60000;

		if (c.cantidad_minima_players_online > 0 && ExorTick1Hz.Jugadores().Count() < c.cantidad_minima_players_online)
			return;
		if (c.desactivar_en_horario_raid && ExorMuebleRules.IsLootFreeNow())
			return;
		if (!EnHorario(c))
			return;

		// se sortea el recorrido ENTERO, no cada punto por su lado
		ExorCfgMaletinRecorrido r = c.recorridos.Get(Math.RandomInt(0, c.recorridos.Count()));
		if (!r || !r.inicio || !r.fin)
			return;
		m_Inicio = PosDe(r.inicio);
		m_Fin = PosDe(r.fin);

		if (!CrearMaletin(c))
		{
			Print(string.Format("%1 %2: no se pudo crear el maletin, se reintenta", ExorStorageConstants.LOG, TAG));
			return;
		}

		m_Estado = ExorMaletinEstado.EN_PISO;
		m_LimiteMs = now + (c.minutos_duracion_evento * 60000);
		m_Portador = null;
		m_CampeandoMs = 0;

		CrearHumoFin(c);
		if (c.marcar_en_mapa_inicio_y_fin)
		{
			Marca(MARCA_INICIO, true, m_Inicio, "Maletin");
			Marca(MARCA_FIN, true, m_Fin, "Entrega");
		}
		if (c.avisos_en_chat)
			ExorKoth.Alert("Aparecio el maletin. Llevalo hasta el punto de entrega (los dos estan marcados en el mapa).", 15, Argb());
		Print(string.Format("%1 %2: evento iniciado, inicio %3 -> fin %4", ExorStorageConstants.LOG, TAG, m_Inicio, m_Fin));
	}

	bool CrearMaletin(ExorCfgMaletin c)
	{
		s_Creando = true;
		Object o = GetGame().CreateObjectEx(c.classname_maletin, m_Inicio, ECE_PLACE_ON_SURFACE);
		s_Creando = false;
		m_Maletin = Exor_MaletinEvento.Cast(o);
		if (!m_Maletin)
		{
			if (o)
				GetGame().ObjectDelete(o);
			return false;
		}
		m_Maletin.ExorSetHumo(ExorHumoFx.IdxDe(c.color_humo));
		m_Maletin.m_ExorEnInicio = true;	// unico lugar donde puede estar sin dueño
		return true;
	}

	// Baliza del punto de entrega: el mismo humo que el del maletin, para que los dos
	// extremos del recorrido se vean igual de lejos. Vive todo el evento -tambien cuando el
	// maletin vuelve al inicio- y se borra cuando el maletin llega o el evento se cancela.
	void CrearHumoFin(ExorCfgMaletin c)
	{
		BorrarHumoFin();
		Object o = GetGame().CreateObjectEx("Exor_HumoEvento", m_Fin, ECE_PLACE_ON_SURFACE);
		m_HumoFin = Exor_HumoEvento.Cast(o);
		if (!m_HumoFin)
		{
			if (o)
				GetGame().ObjectDelete(o);
			Print(string.Format("%1 %2: no se pudo crear la baliza del punto de entrega", ExorStorageConstants.LOG, TAG));
			return;
		}
		m_HumoFin.ExorSetHumo(ExorHumoFx.IdxDe(c.color_humo));
	}

	void BorrarHumoFin()
	{
		if (m_HumoFin)
		{
			GetGame().ObjectDelete(m_HumoFin);
			m_HumoFin = null;
		}
	}

	// ------------------------------------------------------------------------
	//  FASE 1: EN EL PISO
	// ------------------------------------------------------------------------
	void TickEnPiso(ExorCfgMaletin c, int now)
	{
		if (!m_Maletin)
		{
			Cancelar(c, now, "El maletin se perdio.");
			return;
		}
		if (now >= m_LimiteMs)
			Cancelar(c, now, "Nadie fue a buscar el maletin. El evento se cancelo.");
	}

	// Lo levantaron. El humo se apaga solo (lo emite el maletin) y arranca la cuenta atras.
	void OnTomado(ExorCfgMaletin c, PlayerBase quien, int now)
	{
		m_Portador = quien;
		m_Estado = ExorMaletinEstado.EN_TRANSITO;
		m_LimiteMs = now + (c.minutos_para_ir_desde_inicio_al_final * 60000);
		m_CampeandoMs = 0;
		m_UltimaMarcaMs = 0;
		m_UltimoCampeoMs = now;
		m_Maletin.ExorSetHumo(0);
		m_Maletin.m_ExorEnInicio = false;	// de aca en mas solo vale estar encima de su dueño
		Marca(MARCA_INICIO, false, m_Inicio, "");
		if (c.avisos_en_chat)
			ExorKoth.Alert(string.Format("%1 agarro el maletin. Va marcado en el mapa: %2 minutos para entregarlo.", NombreDe(quien), c.minutos_para_ir_desde_inicio_al_final), 15, Argb());
		Print(string.Format("%1 %2: %3 tomo el maletin", ExorStorageConstants.LOG, TAG, NombreDe(quien)));
	}

	// ------------------------------------------------------------------------
	//  FASE 2: EN VIAJE
	// ------------------------------------------------------------------------
	void TickEnTransito(ExorCfgMaletin c, int now)
	{
		// murio o se desconecto -> el maletin vuelve al inicio
		if (!m_Maletin || !m_Portador || !m_Portador.IsAlive())
		{
			VolverAlInicio(c, now, "El que llevaba el maletin cayo. El maletin volvio al punto de inicio.");
			return;
		}
		// sigue vivo pero el maletin no lo tiene el: lo solto, lo metio en un auto, o solto
		// la mochila donde lo llevaba. Se le devuelve: del maletin no se puede zafar.
		if (PortadorReal() != m_Portador)
		{
			DevolverAlPortador();
			return;
		}

		if (now >= m_LimiteMs)
		{
			Cancelar(c, now, "Se acabo el tiempo para entregar el maletin. El evento se cancelo.");
			return;
		}

		vector pos = m_Portador.GetPosition();

		// llegada
		float r = c.metros_para_entregar;
		if (ExorMath.Dist2DSq(pos, m_Fin) <= (r * r))
		{
			Entregar(c, now);
			return;
		}

		// marca del mapa que se mueve (su propio intervalo: ver la cabecera del archivo)
		if (c.marcar_en_mapa_global_player_con_maletin && now - m_UltimaMarcaMs >= (c.segundos_refrescar_marca_portador * 1000))
		{
			m_UltimaMarcaMs = now;
			Marca(MARCA_PORTADOR, true, pos, "Maletin: " + NombreDe(m_Portador));
		}

		// anti-campeo adentro de una base
		if (c.activar_muerte_por_permanecer_cerca_mastil_base && now - m_UltimoCampeoMs >= 5000)
		{
			int transcurrido = now - m_UltimoCampeoMs;
			m_UltimoCampeoMs = now;
			RevisarCampeo(c, pos, transcurrido);
		}
	}

	// El que lleva el maletin no puede quedarse metido adentro de una base esperando que
	// se acabe el reloj: si se queda cerca de un mastil el tiempo configurado, se muere.
	void RevisarCampeo(ExorCfgMaletin c, vector pos, int transcurrido)
	{
		if (!CercaDeMastil(pos, c.metros_para_morir_cercania_mastil))
		{
			m_CampeandoMs = 0;
			return;
		}
		m_CampeandoMs = m_CampeandoMs + transcurrido;
		int limite = c.minutos_morir_por_cercania_mastil * 60000;
		int faltan = (limite - m_CampeandoMs) / 1000;
		// un solo aviso, a la mitad del tiempo: alcanza para que se entere y se vaya
		if (m_CampeandoMs >= (limite / 2) && m_CampeandoMs - transcurrido < (limite / 2))
			ExorAviso.Error(m_Portador, string.Format("Estas campeando en una base con el maletin. Salite o vas a morir en %1 segundos.", faltan));
		if (m_CampeandoMs >= limite)
		{
			ExorAviso.Error(m_Portador, "Te quedaste en una base con el maletin.");
			m_Portador.SetHealth("", "", 0);
			if (c.avisos_en_chat)
				ExorKoth.Alert(string.Format("%1 murio por quedarse con el maletin adentro de una base.", NombreDe(m_Portador)), 12, Argb());
			// el resto (maletin de vuelta al inicio) lo resuelve el proximo tick al ver
			// que el portador ya no esta vivo
		}
	}

	bool CercaDeMastil(vector pos, float metros)
	{
		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		if (!tm || !tm.m_Masts)
			return false;
		float r2 = metros * metros;
		int i;
		for (i = 0; i < tm.m_Masts.Count(); i++)
		{
			TerritoryFlag m = tm.m_Masts.Get(i);
			if (!m)
				continue;
			if (ExorMath.Dist2DSq(pos, m.GetPosition()) <= r2)
				return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------
	//  FINALES
	// ------------------------------------------------------------------------
	void Entregar(ExorCfgMaletin c, int now)
	{
		string quien = NombreDe(m_Portador);
		BorrarMaletin();
		BorrarHumoFin();	// llego el maletin: la baliza del punto de entrega ya no hace falta
		OcultarMarcas();

		int puestos = CrearPremio(c);
		m_Estado = ExorMaletinEstado.PREMIO;
		m_LimpiarPremioMs = now + (c.minutos_despawn_cofre_premio * 60000);
		m_ProximoIntentoMs = now + (c.minutos_para_repetir_evento * 60000);

		if (c.avisos_en_chat)
			ExorKoth.Alert(string.Format("%1 entrego el maletin. El premio esta en el punto de entrega.", quien), 17, Argb());
		Print(string.Format("%1 %2: entregado por %3 (%4 items en el cofre)", ExorStorageConstants.LOG, TAG, quien, puestos));
	}

	int CrearPremio(ExorCfgMaletin c)
	{
		vector pos = m_Fin;
		m_Cofre = GetGame().CreateObjectEx(c.clase_cofre, pos, ECE_PLACE_ON_SURFACE);
		if (!m_Cofre)
		{
			Print(string.Format("%1 %2: no se pudo crear el cofre del premio ('%3')", ExorStorageConstants.LOG, TAG, c.clase_cofre));
			return 0;
		}
		Barrel_ColorBase brl = Barrel_ColorBase.Cast(m_Cofre);
		if (brl)
			brl.Open();	// el barril cerrado esconde el cargo (mismo caso que el pallet del KOTH)

		if (c.clase_fuegos_artificiales != "")
		{
			vector fpos = pos + Vector(c.metros_fuegos_lejos_del_cofre, 0, 0);
			fpos[1] = GetGame().SurfaceY(fpos[0], fpos[2]);
			m_Fuegos = GetGame().CreateObjectEx(c.clase_fuegos_artificiales, fpos, ECE_PLACE_ON_SURFACE);
			FireworksBase fb = FireworksBase.Cast(m_Fuegos);
			if (fb)
				fb.ExorKothIgnite();
		}

		ExorCfgMaletinCofre tabla = c.SortearCofre();
		if (!tabla)
			return 0;
		EntityAI cont = EntityAI.Cast(m_Cofre);
		int puestos = 0;
		int i, n, k;
		for (i = 0; i < tabla.items.Count(); i++)
		{
			ExorCfgMaletinItem it = tabla.items.Get(i);
			if (!it || it.classname == "")
				continue;
			if (it.probabilidad < 100 && Math.RandomInt(0, 100) >= it.probabilidad)
				continue;
			int cuantos = it.cantidad;
			if (cuantos < 1)
				cuantos = 1;
			for (n = 0; n < cuantos; n++)
			{
				EntityAI e = cont.GetInventory().CreateInInventory(it.classname);
				if (!e)
				{
					Print(string.Format("%1 %2: '%3' NO entro al cofre del premio", ExorStorageConstants.LOG, TAG, it.classname));
					continue;
				}
				puestos++;
				if (!it.attachments)
					continue;
				for (k = 0; k < it.attachments.Count(); k++)
				{
					string att = it.attachments.Get(k);
					if (att == "")
						continue;
					EntityAI a = e.GetInventory().CreateAttachment(att);
					if (!a)
						a = cont.GetInventory().CreateInInventory(att);
					if (a)
						puestos++;
				}
			}
		}
		Print(string.Format("%1 %2: premio '%3' con %4 items", ExorStorageConstants.LOG, TAG, tabla.nombre, puestos));
		return puestos;
	}

	void LimpiarPremio(ExorCfgMaletin c, int now)
	{
		if (m_Cofre)
		{
			GetGame().ObjectDelete(m_Cofre);
			m_Cofre = null;
		}
		if (m_Fuegos)
		{
			GetGame().ObjectDelete(m_Fuegos);
			m_Fuegos = null;
		}
		m_Estado = ExorMaletinEstado.IDLE;
		if (m_ProximoIntentoMs < now)
			m_ProximoIntentoMs = now;
	}

	// El portador cayo: el maletin vuelve al punto de inicio, con su humo, y el evento
	// sigue corriendo (no se cancela: la idea es que otro lo agarre).
	void VolverAlInicio(ExorCfgMaletin c, int now, string aviso)
	{
		BorrarMaletin();
		Marca(MARCA_PORTADOR, false, m_Inicio, "");
		m_Portador = null;
		m_CampeandoMs = 0;
		if (!CrearMaletin(c))
		{
			Cancelar(c, now, "El maletin se perdio.");
			return;
		}
		m_Estado = ExorMaletinEstado.EN_PISO;
		m_LimiteMs = now + (c.minutos_duracion_evento * 60000);
		if (c.marcar_en_mapa_inicio_y_fin)
			Marca(MARCA_INICIO, true, m_Inicio, "Maletin");
		if (c.avisos_en_chat && aviso != "")
			ExorKoth.Alert(aviso, 15, Argb());
		Print(string.Format("%1 %2: el maletin volvio al inicio", ExorStorageConstants.LOG, TAG));
	}

	void Cancelar(ExorCfgMaletin c, int now, string aviso)
	{
		BorrarMaletin();
		BorrarHumoFin();
		OcultarMarcas();
		m_Portador = null;
		m_Estado = ExorMaletinEstado.IDLE;
		m_ProximoIntentoMs = now + (c.minutos_para_repetir_evento * 60000);
		if (c.avisos_en_chat && aviso != "")
			ExorKoth.Alert(aviso, 12, Argb());
		Print(string.Format("%1 %2: evento cancelado (%3)", ExorStorageConstants.LOG, TAG, aviso));
	}

	void BorrarMaletin()
	{
		if (m_Maletin)
		{
			GetGame().ObjectDelete(m_Maletin);
			m_Maletin = null;
		}
	}

	// ⭐ Lo llama EEKilled del jugador, no el latido. Si esto se dejara para el tick, dos
	// segundos despues el cuerpo ya es una tumba con el maletin adentro, y como el contenido
	// de la tumba se virtualiza a JSON, borrar la entidad ya no alcanza: al abrir la tumba
	// el restore lo vuelve a crear. Por eso el maletin se borra en el mismo instante de la
	// muerte, antes de que el cuerpo se convierta en nada.
	void OnPortadorMuerto(PlayerBase p)
	{
		if (m_Estado != ExorMaletinEstado.EN_TRANSITO)
			return;
		if (!p || p != m_Portador)
			return;
		ExorCfgMaletin c = Cfg();
		if (!c || !c.enable)
			return;
		VolverAlInicio(c, GetGame().GetTime(), "El que llevaba el maletin cayo. El maletin volvio al punto de inicio.");
	}

	// ------------------------------------------------------------------------
	//  MOVIMIENTOS DEL MALETIN (los avisa la entidad)
	// ------------------------------------------------------------------------
	// Una sola puerta de entrada para todo: lo levantaron, lo tiraron, lo pasaron a otro,
	// o el motor lo metio en la bolsa de cadaver al morir el que lo llevaba.
	void OnMaletinMovido(Exor_MaletinEvento maletin)
	{
		if (maletin != m_Maletin)
			return;	// un maletin que no es el del evento en curso: no es asunto nuestro
		ExorCfgMaletin c = Cfg();
		if (!c || !c.enable)
			return;
		int now = GetGame().GetTime();
		PlayerBase quien = PortadorReal();

		if (m_Estado == ExorMaletinEstado.EN_PISO)
		{
			if (quien)
				OnTomado(c, quien, now);
			return;
		}

		if (m_Estado != ExorMaletinEstado.EN_TRANSITO)
			return;

		if (quien == m_Portador)
			return;	// lo movio dentro de su propio inventario: todo bien

		// Lo solto, se lo sacaron, o cayo a la bolsa del cadaver: una sola resolucion.
		OnMaletinSinDuenio(maletin);
	}

	// Lo llama la entidad cuando quedo en un lugar donde NO puede estar (una tumba, un
	// contenedor en el piso, el inventario de un muerto). Es la red de seguridad del hook
	// de muerte: cubre lo que ese hook no ve, como una desconexion.
	void OnMaletinSinDuenio(Exor_MaletinEvento maletin)
	{
		if (maletin != m_Maletin || m_Estado != ExorMaletinEstado.EN_TRANSITO)
			return;
		ExorCfgMaletin c = Cfg();
		if (!c || !c.enable)
			return;
		if (m_Portador && m_Portador.IsAlive())
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DevolverAlPortador, 1, false);
			return;
		}
		VolverAlInicio(c, GetGame().GetTime(), "El que llevaba el maletin cayo. El maletin volvio al punto de inicio.");
	}

	void DevolverAlPortador()
	{
		if (!m_Maletin || !m_Portador || !m_Portador.IsAlive())
			return;
		if (PortadorReal() == m_Portador)
			return;	// ya volvio solo
		if (m_Portador.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY, m_Maletin))
			ExorAviso.Error(m_Portador, "El maletin no se puede soltar: hay que entregarlo.");
		else
			VolverAlInicio(Cfg(), GetGame().GetTime(), "El maletin se cayo y volvio al punto de inicio.");
	}

	// Jugador VIVO que tiene el maletin en su inventario ahora mismo (null si esta en el
	// piso, en un contenedor o en una bolsa de cadaver).
	PlayerBase PortadorReal()
	{
		if (!m_Maletin)
			return null;
		PlayerBase p = PlayerBase.Cast(m_Maletin.GetHierarchyRootPlayer());
		if (p && p.IsAlive())
			return p;
		return null;
	}

	// ------------------------------------------------------------------------
	//  HORARIO / MARCAS / VARIOS
	// ------------------------------------------------------------------------
	bool EnHorario(ExorCfgMaletin c)
	{
		if (!c.horarios || c.horarios.Count() == 0)
			return true;	// sin horarios = a cualquier hora
		int minOfDay, weekday, dayKey;
		ExorCofre.NowLocal(c.offset_horas, minOfDay, weekday, dayKey);
		int i;
		for (i = 0; i < c.horarios.Count(); i++)
		{
			ExorCfgMaletinHorario h = c.horarios.Get(i);
			if (!h || !h.activado)
				continue;
			if (!ExorMuebleRules.DayMatches(h.dia, weekday))
				continue;
			int desde = ExorMuebleRules.ParseHHMM(h.hora_inicio);
			int hasta = ExorMuebleRules.ParseHHMM(h.hora_fin);
			if (desde < 0 || hasta < 0)
				continue;
			if (hasta >= desde)
			{
				if (minOfDay >= desde && minOfDay <= hasta)
					return true;
			}
			else	// cruza medianoche
			{
				if (minOfDay >= desde || minOfDay <= hasta)
					return true;
			}
		}
		return false;
	}

	// Apaga las TRES marcas del evento de una. Se llama en los tres finales -entregado,
	// cancelado, y el portador que cae- para que nadie quede marcado en el mapa de los
	// demas cuando el evento ya no existe. Ir apagandolas de a una por cada final es
	// justamente como se olvida una.
	void OcultarMarcas()
	{
		Marca(MARCA_PORTADOR, false, m_Fin, "");
		Marca(MARCA_INICIO, false, m_Inicio, "");
		Marca(MARCA_FIN, false, m_Fin, "");
	}

	void Marca(int id, bool mostrar, vector pos, string label)
	{
		ExorKothMarkerDTO d = new ExorKothMarkerDTO();
		d.show = mostrar;
		d.id = id;
		d.x = pos[0];
		d.y = pos[1];
		d.z = pos[2];
		d.argb = Argb();
		d.label = label;
		ExorKoth.BroadcastMarker(d);
	}

	// Al conectarse alguien, que vea las marcas del evento en curso.
	void SyncMarkersToPlayer(PlayerBase pb)
	{
		if (!pb || !pb.GetIdentity())
			return;
		ExorCfgMaletin c = Cfg();
		if (!c || !c.enable || m_Estado == ExorMaletinEstado.IDLE)
			return;
		if (c.marcar_en_mapa_inicio_y_fin)
		{
			if (m_Estado == ExorMaletinEstado.EN_PISO)
				MarcaA(pb, MARCA_INICIO, m_Inicio, "Maletin");
			MarcaA(pb, MARCA_FIN, m_Fin, "Entrega");
		}
		if (c.marcar_en_mapa_global_player_con_maletin && m_Estado == ExorMaletinEstado.EN_TRANSITO && m_Portador)
			MarcaA(pb, MARCA_PORTADOR, m_Portador.GetPosition(), "Maletin: " + NombreDe(m_Portador));
	}

	void MarcaA(PlayerBase pb, int id, vector pos, string label)
	{
		ExorKothMarkerDTO d = new ExorKothMarkerDTO();
		d.show = true;
		d.id = id;
		d.x = pos[0];
		d.y = pos[1];
		d.z = pos[2];
		d.argb = Argb();
		d.label = label;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(d, false, data);
		pb.RPCSingleParam(ExorRPC.KOTH_SYNC, new Param1<string>(data), true, pb.GetIdentity());
	}

	vector PosDe(ExorCfgMaletinCoord c)
	{
		vector pos = Vector(c.x, c.y, c.z);
		if (c.y <= 0)
			pos[1] = GetGame().SurfaceY(c.x, c.z);
		return pos;
	}

	static string NombreDe(PlayerBase p)
	{
		if (p && p.GetIdentity())
			return p.GetIdentity().GetName();
		return "alguien";
	}
}
