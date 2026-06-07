// ============================================================================
// 3xor_Vanilla_Optimization - Cobertura de vehiculos (Fase 2.5)
// Un vehiculo inactivo se serializa a JSON y se reemplaza por:
//   - un objeto ESTATICO con el modelo del auto (sin fisica = sin costo)
//   - la red camo (Exor_VehicleCover) que guarda el ID y permite restaurar
// ============================================================================

// Registra los vehiculos en el manager y trackea su actividad
modded class CarScript
{
	protected int m_ExorLastActiveMs;

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			m_ExorLastActiveMs = GetGame().GetTime();
			ExorVO_Manager.RegisterVehicle(this);
		}
	}

	bool ExorIsActive()
	{
		if (EngineIsOn())
			return true;
		int i;
		for (i = 0; i < CrewSize(); i++)
		{
			if (CrewMember(i))
				return true;
		}
		return false;
	}

	void ExorMarkActive(int now)
	{
		m_ExorLastActiveMs = now;
	}

	int ExorGetLastActive()
	{
		return m_ExorLastActiveMs;
	}
}

// La red camo colocada sobre el auto cubierto
class Exor_VehicleCover : ItemBase
{
	protected string m_ExorVehId;
	protected string m_ExorVehType;
	protected Object m_ExorStaticCar;

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		ExorRemoveStaticCar();
		super.EEDelete(parent);
	}

	// No se puede levantar ni guardar: solo "Quitar la cobertura"
	override bool CanPutInCargo(EntityAI parent)
	{
		return false;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorVehId);
		ctx.Write(m_ExorVehType);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string id;
		string vtype;
		if (!ctx.Read(id))
			return false;
		if (!ctx.Read(vtype))
			return false;
		m_ExorVehId = id;
		m_ExorVehType = vtype;
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		// Tras un reinicio el objeto estatico visual no persiste: recrearlo
		if (GetGame().IsServer() && m_ExorVehType != "")
		{
			ExorSpawnStaticCar();
		}
	}

	void ExorSetup(string vehId, string vehType)
	{
		m_ExorVehId = vehId;
		m_ExorVehType = vehType;
		ExorSpawnStaticCar();
	}

	string ExorGetVehicleId()
	{
		return m_ExorVehId;
	}

	string ExorGetVehiclePath()
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.VEHICLES_DIR, m_ExorVehId);
	}

	void ExorSpawnStaticCar()
	{
		if (m_ExorStaticCar)
			return;
		string modelPath;
		if (!GetGame().ConfigGetText(string.Format("CfgVehicles %1 model", m_ExorVehType), modelPath))
			return;
		if (modelPath == "")
			return;
		m_ExorStaticCar = GetGame().CreateStaticObjectUsingP3D(modelPath, GetPosition(), GetOrientation());
		if (m_ExorStaticCar)
		{
			ExorVO_Manager.RegisterStaticCover(m_ExorStaticCar, this);
		}
	}

	void ExorRemoveStaticCar()
	{
		if (m_ExorStaticCar)
		{
			ExorVO_Manager.UnregisterStaticCover(m_ExorStaticCar);
			GetGame().ObjectDelete(m_ExorStaticCar);
			m_ExorStaticCar = null;
		}
	}
}

