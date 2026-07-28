// ============================================================================
// 3xor_Vanilla_Optimization - LOADOUT DE PRUEBA para dummies del VPP
//
// PARA QUE SIRVE
// Probar la tumba a mano es carisimo: hay que juntar el equipo, romperlo en los
// estados justos y morirse, una y otra vez. Con esto se spawnea un survivor con
// el VPP admin, ya sale vestido y armado EXACTAMENTE con los casos que rompian
// el restore, se lo mata y se mira la tumba. Un ciclo de test en 10 segundos.
//
// PlayerBase.EEKilled ya arma tumba para un dummy (no tiene identidad, queda
// registrado como "?"), asi que el camino que se prueba es el REAL, el mismo
// que corre cuando muere un jugador.
//
// COMO SE PRENDE (y por que NO se puede prender solo en el server de verdad)
// Hace falta que exista el archivo marcador:
//     profiles\3xorVanillaOptimization\TEST_LOADOUT
// Sin ese archivo esto no hace absolutamente nada. Es a proposito: un flag en un
// JSON de config se copia de un server a otro sin querer, un archivo suelto que
// hay que crear a mano no. Si el marcador no esta, ni siquiera se programa el
// callback.
//
// QUE REPRODUCE (los tres casos que salieron de los logs del server real)
//   - chaleco DESTRUIDO con pouches, holster, granadas y una pistola adentro
//   - M4A1 DESTRUIDO armado entero (handguard, culata, mira, cargador)
//   - linterna DESTRUIDA con la pila puesta
//   - mochila SANA con cosas adentro  <- grupo de control, tiene que llegar intacta
//
// Todo se arma a vida LLENA y recien al final se destruye, porque un item ruined
// rechaza que le metan nada (ItemBase.CanPutInCargo / CanPutAsAttachment). Es el
// mismo bug que se arreglo en ExorVO_Serializer: si aca se hiciera al reves, el
// loadout saldria a medio armar y el test no probaria nada.
// ============================================================================
class ExorTestLoadout
{
	static const int CHEQUEO_DUMMY_MS = 2000;	// margen para que un player REAL ya tenga identidad

	static int  s_Estado;	// 0 = sin chequear, 1 = prendido, 2 = apagado

	static bool Activo()
	{
		if (s_Estado == 0)
		{
			string marcador = string.Format("%1\\TEST_LOADOUT", ExorStorageConstants.CONFIG_DIR);
			if (FileExist(marcador))
			{
				s_Estado = 1;
				Print(string.Format("%1 TEST-LOADOUT: PRENDIDO (existe %2). Los survivors que spawnees con el VPP salen equipados para probar la tumba. Borra ese archivo para apagarlo.", ExorStorageConstants.LOG, marcador));
			}
			else
			{
				s_Estado = 2;
			}
		}
		return s_Estado == 1;
	}

	// Equipa a 'p'. Devuelve false si no habia con que (no deberia pasar).
	static bool Equipar(PlayerBase p)
	{
		if (!p || !p.GetInventory())
			return false;

		EntityAI vest, pouches, holster, mochila, arma, casco, linterna, pistola, mira;

		// --- CHALECO: pouches + holster con pistola + granadas. Se destruye al final ---
		vest = p.GetInventory().CreateAttachment("PlateCarrierVest");
		if (vest)
		{
			pouches = vest.GetInventory().CreateAttachment("PlateCarrierPouches");
			holster = vest.GetInventory().CreateAttachment("PlateCarrierHolster");
			if (pouches)
			{
				pouches.GetInventory().CreateInInventory("M18SmokeGrenade_Green");
				pouches.GetInventory().CreateInInventory("RGD5Grenade");
				pouches.GetInventory().CreateInInventory("M67Grenade");
			}
			if (holster)
			{
				pistola = holster.GetInventory().CreateInInventory("MKII");
				if (pistola)
					pistola.GetInventory().CreateAttachment("Mag_MKII_10Rnd");
			}
		}

		// --- MOCHILA SANA: grupo de control, tiene que llegar a la tumba con todo adentro ---
		mochila = p.GetInventory().CreateAttachment("HuntingBag");
		if (mochila)
		{
			mochila.GetInventory().CreateInInventory("Canteen");
			mochila.GetInventory().CreateInInventory("Rangefinder");
			mochila.GetInventory().CreateInInventory("Pot");
			mochila.GetInventory().CreateInInventory("Mag_STANAG_30Rnd");
		}

		// --- ARMA armada entera. Se destruye al final ---
		if (p.GetHumanInventory())
			arma = p.GetHumanInventory().CreateInHands("M4A1");
		if (!arma)	// dummy sin manos utilizables -> que vaya al slot de la espalda
			arma = p.GetInventory().CreateAttachment("M4A1");
		if (arma)
		{
			arma.GetInventory().CreateAttachment("M4_MPHndgrd");
			arma.GetInventory().CreateAttachment("M4_CQBBttstck");
			mira = arma.GetInventory().CreateAttachment("ACOGOptic");
			arma.GetInventory().CreateAttachment("Mag_STANAG_30Rnd");
		}

		// --- LINTERNA con pila. Se destruye al final ---
		casco = p.GetInventory().CreateAttachment("Mich2001Helmet");
		linterna = p.GetInventory().CreateAttachment("Headtorch_Grey");
		if (linterna)
			linterna.GetInventory().CreateAttachment("Battery9V");

		// --- AHORA los estados rotos. Antes de este punto no aceptaban nada adentro ---
		Romper(vest);
		Romper(arma);
		Romper(linterna);
		Romper(casco);

		// La mira tambien ROTA: sirve para ver la regla de "lo destruido va al inventario de la
		// tumba, no al slot" tambien a nivel de un attachment suelto.
		Romper(mira);

		Print(string.Format("%1 TEST-LOADOUT: dummy equipado en %2 (chaleco/arma/linterna DESTRUIDOS, mochila sana). Matalo y revisa la tumba.", ExorStorageConstants.LOG, p.GetPosition().ToString()));
		return true;
	}

	static void Romper(EntityAI e)
	{
		if (e)
			e.SetHealth01("", "", 0.0);
	}
}

modded class PlayerBase
{
	bool m_ExorTestEquipado;

	override void EEInit()
	{
		super.EEInit();

		// Solo server, y solo si el marcador esta puesto. Sin marcador ni se agenda nada.
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!ExorTestLoadout.Activo())
			return;

		// No se puede decidir ACA si es un dummy: un jugador de verdad todavia puede no tener
		// la identidad asignada en EEInit y lo estariamos equipando a el. Se espera un margen
		// y recien ahi se mira: si sigue SIN identidad, es un survivor spawneado por el VPP.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorTestEquiparSiEsDummy, ExorTestLoadout.CHEQUEO_DUMMY_MS, false);
	}

	void ExorTestEquiparSiEsDummy()
	{
		if (m_ExorTestEquipado)
			return;
		if (!IsAlive())
			return;
		if (GetIdentity())
			return;	// jugador de verdad -> no se le toca NADA
		m_ExorTestEquipado = true;
		ExorTestLoadout.Equipar(this);
	}
};
