// ============================================================================
//  3xor_Vanilla_Optimization - COFRES DE LOOT (manager, SOLO server)
// ----------------------------------------------------------------------------
//  Cofres camuflados fijos en el mapa (cofres_loot.json) para enriquecer el loot
//  de ciertas zonas. Cada POSICION del JSON tiene, como mucho, UN cofre vivo. El
//  cofre no se agarra ni se mueve: se abre a golpes de melee o a tiros, y ahi
//  aparece el loot de la TABLA que le toco.
//
//  ============================ POR QUE NO METE LAG ==========================
//  Son las cuatro decisiones del diseño, y cada una ataca un costo distinto:
//
//  1) EL LOOT SE CREA AL ABRIR. Un cofre cerrado es UNA entidad. Si el loot se
//     creara al spawnear, 30 cofres serian ~400 entidades quietas que el server
//     sincroniza, guarda en la persistencia y limpia sin que nadie las mire.
//
//  2) EL LATIDO ES LENTO Y ARITMETICO. Un tick cada 30 s que recorre el array de
//     posiciones comparando enteros (uptime en ms). No hay busquedas de objetos,
//     ni distancias, ni lecturas de disco en el camino normal.
//
//  3) LO CARO ESTA RACIONADO. Crear cofres se limita a N por ronda
//     (maximo_cofres_spawneados_por_chequeo), asi un arranque con 40 posiciones
//     no crea 40 entidades en el mismo frame: se reparten en varias rondas de 30 s.
//     Mismo patron que el presupuesto por tick de los contenedores.
//
//  4) LAS CONSULTAS AL MUNDO SE EVITAN. Lo unico que mira el mundo en el camino
//     normal es "hay un jugador cerca", y sale del cache de jugadores compartido
//     del latido de 1 Hz. El unico escaneo de verdad es la limpieza de huerfanos,
//     que corre UNA vez por arranque.
//
//  EL CUPO: las posiciones son un ARRAY y 'cantidad_cofres_a_spawnear' dice cuantos
//  cofres hay A LA VEZ entre todas. Con 3 posiciones y cupo 2 hay siempre 2 cofres,
//  y cada vez que se consume uno el siguiente cae en otra de las posiciones: rotan
//  solos y nadie se queda esperando parado en un punto fijo.
//
//  La entidad (Exor_CofreLoot) no tiene tick propio: solo reacciona a EEHitBy.
// ============================================================================

// Estado en vivo de UNA posicion del JSON.
class ExorCofreLootPunto
{
	int m_Idx;					// indice en cofres_loot.posiciones
	Exor_CofreLoot m_Cofre;		// cofre vivo en este punto (null = vacio)
	int m_ProximoIntentoMs;		// uptime ms del proximo intento de spawn
	ref array<Object> m_Zombies;	// la guardia de ESTE cofre (se va y vuelve con los jugadores)

	void ExorCofreLootPunto(int idx)
	{
		m_Idx = idx;
		m_Cofre = null;
		m_ProximoIntentoMs = 0;
		m_Zombies = new array<Object>;
	}
}

class ExorCofreLoot
{
	static ref ExorCofreLoot s_Instance;
	static const string TAG = "COFRE-LOOT";
	// Dedup de impactos del MISMO disparo: los perdigones de una escopeta llegan como
	// varios EEHitBy en el mismo frame y contarian como varios tiros. 50 ms corta eso y
	// deja pasar hasta 1200 disparos por minuto (mas que cualquier automatica del juego).
	static const int DEDUP_MS = 50;

	ref array<ref ExorCofreLootPunto> m_Puntos;
	bool m_Arrancado;
	int m_ProximoSpawnMs;	// cooldown GLOBAL: uptime ms hasta que se puede volver a sembrar

	void ExorCofreLoot()
	{
		m_Puntos = new array<ref ExorCofreLootPunto>;
	}

	static ExorCofreLoot Get()
	{
		if (!s_Instance)
			s_Instance = new ExorCofreLoot();
		return s_Instance;
	}

	static ExorCfgCofreLoot Cfg()
	{
		return GetExorConfig().cofres_loot;
	}

