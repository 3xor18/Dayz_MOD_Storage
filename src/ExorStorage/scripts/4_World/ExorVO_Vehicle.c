// ============================================================================
// 3xor_Vanilla_Optimization - VIRTUALIZACION DE VEHICULOS (nucleo, WIP)
// ============================================================================
// Los autos son las entidades MAS CARAS del server: fisica + RED (se replican a
// todos los clientes en rango, costo O(jugadores)) + persistencia + inventario en
// memoria. "Dormir" un auto (DisableSimulation) solo baja la fisica: el auto SIGUE
// en la red y en la persistencia. VIRTUALIZAR lo saca del mundo por completo y lo
// guarda a disco, y se recrea al pedido -> baja las cuatro cargas.
//
// ESTE ARCHIVO ES SOLO EL NUCLEO capturar<->restaurar, para VALIDAR EN LOCAL que un
// auto se puede guardar y recrear SIN PERDER NADA (posicion, orientacion, fluidos,
// daño, piezas, cargo, y el barril 3xor del slot con su contenido). La automatizacion
// (parking pad + tiempo) se construye DESPUES de que esto quede probado por tipo de auto.
//
// CASO CRITICO (Raptor con barril 3xor en slot de barril): el barril es un ATTACHMENT
// del auto. Mientras esta en el slot, ExorIsInVehicleSlot lo mantiene ABIERTO y NO lo
// virtualiza, asi que sus items son REALES -> la captura recursiva los toma inline.
// Guard: si por lo que sea el barril quedara virtualizado al momento de capturar, se
// ABORTA (no se captura un barril a medias) y se reintenta despues de restaurarlo.
// ============================================================================

// Estado persistido de un vehiculo virtualizado. Reusa ExorVO_ItemData (attachments +
// cargo recursivos) y agrega lo que un item no tiene: transform y fluidos.
class ExorVO_VehicleFile
{
	int version = 1;
	string id;
	string type;
	string group_id;		// clan dueño (para que el menu del mastil muestre solo los tuyos)
	string display;			// nombre legible del auto (para el menu)
	// transform: un barril se para derecho; un auto necesita su rotacion EXACTA.
	float px, py, pz;
	float ori_yaw, ori_pitch, ori_roll;
	// salud global del chasis (cada pieza -rueda/puerta/motor- guarda la suya como attachment)
	float health = 1.0;
	// fluidos: NO son items, son fracciones 0..1 del vehiculo mismo
	float fuel = 0;
	float oil = 0;
	float brake = 0;
	float coolant = 0;
	// piezas (ruedas, bateria, bujia, radiador, puertas, faros, barril-en-slot) y cargo (baul/cabina)
	ref array<ref ExorVO_ItemData> att;
	ref array<ref ExorVO_ItemData> cargo;

	void ExorVO_VehicleFile()
	{
		att = new array<ref ExorVO_ItemData>;
		cargo = new array<ref ExorVO_ItemData>;
	}
}

class ExorVO_Vehicle
{
	// ------------------------- CAPTURA -------------------------
	// Devuelve el estado del auto, o null si NO es seguro capturarlo ahora (barril del
	// slot a medias). El caller decide si borra el auto (virtualiza) segun el resultado.
	static ExorVO_VehicleFile Capture(CarScript car)
	{
		if (!car)
			return null;

		// GUARD barril-en-slot: no capturar si algun contenedor 3xor atado sigue virtualizado
		// (capturariamos un barril vacio y su loot quedaria huerfano en otro JSON).
		if (HasVirtualizedAttachedContainer(car))
			return null;

		ExorVO_VehicleFile f = new ExorVO_VehicleFile();
		f.type = car.GetType();

		vector p = car.GetPosition();
		f.px = p[0]; f.py = p[1]; f.pz = p[2];
		vector o = car.GetOrientation();
		f.ori_yaw = o[0]; f.ori_pitch = o[1]; f.ori_roll = o[2];

		f.health = car.GetHealth01("", "");

		f.fuel    = car.GetFluidFraction(CarFluid.FUEL);
		f.oil     = car.GetFluidFraction(CarFluid.OIL);
		f.brake   = car.GetFluidFraction(CarFluid.BRAKE);
		f.coolant = car.GetFluidFraction(CarFluid.COOLANT);

		// piezas + cargo recursivos (mismo serializer que barriles/tumbas). Captura ruedas,
		// motor, puertas, el baul, y el barril-del-slot CON su contenido real.
		GameInventory inv = car.GetInventory();
		if (inv)
		{
			int i;
			for (i = 0; i < inv.AttachmentCount(); i++)
			{
				EntityAI att = inv.GetAttachmentFromIndex(i);
				if (att)
					f.att.Insert(ExorVO_Serializer.CaptureItem(att));
			}
			CargoBase cargo = inv.GetCargo();
			if (cargo)
			{
				for (i = 0; i < cargo.GetItemCount(); i++)
				{
					EntityAI it = cargo.GetItem(i);
					if (it)
						f.cargo.Insert(ExorVO_Serializer.CaptureItem(it));
				}
			}
		}
		return f;
	}

