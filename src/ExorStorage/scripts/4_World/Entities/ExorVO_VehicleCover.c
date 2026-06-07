// ============================================================================
// 3xor_Vanilla_Optimization - Cobertura de vehiculos (Fase 2.5, arquitectura v2)
// El vehiculo NO se borra (asi el CE lo sigue contando y no spawnea clones):
//   - se le desactiva la simulacion fisica (DisableSimulation = costo ~cero)
//   - se teletransporta 100 m bajo tierra (invisible e inaccesible)
//   - en su lugar queda la red camo (Exor_VehicleCover) + el auto estatico visual
// Al descubrir: el vehiculo vuelve a su posicion con TODO su inventario intacto
// (nunca salio del mundo = cero riesgo de perdida o dupe).
// ============================================================================

modded class CarScript
{
	protected int m_ExorLastActiveMs;
	// Si esta cubierto: id que lo liga con su cobertura (persistido)
	protected string m_ExorCoverId;
	// Posicion original (superficie) para recuperacion (persistido)
	protected vector m_ExorOrigPos;

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			m_ExorLastActiveMs = GetGame().GetTime();
			ExorVO_Manager.RegisterVehicle(this);
			if (ExorIsCovered())
			{
				// Recien creado ya-cubierto no pasa por aca (EEInit corre antes
				// del store load); el caso real se maneja en AfterStoreLoad
				DisableSimulation(true);
			}
		}
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorCoverId);
		ctx.Write(m_ExorOrigPos);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string id;
		vector pos;
		if (!ctx.Read(id))
			return false;
		if (!ctx.Read(pos))
			return false;
		m_ExorCoverId = id;
		m_ExorOrigPos = pos;
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		if (GetGame().IsServer() && ExorIsCovered())
		{
			// Tras reinicio: seguir dormido bajo tierra y re-registrarse
			DisableSimulation(true);
			ExorVO_Manager.RegisterCoveredVehicle(m_ExorCoverId, this);
		}
	}

	bool ExorIsCovered()
	{
		return m_ExorCoverId != "";
	}

	string ExorGetCoverId()
	{
		return m_ExorCoverId;
	}

	vector ExorGetOrigPos()
	{
		return m_ExorOrigPos;
	}

	void ExorCover(string id)
	{
		m_ExorCoverId = id;
		m_ExorOrigPos = GetPosition();
		vector under = m_ExorOrigPos;
		under[1] = under[1] - 100.0;
		DisableSimulation(true);
		SetPosition(under);
		ExorVO_Manager.RegisterCoveredVehicle(id, this);
	}

	void ExorUncover()
	{
		string id = m_ExorCoverId;
		vector pos = m_ExorOrigPos;
		m_ExorCoverId = "";
		SetPosition(pos);
		DisableSimulation(false);
		m_ExorLastActiveMs = GetGame().GetTime();
		ExorVO_Manager.UnregisterCoveredVehicle(id);
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

// La red camo colocada donde estaba el auto
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
		if (GetGame().IsServer())
		{
			ExorVO_Manager.UnregisterCoverId(m_ExorVehId);
		}
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
			ExorVO_Manager.RegisterCoverId(m_ExorVehId, this);
		}
	}

	void ExorSetup(string vehId, string vehType)
	{
		m_ExorVehId = vehId;
		m_ExorVehType = vehType;
		ExorSpawnStaticCar();
		ExorVO_Manager.RegisterCoverId(m_ExorVehId, this);
	}

	string ExorGetVehicleId()
	{
		return m_ExorVehId;
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
		if (car.ExorIsCovered())
			return;

		string id = ExorVO_Serializer.GenerateId();
		vector pos = car.GetPosition();
		vector ori = car.GetOrientation();
		string vtype = car.GetType();

		Exor_VehicleCover cover = Exor_VehicleCover.Cast(GetGame().CreateObjectEx("Exor_VehicleCover", pos, ECE_KEEPHEIGHT));
		if (!cover)
		{
			Print(string.Format("%1 ERROR: no se pudo crear Exor_VehicleCover; %2 queda activo", ExorStorageConstants.LOG, vtype));
			return;
		}
		cover.SetPosition(pos);
		cover.SetOrientation(ori);
		cover.ExorSetup(id, vtype);

		car.ExorCover(id);
		Print(string.Format("%1 Vehiculo %2 cubierto (id %3) - dormido bajo tierra, CE lo sigue contando", ExorStorageConstants.LOG, vtype, id));
	}

	static void UncoverVehicle(Exor_VehicleCover cover)
	{
		if (!cover)
			return;

		string id = cover.ExorGetVehicleId();
		CarScript car = ExorVO_Manager.GetCoveredVehicle(id);
		if (!car)
		{
			Print(string.Format("%1 ALERTA: cobertura %2 sin vehiculo asociado (perdido?); se elimina la cobertura", ExorStorageConstants.LOG, id));
			GetGame().ObjectDelete(cover);
			return;
		}

		vector pos = cover.GetPosition();
		vector ori = cover.GetOrientation();
		GetGame().ObjectDelete(cover);

		car.SetOrientation(ori);
		car.ExorUncover();
		car.SetPosition(pos);

		Print(string.Format("%1 Vehiculo %2 descubierto (id %3)", ExorStorageConstants.LOG, car.GetType(), id));
	}
}
