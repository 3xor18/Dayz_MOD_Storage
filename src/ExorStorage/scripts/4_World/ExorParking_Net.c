// ============================================================================
// 3xor_Vanilla_Optimization - PARKING: datos del menu (DTO) + builder del JSON
// ============================================================================
// El menu de parking tiene DOS paneles: "Almacenados" (autos virtualizados del clan)
// y "Sin Almacenar" (autos reales en el radio del parking). El server arma este DTO
// y lo manda por RPC PARKING_OPEN (en trozos); el cliente lo cachea en ExorParkingClient
// y dibuja las filas. Ver ExorParkingMenu.c (5_Mission) y ExorActionOpenParking.c.
// ============================================================================

// Un auto en el menu. Para los ALMACENADOS se usa 'id' (id del JSON en disco). Para los
// REALES se usa el network id (low/high) -> asi el server lo re-ubica al virtualizar sin
// depender de indices que pueden moverse entre abrir y clickear.
class ExorParkingCarDTO
{
	string	name;
	string	id;			// almacenado: id del JSON; real: ""
	int		netLow;		// real: network id (low bits)
	int		netHigh;	// real: network id (high bits)
}

class ExorParkingMenuDTO
{
	ref array<ref ExorParkingCarDTO> almacenados;	// virtualizados del clan
	ref array<ref ExorParkingCarDTO> reales;		// reales en el radio

	void ExorParkingMenuDTO()
	{
		almacenados = new array<ref ExorParkingCarDTO>;
		reales = new array<ref ExorParkingCarDTO>;
	}
}

// Cache CLIENTE: el ultimo DTO recibido. Lo lee el menu al dibujar/refrescar.
// s_Version se incrementa en cada Set: el menu (5_Mission) lo mira en su Update y se
// refresca cuando cambia. Asi el server (4_World) NO tiene que llamar al menu (que vive
// en 5_Mission y no seria visible desde aca por el orden de carga de modulos).
class ExorParkingClient
{
	static ref ExorParkingMenuDTO s_DTO;
	static int s_Version;
	static void Set(ExorParkingMenuDTO d)
	{
		s_DTO = d;
		s_Version++;
	}
}

// Builder SERVER: arma el DTO (y su JSON) para un parking en 'center' del clan 'groupId'.
class ExorParkingNet
{
	// radio de escaneo de autos reales alrededor del parking (metros). Fallback si no hay config.
	static const float RADIUS = 30.0;

	static float ScanRadius()
	{
		float r = GetExorConfig().storage.parking_radio_metros;
		if (r <= 0)
			r = RADIUS;
		return r;
	}

	static string BuildJson(vector center, string groupId)
	{
		ExorParkingMenuDTO dto = new ExorParkingMenuDTO();

		// ALMACENADOS: los virtualizados del clan (id + nombre).
		array<string> virtIds, virtNames;
		ExorVehicleGarage.ListForGroup(groupId, virtIds, virtNames);
		int i;
		for (i = 0; i < virtIds.Count(); i++)
		{
			ExorParkingCarDTO a = new ExorParkingCarDTO();
			a.id = virtIds.Get(i);
			a.name = virtNames.Get(i);
			dto.almacenados.Insert(a);
		}

		// REALES: autos en el radio (sin tripulantes ya lo filtra el manager/guards).
		array<CarScript> cars;
		ExorVehicleGarage.NearbyReal(center, ScanRadius(), cars);
		for (i = 0; i < cars.Count(); i++)
		{
			CarScript car = cars.Get(i);
			if (!car)
				continue;
			ExorParkingCarDTO r = new ExorParkingCarDTO();
			r.name = car.GetDisplayName();
			int lo, hi;
			car.GetNetworkID(lo, hi);
			r.netLow = lo;
			r.netHigh = hi;
			dto.reales.Insert(r);
		}

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		return data;
	}
}
