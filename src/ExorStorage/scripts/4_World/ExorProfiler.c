// ============================================================================
// 3xor_Vanilla_Optimization - PROFILER Y BANCO DE PRUEBAS (SOLO server, admin)
// ============================================================================
// POR QUE EXISTE
// ----------------------------------------------------------------------------
// Hasta ahora, para saber si algo del mod era lento habia que DEDUCIRLO leyendo
// codigo. El monitor de performance que ya existia dice CUANTO tardo un tick, pero
// no QUE funcion se lo comio. Y las decisiones de optimizacion tomadas por intuicion
// se equivocan: en este mismo mod la teoria del "5/5" se desmintio midiendo, y la
// causa del lag de raid resulto ser acumulacion de entidades y no el costo de
// virtualizar.
//
// Enfusion expone un profiler completo desde script (EnProfiler) que casi nadie usa:
//   - tiempo POR FUNCION y por clase
//   - ALLOCATIONS por clase  -> caza el churn de memoria
//   - INSTANCIAS VIVAS por clase -> caza las fugas
// Esto lo envuelve en algo utilizable: se prende, corre N frames, y vuelca un reporte
// ordenado a un archivo del profile.
//
// COMO SE USA (por chat, solo admin, ver ExorChatServer):
//   /perf            -> perfila 300 frames y escribe el reporte
//   /perf 900        -> idem, 900 frames
//   /carga 20 200    -> banco de pruebas: 20 contenedores con 200 items cada uno
//   /entidades       -> conteo rapido de lo que el mod tiene registrado
//
// El banco de pruebas existe porque medir con 1 jugador no dice nada: el problema
// aparece a 70 jugadores y cientos de contenedores. Generar esa carga a mano es
// imposible; generarla por codigo son dos segundos.
// ============================================================================
class ExorProfiler
{
	static bool s_Corriendo;
	static int  s_FramesObjetivo;
	static int  s_FrameInicio;
	static string s_Pedido;		// steamid del admin que lo pidio (para avisarle al terminar)
	static int  s_InicioMs;		// para el timeout (el profiler no corre en build release)
	static const int EXOR_PERF_TIMEOUT_MS = 90000;