	// ------------------------------------------------------------------------
	//  ARRANQUE
	// ------------------------------------------------------------------------
	static void Start()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgCofreLoot c = Cfg();
		if (!c || !c.enable)
		{
			Print(string.Format("%1 %2: desactivado (cofres_loot.json enable=false)", ExorStorageConstants.LOG, TAG));
			return;
		}
		if (!c.posiciones || c.posiciones.Count() == 0)
		{
			Print(string.Format("%1 %2: sin posiciones configuradas, no arranca", ExorStorageConstants.LOG, TAG));
			return;
		}
		int i;
		for (i = 0; i < c.posiciones.Count(); i++)
			Get().m_Puntos.Insert(new ExorCofreLootPunto(i));
		Get().m_Arrancado = true;
		Get().ValidarClassnames();
		// Diferido 20 s: la persistencia todavia esta cargando entidades en los primeros
		// segundos y la limpieza tiene que ver el mundo ya armado para no dejar huerfanos.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().LimpiarYArrancar, 20000, false);
		Print(string.Format("%1 %2: %3 posiciones, %4 cofres a la vez, %5 tablas de loot, respawn %6 min, siembra a %7 m", ExorStorageConstants.LOG, TAG, c.posiciones.Count(), c.Cupo(), c.tipos.Count(), c.minutos_re_spawn, c.metros_para_spawnear_cofre));
	}

	// Revisa UNA vez, al arrancar, que todos los classnames de las tablas existan de verdad
	// en la config del juego. Un typo (o un mod que se saco del server) no da ningun error
	// por si solo: el item simplemente no aparece el dia que alguien abre el cofre, meses
	// despues y sin que nadie sepa por que. Esto lo deja escrito en el RPT el primer minuto.
	void ValidarClassnames()
	{
		ExorCfgCofreLoot c = Cfg();
		if (!c.tipos)
			return;
		int malos = 0;
		int i, k, n;
		for (i = 0; i < c.tipos.Count(); i++)
		{
			ExorCfgCofreLootTipo t = c.tipos.Get(i);
			if (!t || !t.items)
				continue;
			for (k = 0; k < t.items.Count(); k++)
			{
				ExorCfgCofreLootItem it = t.items.Get(k);
				if (!it)
					continue;
				if (!ExisteClase(it.classname))
				{
					Print(string.Format("%1 %2: OJO, la tabla '%3' pide '%4' y esa clase NO existe en este server", ExorStorageConstants.LOG, TAG, t.nombre, it.classname));
					malos++;
				}
				if (!it.attachments)
					continue;
				for (n = 0; n < it.attachments.Count(); n++)
				{
					if (!ExisteClase(it.attachments.Get(n)))
					{
						Print(string.Format("%1 %2: OJO, la tabla '%3' le engancha '%4' a '%5' y esa clase NO existe", ExorStorageConstants.LOG, TAG, t.nombre, it.attachments.Get(n), it.classname));
						malos++;
					}
				}
			}
		}
		malos = malos + ValidarZombies(c.clase_zombie, "la lista de la raiz");
		for (i = 0; i < c.tipos.Count(); i++)
		{
			ExorCfgCofreLootTipo tz = c.tipos.Get(i);
			if (tz)
				malos = malos + ValidarZombies(tz.clase_zombie, "la tabla '" + tz.nombre + "'");
		}
		if (malos == 0)
			Print(string.Format("%1 %2: classnames de las %3 tablas verificados, todos existen", ExorStorageConstants.LOG, TAG, c.tipos.Count()));
	}

	int ValidarZombies(TStringArray clases, string donde)
	{
		if (!clases)
			return 0;
		int malos = 0;
		int i;
		for (i = 0; i < clases.Count(); i++)
		{
			if (!ExisteClase(clases.Get(i)))
			{
				Print(string.Format("%1 %2: OJO, %3 pide el infectado '%4' y esa clase NO existe en este server", ExorStorageConstants.LOG, TAG, donde, clases.Get(i)));
				malos++;
			}
		}
		return malos;
	}

	static bool ExisteClase(string cls)
	{
		if (cls == "")
			return false;
		if (GetGame().ConfigIsExisting("CfgVehicles " + cls))
			return true;
		if (GetGame().ConfigIsExisting("CfgWeapons " + cls))
			return true;
		return GetGame().ConfigIsExisting("CfgMagazines " + cls);
	}

	// Borra los cofres que quedaron de la sesion anterior (la persistencia los guarda como
	// cualquier contenedor) y recien ahi arranca el latido. Es el UNICO escaneo del mundo
	// que hace el modulo, y corre una sola vez por arranque.
	void LimpiarYArrancar()
	{
		LimpiarHuerfanos();
		int ms = Cfg().segundos_entre_chequeos * 1000;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(TickTimed, ms, true);
	}

	void LimpiarHuerfanos()
	{
		ExorCfgCofreLoot c = Cfg();
		int borrados = 0;
		int i, k;
		for (i = 0; i < c.posiciones.Count(); i++)
		{
			ExorCfgCofreLootPos p = c.posiciones.Get(i);
			if (!p)
				continue;
			vector pos = PosDe(p);
			array<Object> objs = new array<Object>;
			array<CargoBase> cargos = new array<CargoBase>;
			GetGame().GetObjectsAtPosition3D(pos, 10.0, objs, cargos);
			for (k = 0; k < objs.Count(); k++)
			{
				Exor_CofreLoot viejo = Exor_CofreLoot.Cast(objs.Get(k));
				if (viejo)
				{
					GetGame().ObjectDelete(viejo);
					borrados++;
				}
			}
		}
		if (borrados > 0)
			Print(string.Format("%1 %2: %3 cofres de la sesion anterior borrados al arrancar", ExorStorageConstants.LOG, TAG, borrados));
	}

	// ------------------------------------------------------------------------
	//  LATIDO
	// ------------------------------------------------------------------------
	// Envoltorio cronometrado (mismo patron que KOTH/COFRE): mide cuanto tarda el
	// subsistema y solo escribe en el log si se pasa del umbral. Ver ExorPerfMonitor.
	void TickTimed()
	{
		int t = ExorPerfMonitor.Now();
		Tick();
		ExorPerfMonitor.Fin("cofreloot", t);
	}

	void Tick()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorCfgCofreLoot c = Cfg();
		if (!c || !c.enable)
			return;

		int now = GetGame().GetTime();

		// 1) los que ya estan: ver si toca sacarlos, y de paso contar cuantos hay vivos
		int vivos = 0;
		int i;
		for (i = 0; i < m_Puntos.Count(); i++)
		{
			ExorCofreLootPunto pt = m_Puntos.Get(i);
			if (!pt || !pt.m_Cofre)
				continue;
			RevisarCofreVivo(c, pt, now);
			if (pt.m_Cofre)
				vivos++;
		}

		// 2) sembrar hasta llenar el CUPO del array (no una por posicion)
		int cupo = c.Cupo();
		if (vivos >= cupo)
			return;
		if (now < m_ProximoSpawnMs)
			return;	// cooldown global: tras consumirse un cofre se esperan los minutos de respawn

		int creados = 0;
		int faltan = cupo - vivos;
		while (creados < faltan && creados < c.maximo_cofres_spawneados_por_chequeo)
		{
			ExorCofreLootPunto elegido = ElegirPuntoLibre(now);
			if (!elegido)
				break;	// no quedan posiciones disponibles en esta ronda
			if (!Intentar(c, elegido, now))
				break;	// no se pudo (raid, jugador cerca, dado, tabla mala): se reintenta despues
			creados++;
		}
	}

	// Una posicion libre AL AZAR entre las que estan disponibles. Al azar y no por orden:
	// con cupo 2 sobre 3 posiciones, recorrer el array siempre de arriba a abajo dejaria a
	// la ultima sin usarse casi nunca, y los cofres quedarian siempre en los mismos dos
	// puntos, que es justo lo que el cupo viene a evitar.
	ExorCofreLootPunto ElegirPuntoLibre(int now)
	{
		array<ExorCofreLootPunto> libres = new array<ExorCofreLootPunto>;
		int i;
		for (i = 0; i < m_Puntos.Count(); i++)
		{
			ExorCofreLootPunto pt = m_Puntos.Get(i);
			if (!pt || pt.m_Cofre)
				continue;
			if (now < pt.m_ProximoIntentoMs)
				continue;	// esta posicion todavia esta en cooldown (recien tuvo cofre)
			libres.Insert(pt);
		}
		if (libres.Count() == 0)
			return null;
		return libres.Get(Math.RandomInt(0, libres.Count()));
	}

	// Un cofre ya abierto se va cuando lo vaciaron, o cuando se vencio su plazo. Los
	// cerrados no se tocan: esperan a que alguien los reviente.
	void RevisarCofreVivo(ExorCfgCofreLoot c, ExorCofreLootPunto pt, int now)
	{
		Exor_CofreLoot cofre = pt.m_Cofre;
		if (!cofre.m_ExorAbierto)
			return;
		bool vacio = ExorContainerOps.CargoCount(cofre) == 0;
		bool vencido = cofre.m_ExorBorrarEnMs > 0 && now >= cofre.m_ExorBorrarEnMs;
		if (vacio || vencido)
			GetGame().ObjectDelete(cofre);	// EEDelete libera el punto y programa el respawn
	}

	// ------------------------------------------------------------------------
	//  SPAWN
	// ------------------------------------------------------------------------
	// true si se creo un cofre (consume cupo de la ronda).
	bool Intentar(ExorCfgCofreLoot c, ExorCofreLootPunto pt, int now)
	{
		ExorCfgCofreLootPos p = c.posiciones.Get(pt.m_Idx);
		if (!p)
			return false;

		// En horario de raid no se siembra nada nuevo (el horario sale de raid.json, que es
		// la fuente unica del mod). Se reintenta en un minuto, no se pierde la ronda.
		if (c.no_spawnear_cofres_en_horario_raid && ExorMuebleRules.IsLootFreeNow())
		{
			pt.m_ProximoIntentoMs = now + 60000;
			return false;
		}

		vector pos = PosDe(p);

		// NADIE CERCA = NO SE SIEMBRA. Es la regla que hace que tener 50 coordenadas por el
		// mapa no cueste nada: el cofre, su humo, su luz y sus infectados existen solo
		// mientras haya alguien a la distancia configurada. Se reintenta en la proxima ronda
		// sin gastar el cooldown de respawn (todavia no paso nada).
		if (c.metros_para_spawnear_cofre > 0 && !HayJugadorCerca(pos, c.metros_para_spawnear_cofre))
			return false;

		// el dado de la posicion: si sale que no, esta ronda queda vacia
		pt.m_ProximoIntentoMs = now + (c.minutos_re_spawn * 60000);
		if (p.probabilidad_spawn < 100 && Math.RandomInt(0, 100) >= p.probabilidad_spawn)
			return false;

		string tipo = ElegirTipo(p);
		if (tipo == "")
		{
			Print(string.Format("%1 %2: la posicion %3 no tiene tipos validos, no spawnea", ExorStorageConstants.LOG, TAG, pt.m_Idx));
			return false;
		}

		Exor_CofreLoot cofre = Crear(pos, p.y);
		if (!cofre)
			return false;

		cofre.m_ExorPunto = pt.m_Idx;
		cofre.m_ExorTipo = tipo;
		cofre.ExorSetFx(c.CodigoFx());	// humo + luz, en un solo entero sincronizado
		pt.m_Cofre = cofre;
		PonerGuardia(c, pt);	// la guardia nace con el cofre, y solo esta vez
		Print(string.Format("%1 %2: cofre spawneado en %3 (posicion %4, tabla '%5')", ExorStorageConstants.LOG, TAG, pos, pt.m_Idx, tipo));
		return true;
	}

	Exor_CofreLoot Crear(vector pos, float yConfig)
	{
		Object o;
		if (yConfig > 0)
		{
			// altura EXACTA pedida por el admin (pisos, techos, interiores): sin snap al
			// terreno, que lo dejaria abajo del piso. Mismo criterio que la mesa del cofre.
			o = GetGame().CreateObjectEx("Exor_CofreLoot", pos, ECE_CREATEPHYSICS);
			if (o)
				o.SetPosition(pos);
		}
		else
		{
			o = GetGame().CreateObjectEx("Exor_CofreLoot", pos, ECE_PLACE_ON_SURFACE);
		}
		Exor_CofreLoot cofre = Exor_CofreLoot.Cast(o);
		if (!cofre)
		{
			Print(string.Format("%1 %2: NO se pudo crear el cofre en %3", ExorStorageConstants.LOG, TAG, pos));
			return null;
		}
		cofre.SetOrientation(Vector(Math.RandomInt(0, 360), 0, 0));
		return cofre;
	}

	// Sorteo PONDERADO entre los tipos de la posicion. Los pesos no tienen que sumar 100.
	string ElegirTipo(ExorCfgCofreLootPos p)
	{
		ExorCfgCofreLoot c = Cfg();
		if (!p.tipo || p.tipo.Count() == 0)
			return "";
		int total = 0;
		int i;
		for (i = 0; i < p.tipo.Count(); i++)
		{
			ExorCfgCofreLootPeso w = p.tipo.Get(i);
			if (!w || !c.BuscarTipo(w.nombre))
				continue;
			if (w.probabilidad_que_sea_de_este_tipo > 0)
				total = total + w.probabilidad_que_sea_de_este_tipo;
		}
		if (total <= 0)
			return "";
		int dado = Math.RandomInt(0, total);
		int acum = 0;
		for (i = 0; i < p.tipo.Count(); i++)
		{
			ExorCfgCofreLootPeso w2 = p.tipo.Get(i);
			if (!w2 || w2.probabilidad_que_sea_de_este_tipo <= 0 || !c.BuscarTipo(w2.nombre))
				continue;
			acum = acum + w2.probabilidad_que_sea_de_este_tipo;
			if (dado < acum)
				return w2.nombre;
		}
		return "";
	}

	vector PosDe(ExorCfgCofreLootPos p)
	{
		vector pos = Vector(p.x, p.y, p.z);
		if (p.y <= 0)
			pos[1] = GetGame().SurfaceY(p.x, p.z);
		return pos;
	}

	// ------------------------------------------------------------------------
	//  GUARDIA DE INFECTADOS
	// ------------------------------------------------------------------------
	// Se pone UNA SOLA VEZ, en el mismo momento en que nace el cofre (o sea, cuando un
	// jugador entro en el radio de siembra). Y no se repone nunca: el que los mata se gano
	// el cofre, y el que vuelve al rato no se encuentra con la guardia otra vez.
	//
	// ⭐ POR QUE NO SE REPONEN NI SE RECICLAN
	// Repoblarlos por tick -o sacarlos y devolverlos segun quien ande cerca- convertiria el
	// cofre en una fabrica de infectados: cada ronda de 30 s crearia entidades con IA,
	// navmesh y sincronizacion, que es lo mas caro que puede generar el mod. Naciendo con el
	// cofre, la cuenta es exacta: por cada cofre que aparece se crean N infectados, ni uno
	// mas, y se van todos juntos cuando el cofre se va.
	void PonerGuardia(ExorCfgCofreLoot c, ExorCofreLootPunto pt)
	{
		if (!pt.m_Cofre)
			return;
		ExorCfgCofreLootTipo tabla = c.BuscarTipo(pt.m_Cofre.m_ExorTipo);
		int cuantos = c.ZombiesDe(tabla);
		if (cuantos <= 0)
			return;
		TStringArray clases = c.ClasesZombieDe(tabla);
		if (!clases || clases.Count() == 0)
			return;
		vector centro = pt.m_Cofre.GetPosition();
		int i;
		for (i = 0; i < cuantos; i++)
		{
			string cls = clases.Get(Math.RandomInt(0, clases.Count()));
			vector donde = PosAlrededor(centro, 3.0, 7.0);
			Object z = GetGame().CreateObject(cls, donde, false, true, true);
			if (z)
				pt.m_Zombies.Insert(z);
			else
				Print(string.Format("%1 %2: no se pudo crear el infectado '%3' (classname invalido?)", ExorStorageConstants.LOG, TAG, cls));
		}
		if (pt.m_Zombies.Count() > 0)
			Print(string.Format("%1 %2: %3 infectados de guardia en la posicion %4", ExorStorageConstants.LOG, TAG, pt.m_Zombies.Count(), pt.m_Idx));
	}

	void BorrarZombies(ExorCofreLootPunto pt)
	{
		int i;
		for (i = 0; i < pt.m_Zombies.Count(); i++)
		{
			Object z = pt.m_Zombies.Get(i);
			if (z)
				GetGame().ObjectDelete(z);
		}
		pt.m_Zombies.Clear();
	}

	vector PosAlrededor(vector centro, float minM, float maxM)
	{
		float ang = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(minM, maxM);
		vector p = centro;
		p[0] = centro[0] + (Math.Cos(ang) * dist);
		p[2] = centro[2] + (Math.Sin(ang) * dist);
		p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	// Usa el cache de jugadores del latido de 1 Hz: no pide su propia lista.
	bool HayJugadorCerca(vector pos, float metros)
	{
		array<Man> players = ExorTick1Hz.Jugadores();
		if (!players)
			return false;
		float r2 = metros * metros;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb || !pb.IsAlive())
				continue;
			if (ExorMath.Dist2DSq(pos, pb.GetPosition()) <= r2)
				return true;
		}
		return false;
	}

	// El cofre se fue (lo borro el tick, un admin, o el apagado): liberar el punto y
	// programar la proxima ronda.
	void OnCofreBorrado(Exor_CofreLoot cofre)
	{
		if (!cofre || cofre.m_ExorPunto < 0 || cofre.m_ExorPunto >= m_Puntos.Count())
			return;
		int i;
		for (i = 0; i < m_Puntos.Count(); i++)
		{
			ExorCofreLootPunto pt = m_Puntos.Get(i);
			if (pt && pt.m_Cofre == cofre)
			{
				pt.m_Cofre = null;
				BorrarZombies(pt);	// la guardia se va con su cofre
				int ahora = GetGame().GetTime();
				// El cofre consumido libera un lugar del CUPO, pero el lugar no se vuelve a
				// llenar al instante: se esperan los minutos de respawn. Si no, lootear un
				// cofre haria aparecer otro en la posicion de al lado en el acto.
				m_ProximoSpawnMs = ahora + (Cfg().minutos_re_spawn * 60000);
				// Ademas la posicion que acaba de tener cofre queda en cooldown mas largo,
				// para que el proximo caiga en otra de las del array y no siempre en la misma.
				pt.m_ProximoIntentoMs = ahora + (Cfg().minutos_re_spawn * 60000 * 2);
				return;
			}
		}
	}

	// ------------------------------------------------------------------------
	//  GOLPES / TIROS
	// ------------------------------------------------------------------------
	// Contador FRACCIONARIO, igual criterio que el modulo de raid: cada impacto suma
	// 1/cantidad-necesaria, asi mezclar balas y hachazos sale gratis (10 tiros de 20 = 0,5
	// + 15 golpes de 30 = 0,5 -> se abre) y cada herramienta puede costar distinto.
	void Impacto(Exor_CofreLoot cofre, int damageType, EntityAI source)
	{
		if (!cofre || cofre.m_ExorAbierto)
			return;
		ExorCfgCofreLoot c = Cfg();
		if (!c || !c.enable)
			return;

		int now = GetGame().GetTime();
		if (now - cofre.m_ExorUltimoHitMs < DEDUP_MS)
			return;	// mismo disparo (perdigones): ya se conto
		cofre.m_ExorUltimoHitMs = now;

		PlayerBase quien = JugadorDe(source);
		float suma = 0;
		string comoTexto = "";

		if (damageType == DamageType.FIRE_ARM)
		{
			if (c.tiros_para_aperturarlo <= 0)
				return;	// las balas no abren este cofre
			suma = 1.0 / c.tiros_para_aperturarlo;
			comoTexto = "bala";
		}
		else
		{
			// melee y explosiones: van por el contador de golpes, con la excepcion por
			// herramienta si la clase esta listada en golpes_por_herramienta.
			// De que herramienta es el golpe. Segun el caso, el motor manda como origen el
			// item o al propio jugador, asi que si viene el jugador se mira que tiene en la
			// mano; si no, no habria forma de aplicar golpes_por_herramienta.
			string herramienta = "";
			if (source)
			{
				if (source.IsInherited(PlayerBase))
				{
					PlayerBase pe = PlayerBase.Cast(source);
					if (pe && pe.GetHumanInventory())
					{
						EntityAI enMano = pe.GetHumanInventory().GetEntityInHands();
						if (enMano)
							herramienta = enMano.GetType();
					}
				}
				else
				{
					herramienta = source.GetType();
				}
			}
			int necesarios = c.GolpesDe(herramienta);
			if (necesarios <= 0)
			{
				// El melee no abre este cofre. Sin este aviso, pegarle y que no pase nada se
				// lee como que el cofre esta roto: el jugador no tiene forma de saber que es
				// a tiros. Se avisa como mucho una vez cada 3 s para no llenarle el chat.
				if (quien && now - cofre.m_ExorUltimoAvisoMeleeMs > 3000)
				{
					cofre.m_ExorUltimoAvisoMeleeMs = now;
					ExorAviso.Error(quien, "Este cofre solo se abre a tiros.");
				}
				return;
			}
			suma = 1.0 / necesarios;
			comoTexto = herramienta;
			if (comoTexto == "")
				comoTexto = "golpe";
		}

		cofre.m_ExorProgreso = cofre.m_ExorProgreso + suma;
		int pct = Math.Round(cofre.m_ExorProgreso * 100);
		if (pct > 100)
			pct = 100;

		if (c.log_cada_impacto != 0)
			Print(string.Format("%1 %2: impacto %3 -> %4%% (tipo=%5)", ExorStorageConstants.LOG, TAG, comoTexto, pct, cofre.m_ExorTipo));

		if (cofre.m_ExorProgreso >= 1.0)
		{
			Abrir(cofre, quien);
			return;
		}

		// aviso de progreso cada 25%: da la pista de que se abre a golpes sin llenar el chat
		if (c.avisar_progreso_al_golpear && quien)
		{
			int escalon = (pct / 25) * 25;
			if (escalon > 0 && escalon > cofre.m_ExorUltimoAvisoPct)
			{
				cofre.m_ExorUltimoAvisoPct = escalon;
				ExorAviso.Info(quien, string.Format("Cofre %1%%", escalon));
			}
		}
	}

	// La fuente del daño puede ser el jugador (puños) o el item que empuño / disparo.
	PlayerBase JugadorDe(EntityAI source)
	{
		if (!source)
			return null;
		PlayerBase pb = PlayerBase.Cast(source);
		if (pb)
			return pb;
		return PlayerBase.Cast(source.GetHierarchyRootPlayer());
	}

	// ------------------------------------------------------------------------
	//  APERTURA: recien aca se crea el loot
	// ------------------------------------------------------------------------
	void Abrir(Exor_CofreLoot cofre, PlayerBase quien)
	{
		ExorCfgCofreLoot c = Cfg();
		cofre.ExorAbrirVisual();
		cofre.m_ExorBorrarEnMs = GetGame().GetTime() + (c.minutos_para_borrar_cofre_abierto * 60000);

		int puestos = Llenar(cofre, c.BuscarTipo(cofre.m_ExorTipo));
		Print(string.Format("%1 %2: cofre ABIERTO en %3 (tabla '%4', %5 items)", ExorStorageConstants.LOG, TAG, cofre.GetPosition(), cofre.m_ExorTipo, puestos));

		if (c.avisar_al_abrirse_en_el_chat && quien)
			ExorAviso.Info(quien, "Reventaste el cofre. Mira lo que hay adentro.");
	}

	// Tira los dados de la tabla y crea lo que salio. Devuelve cuantos items entraron.
	int Llenar(Exor_CofreLoot cofre, ExorCfgCofreLootTipo tabla)
	{
		if (!tabla || !tabla.items)
			return 0;
		int puestos = 0;
		int i, n, k;
		for (i = 0; i < tabla.items.Count(); i++)
		{
			ExorCfgCofreLootItem it = tabla.items.Get(i);
			if (!it || it.classname == "")
				continue;
			if (it.probabilidad < 100 && Math.RandomInt(0, 100) >= it.probabilidad)
				continue;
			int cuantos = it.cantidad;
			if (cuantos < 1)
				cuantos = 1;
			for (n = 0; n < cuantos; n++)
			{
				EntityAI e = cofre.GetInventory().CreateInInventory(it.classname);
				if (!e)
				{
					Print(string.Format("%1 %2: '%3' NO entro (classname invalido, mod no cargado, o cofre lleno)", ExorStorageConstants.LOG, TAG, it.classname));
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
					{
						// no le entra al item (slot ocupado / incompatible): que al menos
						// quede en el cofre en vez de perderse en silencio.
						a = cofre.GetInventory().CreateInInventory(att);
					}
					if (a)
						puestos++;
				}
			}
		}
		return puestos;
	}
}