	// true si el auto tiene atado un contenedor 3xor (barril/mueble) que sigue virtualizado.
	static bool HasVirtualizedAttachedContainer(CarScript car)
	{
		GameInventory inv = car.GetInventory();
		if (!inv)
			return false;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
		{
			Exor_Barrel_Base b = Exor_Barrel_Base.Cast(inv.GetAttachmentFromIndex(i));
			if (b && b.ExorIsVirtualized())
				return true;
			Exor_OpenableStorage m = Exor_OpenableStorage.Cast(inv.GetAttachmentFromIndex(i));
			if (m && m.ExorIsVirtualized())
				return true;
		}
		return false;
	}

	// ------------------------- RESTAURACION -------------------------
	// Recrea el auto desde su estado. Devuelve el auto nuevo (o null si fallo).
	static CarScript Restore(ExorVO_VehicleFile f)
	{
		if (!f || f.type == "")
			return null;

		vector pos = Vector(f.px, f.py, f.pz);
		// ECE_KEEPHEIGHT: respetar la Y guardada (no re-trazar a la superficie -> el auto
		// vuelve EXACTO donde estaba, no reasentado). Es un vehiculo con fisica -> crear con
		// fisica activa. Se orienta antes de soltar la simulacion.
		CarScript car = CarScript.Cast(GetGame().CreateObjectEx(f.type, pos, ECE_KEEPHEIGHT | ECE_CREATEPHYSICS));
		if (!car)
		{
			Print(string.Format("%1 ERROR: no se pudo recrear el vehiculo tipo '%2'", ExorStorageConstants.LOG, f.type));
			return null;
		}
		car.SetPosition(pos);
		car.SetOrientation(Vector(f.ori_yaw, f.ori_pitch, f.ori_roll));

		// piezas: ruedas/motor/puertas/barril-del-slot van como ATTACHMENT (asAttachment=true).
		// TakeEntityAsAttachment ubica cada una en su slot valido.
		int i;
		for (i = 0; i < f.att.Count(); i++)
			ExorVO_Serializer.RestoreItem(f.att.Get(i), car, pos, car, true);

		// cargo del baul/cabina (mismo camino que el barril, big-first para no fragmentar)
		ExorVO_Serializer.RestoreItemsBigFirst(f.cargo, car, pos);

		// fluidos: setear a la fraccion guardada (delta sobre lo que traiga el auto nuevo)
		RestoreFluid(car, CarFluid.FUEL, f.fuel);
		RestoreFluid(car, CarFluid.OIL, f.oil);
		RestoreFluid(car, CarFluid.BRAKE, f.brake);
		RestoreFluid(car, CarFluid.COOLANT, f.coolant);

		car.SetHealth01("", "", f.health);
		return car;
	}

	// Lleva un fluido a la fraccion objetivo, sin importar con cuanto spawnee el auto nuevo.
	static void RestoreFluid(CarScript car, CarFluid fluid, float targetFrac)
	{
		if (targetFrac < 0)
			targetFrac = 0;
		if (targetFrac > 1)
			targetFrac = 1;
		float cap = car.GetFluidCapacity(fluid);
		if (cap <= 0)
			return;
		float target = targetFrac * cap;
		float current = car.GetFluidFraction(fluid) * cap;
		float delta = target - current;
		if (delta > 0.001)
			car.Fill(fluid, delta);
		else if (delta < -0.001)
			car.Leak(fluid, -delta);
	}
}