	static string PathReporte()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\perf_report.txt";
	}

	// Arranca una sesion de profiling de 'frames' frames.
	static void Iniciar(int frames, PlayerBase quien)
	{
		if (!GetGame().IsServer())
			return;
		if (s_Corriendo)
		{
			ExorMuebleRules.SendRed(quien, "Ya hay un profiling corriendo.");
			return;
		}
		if (frames < 60)
			frames = 60;
		if (frames > 5000)
			frames = 5000;	// tope: el profiler cuesta, no dejarlo prendido eternamente

		s_Corriendo = true;
		s_FramesObjetivo = frames;
		s_Pedido = "";
		if (quien && quien.GetIdentity())
			s_Pedido = quien.GetIdentity().GetPlainId();

		// OJO: EnProfiler SOLO existe en los builds developer/diag ("Only available on
		// developer and diag builds", dice su propio header). En el DayZServer_x64 normal
		// las llamadas no fallan pero el contador de frames NO avanza, asi que sin el
		// timeout de abajo esto se quedaba esperando para siempre.
		// Para un profile real hay que levantar el server con DayZDiag_x64.exe -server.
		EnProfiler.Enable(true, true, true);	// immediate + reset de sesion
		s_FrameInicio = EnProfiler.GetSessionFrame();
		s_InicioMs = GetGame().GetTime();
		ExorMuebleRules.SendVerde(quien, string.Format("Profiling arrancado: %1 frames. Te aviso cuando termine.", frames));
		Print(string.Format("%1 PERF: profiling arrancado (%2 frames)", ExorStorageConstants.LOG, frames));

		// El chequeo va por CallLater y no por frame: el profiler ya cuenta los frames solo.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorProfiler.Chequear, 1000, true);
	}

	static void Chequear()
	{
		if (!s_Corriendo)
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ExorProfiler.Chequear);
			return;
		}
		int hechos = EnProfiler.GetSessionFrame() - s_FrameInicio;
		if (hechos < s_FramesObjetivo)
		{
			// TIMEOUT: si en 90 s no avanzo ni un frame, el profiler no esta disponible
			// (build release). Se corta y se avisa en vez de quedarse colgado.
			if (GetGame().GetTime() - s_InicioMs > EXOR_PERF_TIMEOUT_MS && hechos <= 0)
			{
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ExorProfiler.Chequear);
				s_Corriendo = false;
				EnProfiler.Enable(false, true, false);
				Print(string.Format("%1 PERF: el profiler NO avanza -> este build no lo soporta. EnProfiler solo existe en developer/diag. Levanta el server con DayZDiag_x64.exe -server para perfilar.", ExorStorageConstants.LOG));
				ExorProfiler.EscribirSoloEntidades();
				ExorProfiler.AvisarAlPedido("El profiler no corre en este build (hace falta DayZDiag). Se escribio igual el resumen de entidades.");
			}
			return;
		}
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ExorProfiler.Chequear);
		Terminar(hechos);
	}

	static void Terminar(int framesHechos)
	{
		s_Corriendo = false;
		EnProfiler.Enable(false, true, false);

		FileHandle fh = OpenFile(PathReporte(), FileMode.WRITE);
		if (fh == 0)
		{
			Print(string.Format("%1 PERF: no se pudo escribir el reporte", ExorStorageConstants.LOG));
			return;
		}

		FPrintln(fh, "==========================================================");
		FPrintln(fh, "  REPORTE DE PERFORMANCE - " + ExorRaidLog.TimeStamp());
		FPrintln(fh, "  build " + ExorStorageConstants.MOD_BUILD + " | frames perfilados: " + framesHechos.ToString());
		FPrintln(fh, "  jugadores conectados: " + ExorVO_Manager.s_PopCount.ToString());
		FPrintln(fh, "==========================================================");
		FPrintln(fh, "");

		// ---- 1) TIEMPO POR FUNCION: donde se va el CPU ----
		FPrintln(fh, "--- TOP 40 FUNCIONES POR TIEMPO (ms totales en la sesion) ---");
		array<ref EnProfilerTimeFuncPair> tf = new array<ref EnProfilerTimeFuncPair>;
		EnProfiler.GetTimePerFunc(tf, 40);
		int i;
		for (i = 0; i < tf.Count(); i++)
		{
			EnProfilerTimeFuncPair par = tf.Get(i);
			if (!par)
				continue;
			FPrintln(fh, string.Format("%1 ms   %2", par.param1, par.param2));
		}
		FPrintln(fh, "");

		// ---- 2) ALLOCATIONS POR CLASE: churn de memoria ----
		// Esto es lo que no se puede deducir leyendo codigo: cuantos objetos por segundo
		// se estan creando y tirando. Un numero alto aca explica pausas parejas de GC.
		FPrintln(fh, "--- TOP 40 CLASES POR ALLOCATIONS (objetos creados) ---");
		array<ref EnProfilerCountClassPair> ac = new array<ref EnProfilerCountClassPair>;
		EnProfiler.GetAllocationsPerClass(ac, 40);
		for (i = 0; i < ac.Count(); i++)
		{
			EnProfilerCountClassPair pa = ac.Get(i);
			if (!pa)
				continue;
			FPrintln(fh, string.Format("%1   %2", pa.param1, pa.param2));
		}
		FPrintln(fh, "");

		// ---- 3) INSTANCIAS VIVAS POR CLASE: fugas ----
		// Si un numero de aca CRECE entre dos reportes y no baja, eso es una fuga.
		FPrintln(fh, "--- TOP 40 CLASES POR INSTANCIAS VIVAS (si crece y no baja = FUGA) ---");
		array<ref EnProfilerCountClassPair> ic = new array<ref EnProfilerCountClassPair>;
		EnProfiler.GetInstancesPerClass(ic, 40);
		for (i = 0; i < ic.Count(); i++)
		{
			EnProfilerCountClassPair pi = ic.Get(i);
			if (!pi)
				continue;
			FPrintln(fh, string.Format("%1   %2", pi.param1, pi.param2));
		}
		FPrintln(fh, "");
		FPrintln(fh, "--- ENTIDADES REGISTRADAS POR EL MOD ---");
		FPrintln(fh, ExorProfiler.ResumenEntidades());
		CloseFile(fh);

		Print(string.Format("%1 PERF: reporte escrito en %2 (%3 frames)", ExorStorageConstants.LOG, PathReporte(), framesHechos));
		ExorProfiler.AvisarAlPedido("Profiling terminado. Reporte en profiles\\3xorVanillaOptimization\\perf_report.txt");
	}

	// Reporte minimo cuando el profiler no esta disponible: al menos deja el conteo de
	// entidades, que es lo que sirve para detectar acumulacion entre dos mediciones.
	static void EscribirSoloEntidades()
	{
		FileHandle fh = OpenFile(PathReporte(), FileMode.WRITE);
		if (fh == 0)
			return;
		FPrintln(fh, "==========================================================");
		FPrintln(fh, "  RESUMEN DE ENTIDADES - " + ExorRaidLog.TimeStamp());
		FPrintln(fh, "  build " + ExorStorageConstants.MOD_BUILD);
		FPrintln(fh, "  (sin profiling: EnProfiler solo corre en builds developer/diag)");
		FPrintln(fh, "==========================================================");
		FPrintln(fh, ExorProfiler.ResumenEntidades());
		CloseFile(fh);
		Print(string.Format("%1 PERF: resumen de entidades escrito en %2", ExorStorageConstants.LOG, PathReporte()));
	}

	static void AvisarAlPedido(string texto)
	{
		if (s_Pedido == "")
			return;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity() && pb.GetIdentity().GetPlainId() == s_Pedido)
			{
				ExorMuebleRules.SendVerde(pb, texto);
				return;
			}
		}
	}

	// Conteo de lo que el mod tiene vivo. Barato, sirve para ver de un vistazo si algo
	// se esta acumulando.
	static string ResumenEntidades()
	{
		ExorVO_Manager m = ExorVO_Manager.Get();
		int barriles = 0;
		int muebles = 0;
		int bolsas = 0;
		int autos = 0;
		int virtB = 0;
		int virtM = 0;
		int i;
		if (m.m_Barrels)
		{
			barriles = m.m_Barrels.Count();
			for (i = 0; i < m.m_Barrels.Count(); i++)
			{
				if (m.m_Barrels.Get(i) && m.m_Barrels.Get(i).ExorIsVirtualized())
					virtB++;
			}
		}
		if (m.m_Openables)
		{
			muebles = m.m_Openables.Count();
			for (i = 0; i < m.m_Openables.Count(); i++)
			{
				if (m.m_Openables.Get(i) && m.m_Openables.Get(i).ExorIsVirtualized())
					virtM++;
			}
		}
		if (m.m_BodyBags)
			bolsas = m.m_BodyBags.Count();
		if (m.m_Vehicles)
			autos = m.m_Vehicles.Count();

		string l1 = string.Format("barriles=%1 (virtualizados %2) | muebles=%3 (virtualizados %4)", barriles, virtB, muebles, virtM);
		string l2 = string.Format(" | tumbas=%1 | vehiculos=%2 | grupos=%3", bolsas, autos, ExorGroupManager.Get().m_Groups.Count());
		return l1 + l2;
	}
}