// Logica de cubrir / descubrir
class ExorVO_VehicleCoverSystem
{
	static void CoverVehicle(CarScript car)
	{
		if (!car)
			return;

		ExorVO_VehicleFile f = new ExorVO_VehicleFile();
		f.id = ExorVO_Serializer.GenerateId();
		f.type = car.GetType();

		vector pos = car.GetPosition();
		vector ori = car.GetOrientation();
		f.pos_x = pos[0];
		f.pos_y = pos[1];
		f.pos_z = pos[2];
		f.ori_x = ori[0];
		f.ori_y = ori[1];
		f.ori_z = ori[2];

		f.health = car.GetHealth01("", "");
		f.fuel = car.GetFluidFraction(CarFluid.FUEL);
		f.oil = car.GetFluidFraction(CarFluid.OIL);
		f.coolant = car.GetFluidFraction(CarFluid.COOLANT);
		f.brake = car.GetFluidFraction(CarFluid.BRAKE);

		ExorVO_ItemData tree = ExorVO_Serializer.CaptureItem(car);
		f.attachments = tree.attachments;
		f.cargo = tree.cargo;

		// ANTI-DUPE: JSON primero, borrar despues (crash-safe)
		string path = string.Format("%1\\%2.json", ExorStorageConstants.VEHICLES_DIR, f.id);
		JsonFileLoader<ExorVO_VehicleFile>.JsonSaveFile(path, f);

		Exor_VehicleCover cover = Exor_VehicleCover.Cast(GetGame().CreateObjectEx("Exor_VehicleCover", pos, ECE_KEEPHEIGHT));
		if (!cover)
		{
			// Si no se pudo crear la cobertura, NO borrar el vehiculo
			DeleteFile(path);
			Print(string.Format("%1 ERROR: no se pudo crear Exor_VehicleCover; vehiculo %2 queda vivo", ExorStorageConstants.LOG, f.type));
			return;
		}
		cover.SetPosition(pos);
		cover.SetOrientation(ori);
		cover.ExorSetup(f.id, f.type);

		GetGame().ObjectDelete(car);
		Print(string.Format("%1 Vehiculo %2 cubierto (id %3): %4 attachments, %5 items de cargo", ExorStorageConstants.LOG, f.type, f.id, f.attachments.Count(), f.cargo.Count()));
	}

	static void UncoverVehicle(Exor_VehicleCover cover)
	{
		if (!cover)
			return;

		string path = cover.ExorGetVehiclePath();
		if (!FileExist(path))
		{
			// Archivo consumido o perdido: anti-dupe en accion o error previo
			Print(string.Format("%1 ALERTA: cobertura sin JSON (%2) - posible dupe detectado; se elimina la cobertura vacia", ExorStorageConstants.LOG, path));
			GetGame().ObjectDelete(cover);
			return;
		}

		ExorVO_VehicleFile f = new ExorVO_VehicleFile();
		JsonFileLoader<ExorVO_VehicleFile>.JsonLoadFile(path, f);

		vector pos = Vector(f.pos_x, f.pos_y, f.pos_z);
		vector ori = Vector(f.ori_x, f.ori_y, f.ori_z);

		Car car = Car.Cast(GetGame().CreateObjectEx(f.type, pos, ECE_KEEPHEIGHT | ECE_CREATEPHYSICS));
		if (!car)
		{
			Print(string.Format("%1 ERROR: no se pudo recrear el vehiculo %2; el JSON se conserva en %3", ExorStorageConstants.LOG, f.type, path));
			return;
		}
		car.SetPosition(pos);
		car.SetOrientation(ori);
		car.SetHealth01("", "", f.health);
		car.Fill(CarFluid.FUEL, f.fuel * car.GetFluidCapacity(CarFluid.FUEL));
		car.Fill(CarFluid.OIL, f.oil * car.GetFluidCapacity(CarFluid.OIL));
		car.Fill(CarFluid.COOLANT, f.coolant * car.GetFluidCapacity(CarFluid.COOLANT));
		car.Fill(CarFluid.BRAKE, f.brake * car.GetFluidCapacity(CarFluid.BRAKE));

		int i;
		for (i = 0; i < f.attachments.Count(); i++)
		{
			ExorVO_Serializer.RestoreItem(f.attachments.Get(i), car, pos);
		}
		for (i = 0; i < f.cargo.Count(); i++)
		{
			ExorVO_Serializer.RestoreItem(f.cargo.Get(i), car, pos);
		}

		// ANTI-DUPE: consumir el JSON tras restaurar
		DeleteFile(path);

		GetGame().ObjectDelete(cover);
		Print(string.Format("%1 Vehiculo %2 descubierto y restaurado", ExorStorageConstants.LOG, f.type));
	}
}
