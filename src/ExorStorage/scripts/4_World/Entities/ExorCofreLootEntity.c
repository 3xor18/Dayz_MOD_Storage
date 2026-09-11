// ============================================================================
//  3xor_Vanilla_Optimization - COFRE DE LOOT (entidad)
// ----------------------------------------------------------------------------
//  El baul marino vanilla retexturizado a camuflado. Va FIJO en el piso: no se
//  agarra con las manos, no se guarda en una mochila y no entra al baul de un
//  auto. Se abre reventandolo a golpes de melee o a tiros (cuantos, en
//  cofres_loot.json), y el loot se crea RECIEN en ese momento.
//
//  Lleva humo de color y luz de chemlight, para que se vea de lejos que no es un
//  baul cualquiera sino un cofre del evento.
//
//  El estado abierto/cerrado va SINCRONIZADO al cliente porque de el depende que
//  el cargo se pueda ver: mientras esta cerrado, CanDisplayCargo() devuelve false
//  y el jugador no ve -ni toca- lo que hay adentro.
//
//  POR QUE EL HUMO Y LA LUZ VIAJAN EN UN SOLO ENTERO
//  cofres_loot.json vive SOLO en el server (no se sincroniza como el bundle de
//  config del cliente), asi que el cliente no tiene de donde sacar de que color
//  va el humo. Se manda en un unico int sincronizado (humo*10 + luz), en vez de
//  abrir un RPC o sumar dos variables de red mas por cofre.
//
//  COSTO POR FRAME: cero. La entidad no tiene tick propio; solo reacciona a
//  EEHitBy (que corre unicamente cuando alguien le pega). El ciclo de vida
//  -aparecer, despawnear, reaparecer- lo lleva el manager ExorCofreLoot con un
//  latido lento de 30 s.
// ============================================================================
class Exor_CofreLoot : Container_Base
{
	// sincronizado: de esto depende que se vea (y se pueda tocar) el cargo
	bool m_ExorAbierto;
	// sincronizado: humo*10 + luz (0 = sin nada). Ver ExorCfgCofreLoot.CodigoFx()
	int m_ExorFx;

	// ---- solo cliente ----
	protected Particle m_ExorHumoFx;		// humo (Particle es Object: lo gestiona el motor, sin ref)
	protected ScriptedLightBase m_ExorLuz;	// luz de chemlight

	// ---- solo server ----
	float m_ExorProgreso;		// 0..1 ; a 1 se abre. Fraccionario: mezclar balas y golpes suma
	int   m_ExorUltimoHitMs;	// dedup de impactos del MISMO disparo (perdigones de escopeta)
	int   m_ExorUltimoAvisoPct;	// ultimo porcentaje avisado al que le pega (para no spamear)
	int   m_ExorUltimoAvisoMeleeMs;	// ultimo "esto es a tiros" (idem: para no spamear)
	int   m_ExorPunto;			// indice de la posicion de cofres_loot.json (-1 = suelto/admin)
	int   m_ExorBorrarEnMs;		// uptime ms en que se borra si nadie lo vacio (0 = sin deadline)
	string m_ExorTipo;			// nombre de la tabla de loot que le toco al spawnear

