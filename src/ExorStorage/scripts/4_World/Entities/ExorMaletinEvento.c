// ============================================================================
//  3xor_Vanilla_Optimization - MALETIN DEL EVENTO (entidad)
// ----------------------------------------------------------------------------
//  El maletin tecnico vanilla, usado como objetivo del evento de escolta: aparece
//  en un punto de inicio con humo, y hay que llevarlo hasta el punto de fin.
//
//  Mientras esta en el piso emite el humo del evento. El humo NO es un objeto
//  aparte: lo emite el propio maletin, asi que cuando alguien lo levanta el humo
//  se apaga solo -que es exactamente la regla del evento- sin que el manager
//  tenga que ir a buscar y borrar una entidad de humo.
//
//  El indice de color viaja sincronizado porque evento_maletin.json vive solo en
//  el server: el cliente no tiene de donde sacar de que color va el humo.
// ============================================================================
class Exor_MaletinEvento : Container_Base
{
	int m_ExorHumo;		// sincronizado: 0 = sin humo, 1..6 = color (ver ExorHumoFx)

	protected Particle m_ExorHumoFx;	// cliente (Particle es Object: lo gestiona el motor, sin ref)

	void Exor_MaletinEvento()
	{
		RegisterNetSyncVariableInt("m_ExorHumo", 0, 9);
	}

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			SetAllowDamage(false);	// el objetivo del evento no se rompe a balazos
	}

	// El maletin NO guarda cosas: es un objetivo, no una mochila. Sin esto se podria
	// usar como bolso extra y encima se perderia el contenido cuando el evento lo borra.
	override bool CanDisplayCargo()
	{
		return false;
	}

	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		return false;
	}

	// TODO movimiento del maletin pasa por aca: lo levantan, lo tiran, lo pasan a otro,
	// lo mete el motor en la bolsa de cadaver al morir el que lo llevaba. El manager
	// decide que significa cada caso; la entidad solo avisa.
	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		super.EEItemLocationChanged(oldLoc, newLoc);
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorMaletin.Get().OnMaletinMovido(this);
	}

	override void EEDelete(EntityAI parent)
	{
		ApagarHumo();
		super.EEDelete(parent);
	}

	// SERVER: prende/apaga el humo (0 = apagado)
	void ExorSetHumo(int idx)
	{
		m_ExorHumo = idx;
		SetSynchDirty();
	}

	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		ActualizarHumo();
	}

	void ActualizarHumo()
	{
		if (!GetGame() || !GetGame().IsClient())
			return;
		if (m_ExorHumo > 0 && !m_ExorHumoFx)
		{
			vector p = GetPosition();
			p[1] = p[1] + 0.3;
			m_ExorHumoFx = Particle.PlayInWorld(ExorHumoFx.ParticulaDe(m_ExorHumo), p);
		}
		else if (m_ExorHumo == 0 && m_ExorHumoFx)
		{
			ApagarHumo();
		}
	}

	void ApagarHumo()
	{
		if (m_ExorHumoFx)
		{
			m_ExorHumoFx.Stop();
			m_ExorHumoFx = null;
		}
	}
}

// ============================================================================
//  BALIZA DEL PUNTO DE ENTREGA
// ----------------------------------------------------------------------------
//  Hereda de la luz del evento de cofres (bengala fija, no agarrable) y le suma
//  el humo del mismo color que el del maletin, para que el punto de entrega se
//  vea de lejos igual que el de inicio. La borra el manager cuando llega el
//  maletin, asi que no hay que apagar nada a mano.
// ============================================================================
class Exor_HumoEvento : Exor_CofreLight
{
	int m_ExorHumo;		// sincronizado: 0 = sin humo, 1..6 = color

	protected Particle m_ExorHumoFx;

	void Exor_HumoEvento()
	{
		RegisterNetSyncVariableInt("m_ExorHumo", 0, 9);
	}

	void ExorSetHumo(int idx)
	{
		m_ExorHumo = idx;
		SetSynchDirty();
	}

	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		if (!GetGame() || !GetGame().IsClient())
			return;
		if (m_ExorHumo > 0 && !m_ExorHumoFx)
		{
			vector p = GetPosition();
			p[1] = p[1] + 0.3;
			m_ExorHumoFx = Particle.PlayInWorld(ExorHumoFx.ParticulaDe(m_ExorHumo), p);
		}
		else if (m_ExorHumo == 0 && m_ExorHumoFx)
		{
			m_ExorHumoFx.Stop();
			m_ExorHumoFx = null;
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (m_ExorHumoFx)
		{
			m_ExorHumoFx.Stop();
			m_ExorHumoFx = null;
		}
		super.EEDelete(parent);
	}
}

// ----------------------------------------------------------------------------
//  Traduccion color -> particula de humo, compartida por el cofre y el maletin.
// ----------------------------------------------------------------------------
class ExorHumoFx
{
	static int IdxDe(string color)
	{
		string c = color;
		c.ToLower();
		if (c == "amarillo")
			return 2;
		if (c == "verde")
			return 3;
		if (c == "morado")
			return 4;
		if (c == "rojo")
			return 5;
		if (c == "negro")
			return 6;
		return 1;	// blanco
	}

	static int ParticulaDe(int idx)
	{
		if (idx == 2)
			return ParticleList.GRENADE_M18_YELLOW_LOOP;
		if (idx == 3)
			return ParticleList.GRENADE_M18_GREEN_LOOP;
		if (idx == 4)
			return ParticleList.GRENADE_M18_PURPLE_LOOP;
		if (idx == 5)
			return ParticleList.GRENADE_M18_RED_LOOP;
		if (idx == 6)
			return ParticleList.GRENADE_M18_BLACK_LOOP;
		return ParticleList.GRENADE_M18_WHITE_LOOP;
	}
}