// ============================================================================
//  BANCO DE PRUEBAS: generar carga sintetica
// ============================================================================
// Medir con un jugador no dice nada: los problemas aparecen con cientos de
// contenedores llenos y decenas de jugadores. Esto arma esa situacion en segundos
// para poder medirla de verdad, en vez de estimarla.
//
// Los contenedores se crean alrededor del admin que pide la prueba y se marcan como
// de prueba en el log, para poder borrarlos despues sin dudar de cual era cual.
class ExorLoadTest
{
	static const string EXOR_TEST_ITEM = "Ammo_556x45";

	// Crea 'cuantos' lockers con 'items' items cada uno, alrededor de 'quien'.
	static void Generar(PlayerBase quien, int cuantos, int items)
	{
		if (!GetGame().IsServer() || !quien)
			return;
		GenerarEn(quien.GetPosition(), cuantos, items);
		ExorMuebleRules.SendVerde(quien, "Carga generada. Ahora corré /perf para medir.");
	}

	// Version por POSICION: la usa el auto-perf, que corre sin jugador.
	static void GenerarEn(vector base_, int cuantos, int items)
	{
		if (!GetGame().IsServer())
			return;
		if (cuantos < 1)
			cuantos = 1;
		if (cuantos > 200)
			cuantos = 200;
		if (items < 0)
			items = 0;
		if (items > 500)
			items = 500;

		int creados = 0;
		int itemsTotal = 0;
		int t0 = GetGame().GetTime();
		int i;
		for (i = 0; i < cuantos; i++)
		{
			// grilla de 3 m alrededor del admin.
			// El % se resuelve en enteros y recien despues se pasa a float: en contexto
			// float el compilador de Enforce no lo acepta ("Unknown operator '%'").
			int col = i - ((i / 10) * 10);	// equivalente a i % 10, sin usar el operador
			int fila = i / 10;
			float dx = col * 3.0;
			float dz = fila * 3.0;
			vector pos = Vector(base_[0] + dx, base_[1], base_[2] + dz);
			pos[1] = GetGame().SurfaceY(pos[0], pos[2]);

			EntityAI cont = EntityAI.Cast(GetGame().CreateObjectEx("Exor_Locker", pos, ECE_PLACE_ON_SURFACE));
			if (!cont)
				continue;
			creados++;

			// El contenedor arranca CERRADO y su CanReceiveItemIntoCargo rechaza todo
			// mientras no este abierto (es la regla del mod). Para llenarlo hay que abrirlo
			// primero, igual que haria un jugador. Sin esto la prueba creaba 50 contenedores
			// VACIOS y no medía nada de lo que importa.
			Exor_OpenableStorage op = Exor_OpenableStorage.Cast(cont);
			if (op)
				op.Open();

			int k;
			for (k = 0; k < items; k++)
			{
				EntityAI it = cont.GetInventory().CreateInInventory(EXOR_TEST_ITEM);
				if (!it)
					break;	// se lleno de verdad
				itemsTotal++;
			}
			if (op)
				op.Close();	// cerrarlo deja al manager virtualizarlo, que es lo que se quiere medir
		}
		int ms = GetGame().GetTime() - t0;
		Print(string.Format("%1 PRUEBA DE CARGA: %2 contenedores, %3 items, en %4 ms", ExorStorageConstants.LOG, creados, itemsTotal, ms));
	}