	void Exor_CofreLoot()
	{
		RegisterNetSyncVariableBool("m_ExorAbierto");
		RegisterNetSyncVariableInt("m_ExorFx", 0, 99);
		m_ExorPunto = -1;
		m_ExorTipo = "";
	}

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			// El motor NO tiene que poder romperlo ni arruinarlo: la "vida" del cofre es
			// nuestro contador de progreso, no sus hitpoints. Con hitpoints reales, 20 tiros
			// lo dejarian arruinado (y el cargo inutilizable) antes de que se abra.
			SetHealth("", "", GetMaxHealth("", "Health"));
		}
	}

	override void EEDelete(EntityAI parent)
	{
		ExorApagarFx();
		if (GetGame() && GetGame().IsServer())
			ExorCofreLoot.Get().OnCofreBorrado(this);
		super.EEDelete(parent);
	}

	// ------------------------------------------------------------------------
	//  NO SE AGARRA NI SE MUEVE
	// ------------------------------------------------------------------------
	override bool IsTakeable()
	{
		return false;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	override bool CanPutInCargo(EntityAI parent)
	{
		return false;
	}

	// ------------------------------------------------------------------------
	//  CERRADO = NO SE VE NI SE TOCA EL CONTENIDO
	// ------------------------------------------------------------------------
	override bool CanDisplayCargo()
	{
		return m_ExorAbierto;
	}

	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (!m_ExorAbierto)
			return false;
		return super.CanReceiveItemIntoCargo(item);
	}

	// OJO: el parametro se tiene que llamar 'cargo' como en el prototipo de EntityAI.
	// Con otro nombre el compilador tira 'Can't find variable' y NO compila el modulo World.
	override bool CanReleaseCargo(EntityAI cargo)
	{
		if (!m_ExorAbierto)
			return false;
		return super.CanReleaseCargo(cargo);
	}

	// ------------------------------------------------------------------------
	//  GOLPES Y TIROS
	// ------------------------------------------------------------------------
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source,
		int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		if (!GetGame() || !GetGame().IsServer())
			return;
		// que no se arruine por el camino: el daño real no decide nada aca
		SetHealth("", "", GetMaxHealth("", "Health"));
		ExorCofreLoot.Get().Impacto(this, damageType, source);
	}

	// El cofre NO persiste a proposito: lo crea y lo borra el manager en cada arranque
	// (ver ExorCofreLoot.LimpiarHuerfanos). Guardar su estado solo serviria para
	// resucitar cofres a medio abrir y para llenar la persistencia de entidades que el
	// modulo iba a recrear igual.
	void ExorAbrirVisual()
	{
		m_ExorAbierto = true;
		SetSynchDirty();
	}

	// SERVER: fija que humo y que luz le tocan (viajan juntos en un solo entero)
	void ExorSetFx(int codigo)
	{
		m_ExorFx = codigo;
		SetSynchDirty();
	}

	// ------------------------------------------------------------------------
	//  HUMO Y LUZ (cliente)
	// ------------------------------------------------------------------------
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		ExorActualizarFx();
	}

	void ExorActualizarFx()
	{
		if (!GetGame() || !GetGame().IsClient())
			return;
		int humo = m_ExorFx / 10;
		int luz = m_ExorFx % 10;

		if (humo > 0 && !m_ExorHumoFx)
		{
			vector p = GetPosition();
			p[1] = p[1] + 0.4;	// arriba de la tapa, para que no salga desde adentro del piso
			m_ExorHumoFx = Particle.PlayInWorld(ExorParticulaHumo(humo), p);
		}
		else if (humo == 0 && m_ExorHumoFx)
		{
			m_ExorHumoFx.Stop();
			m_ExorHumoFx = null;
		}

		if (luz > 0 && !m_ExorLuz)
		{
			m_ExorLuz = ScriptedLightBase.CreateLight(ChemlightLight, "0 0 0");
			if (m_ExorLuz)
			{
				ExorColorLuz(ChemlightLight.Cast(m_ExorLuz), luz);
				m_ExorLuz.AttachOnObject(this, "0 0.4 0", "0 0 0");
			}
		}
		else if (luz == 0 && m_ExorLuz)
		{
			GetGame().ObjectDelete(m_ExorLuz);
			m_ExorLuz = null;
		}
	}

	void ExorApagarFx()
	{
		if (m_ExorHumoFx)
		{
			m_ExorHumoFx.Stop();
			m_ExorHumoFx = null;
		}
		if (m_ExorLuz)
		{
			GetGame().ObjectDelete(m_ExorLuz);
			m_ExorLuz = null;
		}
	}

	int ExorParticulaHumo(int idx)
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

	void ExorColorLuz(ChemlightLight l, int idx)
	{
		if (!l)
			return;
		if (idx == 2)
			l.SetColorToRed();
		else if (idx == 3)
			l.SetColorToBlue();
		else if (idx == 4)
			l.SetColorToYellow();
		else if (idx == 5)
			l.SetColorToWhite();
		else
			l.SetColorToGreen();
	}
}

// ============================================================================
//  SIN ESTO EL MELEE NO LE PEGA AL COFRE
// ----------------------------------------------------------------------------
//  Medido en el server local: 20 balas lo abrian y los cuchillazos no
//  registraban NI UN impacto. No era que el daño fuera poco: EEHitBy no se
//  llamaba nunca.
//
//  El motivo esta en DayZPlayerImplementMeleeCombat: el golpe solo elige como
//  objetivo lo que este en dos listas. La de alineado son jugadores, infectados
//  y animales; la de objetos grandes (3er pase) son Building, Transport,
//  CarWheel, CarDoor, TentBase y BaseBuildingBase. Un contenedor no esta en
//  ninguna de las dos, asi que el golpe no tiene a quien pegarle. Por eso una
//  carpa SI se rompe a hachazos y un baul marino no.
//
//  Se agrega la clase del cofre a la lista de objetos grandes. Toca SOLO a esta
//  clase: el resto del melee del juego queda exactamente igual.
// ============================================================================
modded class DayZPlayerImplementMeleeCombat
{
	void DayZPlayerImplementMeleeCombat(DayZPlayerImplement pPlayer)
	{
		if (m_NonAlignableObjects)
			m_NonAlignableObjects.Insert(Exor_CofreLoot);
	}
}