	// Borra TODOS los lockers registrados. Solo para el banco de pruebas.
	static void Limpiar(PlayerBase quien)
	{
		if (!GetGame().IsServer())
			return;
		ExorVO_Manager m = ExorVO_Manager.Get();
		int borrados = 0;
		int i;
		for (i = m.m_Openables.Count() - 1; i >= 0; i--)
		{
			Exor_OpenableStorage f = m.m_Openables.Get(i);
			if (!f)
				continue;
			if (f.GetType() != "Exor_Locker")
				continue;
			GetGame().ObjectDelete(f);
			borrados++;
		}
		string msg = string.Format("PRUEBA DE CARGA: %1 lockers borrados", borrados);
		Print(string.Format("%1 %2", ExorStorageConstants.LOG, msg));
		ExorMuebleRules.SendVerde(quien, msg);
	}
}

// ============================================================================
//  AUTO-PERF: la prueba completa SIN JUGADOR
// ============================================================================
// Los comandos de chat necesitan a alguien al teclado. Para poder medir sin nadie
// adentro -o de forma reproducible, siempre igual- existe este camino: si al arrancar
// hay un archivo marcador en el profile, el server monta la carga solo, perfila y
// escribe el reporte.
//
// MARCADOR: profiles/3xorVanillaOptimization/perf_test.txt
//   linea 1: <contenedores> <items> <frames> <x> <z>
//   ejemplo: 50 300 900 1914 7887
//
// El marcador SE BORRA al empezar: la prueba corre UNA vez y el server queda normal.
// Si no se borrara, cada reinicio spawnearia otra tanda de contenedores.
class ExorAutoPerf
{
	static string PathMarcador()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\perf_test.txt";
	}

	static int s_Cuantos;
	static int s_Items;
	static int s_Frames;
	static float s_X;
	static float s_Z;

	// La llama MissionServer.OnInit. Solo lee el marcador y agenda; la prueba corre
	// despues de que el mundo termino de cargar.
	static void Init()
	{
		if (!GetGame().IsServer())
			return;
		string path = PathMarcador();
		if (!FileExist(path))
			return;

		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return;
		string linea = "";
		FGets(fh, linea);
		CloseFile(fh);
		DeleteFile(path);	// una sola vez

		array<string> a = new array<string>;
		linea.Trim();
		linea.Split(" ", a);
		s_Cuantos = 20;
		s_Items = 200;
		s_Frames = 600;
		s_X = 0;
		s_Z = 0;
		if (a.Count() > 0) s_Cuantos = a.Get(0).ToInt();
		if (a.Count() > 1) s_Items = a.Get(1).ToInt();
		if (a.Count() > 2) s_Frames = a.Get(2).ToInt();
		if (a.Count() > 3) s_X = a.Get(3).ToFloat();
		if (a.Count() > 4) s_Z = a.Get(4).ToFloat();

		Print(string.Format("%1 AUTO-PERF: programado -> %2 contenedores x %3 items, %4 frames, en <%5, %6>",
			ExorStorageConstants.LOG, s_Cuantos, s_Items, s_Frames, s_X, s_Z));

		// 90 s: hay que esperar a que el CE termine de restaurar la persistencia, si no se
		// mide el arranque en vez del regimen normal.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoPerf.Correr, 90000, false);
	}

	static void Correr()
	{
		Print(string.Format("%1 AUTO-PERF: montando la carga...", ExorStorageConstants.LOG));
		vector pos = Vector(s_X, 0, s_Z);
		pos[1] = GetGame().SurfaceY(s_X, s_Z);
		ExorLoadTest.GenerarEn(pos, s_Cuantos, s_Items);

		// 20 s de margen para que el manager haga sus primeros ticks con la carga puesta
		// (reconcile, snapshot, virtualizacion): es justo lo que se quiere medir.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoPerf.Medir, 20000, false);
	}

	static void Medir()
	{
		Print(string.Format("%1 AUTO-PERF: arrancando el profiler (%2 frames)", ExorStorageConstants.LOG, s_Frames));
		ExorProfiler.Iniciar(s_Frames, null);
	}
}
