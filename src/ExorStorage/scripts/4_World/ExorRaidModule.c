// ============================================================================
//  3xor_Vanilla_Optimization - MODULO DE RAID (SOLO server)
// ============================================================================
// Permite reventar puertas/portones de BaseBuildingPlus con EXPLOSIVOS y BALAS,
// y solo dentro de la ventana horaria de raid.json.
//
// ⭐ POR QUE NO SE USA EL SISTEMA DE DANO
// Las clases de BBP NO declaran DamageSystem, ni GlobalHealth, ni hitpoints, ni
// armor (verificado en su config: solo physLayer y carveNavmesh). Sin puntos de
// vida el motor no tiene a que aplicarle dano: una granada explota contra una
// pared BBP y NO PASA NADA, a ninguna hora y con cualquier config. Por eso este
// modulo NO intercepta dano: lleva su PROPIO contador por pieza.
//
// Como se detecta el impacto:
//   - EXPLOSIVO: una granada lanzada se BORRA del mundo justo al estallar. Ese
//     borrado (EEDelete, sin dueno en la jerarquia) es la senal. Sirve aunque el
//     blast quede tapado por una estructura. Mismo truco que usa NoWallDamage.
//   - BALA: se cuenta en el EEHitBy de la pieza. ⚠️ SIN VERIFICAR: como BBP no
//     tiene DamageSystem, puede que el motor NUNCA dispare EEHitBy sobre estas
//     clases. Es lo PRIMERO a comprobar in-game; si no llega, las balas hay que
//     hacerlas por otro camino (o dejarlas afuera).
//
// El contador es FRACCIONARIO: cada impacto resta 1/cantidad (escalado por el
// tier de la pieza), asi que mezclar explosivos sale gratis: 3 granadas de 5
// (0,6) + 1 plastico de 1 (1,0) = 1,6 -> revienta. Al llegar a 1.0 vuela SOLO LA
// HOJA de la puerta (el marco queda en pie, si no el raidero se queda sin poder
// entrar/subir) mas su candado, y se escribe el desglose en el audit.
// Ver ExorRaidBBP.DestruirPuerta.
//
// El contador vive en RAM y se reinicia con el server. Alcanza porque la ventana
// (sabado 20:00-00:00) NO cruza ningun reinicio (02:00 / 10:00 / 17:00). Si algun
// dia la ventana cruza un reinicio, hay que persistirlo por posicion como los
// candados de autos (ver ExorCarLockStore).
// ============================================================================

class ExorRaidHit
{
	string clase;	// classname del explosivo (o "Bala")
	int    veces;
}

class ExorRaidProgreso
{
	float progreso;					// 0..1 ; a 1 la pieza cae
	int   ultimoMs;					// para expirar el progreso viejo
	ref array<ref ExorRaidHit> golpes;	// desglose para el log

	void ExorRaidProgreso()
	{
		golpes = new array<ref ExorRaidHit>;
	}

	void Anotar(string clase)
	{
		int i;
		for (i = 0; i < golpes.Count(); i++)
		{
			if (golpes.Get(i).clase == clase)
			{
				golpes.Get(i).veces = golpes.Get(i).veces + 1;
				return;
			}
		}
		ExorRaidHit h = new ExorRaidHit;
		h.clase = clase;
		h.veces = 1;
		golpes.Insert(h);
	}

	// OJO: 'out' es palabra reservada de Enforce (modificador de parametro), no se puede
	// usar como nombre de variable: rompe la compilacion con "Broken expression".
	string Desglose()
	{
		string txt = "";
		int i;
		for (i = 0; i < golpes.Count(); i++)
		{
			if (txt != "")
				txt = txt + ", ";
			txt = txt + string.Format("%1x %2", golpes.Get(i).veces, golpes.Get(i).clase);
		}
		return txt;
	}
}

class ExorRaidModule
{
	// progreso por pieza. La clave es la posicion redondeada, igual criterio que
	// ExorCarLockStore: no se toca el stream de persistencia de la pieza (agregar
	// campos al OnStoreSave de una entidad corrompe lo ya guardado).
	static ref map<string, ref ExorRaidProgreso> s_Progreso;

	// dedup: un mismo estallido puede llegar dos veces (el objeto se borra y ademas
	// dispara dano de area). Se ignora el mismo explosivo dentro de esta ventana.
	static const int DEDUP_MS = 1500;
	static ref map<string, int> s_UltimoImpacto;

	// El progreso se olvida si la pieza no recibe nada por un rato: evita que alguien
	// deje una puerta a medio raidear un sabado y la termine el sabado siguiente.
	static const int EXPIRA_MS = 30 * 60 * 1000;	// 30 min

	static string ClavePos(vector pos)
	{
		return string.Format("%1_%2", Math.Round(pos[0]), Math.Round(pos[2]));
	}

	static ExorRaidProgreso ProgresoDe(string clave, int ahora)
	{
		if (!s_Progreso)
			s_Progreso = new map<string, ref ExorRaidProgreso>;
		ExorRaidProgreso p;
		if (s_Progreso.Find(clave, p) && p)
		{
			if ((ahora - p.ultimoMs) > EXPIRA_MS)
				p = null;	// caducado -> arranca de cero
			else
				return p;
		}
		p = new ExorRaidProgreso;
		p.progreso = 0;
		p.ultimoMs = ahora;
		s_Progreso.Set(clave, p);
		return p;
	}

	// cuanto hace falta de 'clase' para tirar ESTA pieza (ya escalado por tier)
	static float CantidadNecesaria(ExorCfgRaidEstructura e, int tier, string clase, bool esBala)
	{
		float base_ = 0;
		if (esBala)
		{
			base_ = e.balas_cantidad;
		}
		else
		{
			int i;
			for (i = 0; i < e.explosivos.Count(); i++)
			{
				ExorCfgRaidExplosivo x = e.explosivos.Get(i);
				if (x && x.classname == clase)
				{
					base_ = x.cantidad;
					break;
				}
			}
		}
		if (base_ <= 0)
			return 0;	// 0 = esta cosa NO raidea

		float mult = 1.0;
		if (e.multiplicador_por_tier && e.multiplicador_por_tier.Count() > 0)
		{
			int idx = tier - 1;
			if (idx < 0)
				idx = 0;
			if (idx >= e.multiplicador_por_tier.Count())
				idx = e.multiplicador_por_tier.Count() - 1;
			mult = e.multiplicador_por_tier.Get(idx);
			if (mult <= 0)
				mult = 1.0;
		}
		return base_ * mult;
	}

	// Registra un impacto sobre 'obj'. Devuelve true si la pieza se destruyo.
	static bool Impacto(Object obj, string clase, bool esBala, vector posImpacto, EntityAI fuente)
	{
		if (!GetGame() || !GetGame().IsServer() || !obj)
			return false;

		ExorCfgRaid r = GetExorConfig().raid;
		if (!r || !r.activado)
			return false;
		if (!ExorMuebleRules.IsLootFreeNow())
			return false;	// fuera de la ventana, las piezas son intocables

		ExorCfgRaidEstructura e = r.BuscarEstructura(obj.GetType());
		if (!e)
			return false;	// esta clase no es raideable (pared, ventana, techo...)
		// se re-chequea aca y no solo en la busqueda por cercania, porque el hook de BALAS
		// entra derecho con la pieza que recibio el tiro, sin pasar por PiezaMasCercana.
		if (!ExorRaidBBP.EsObjetivoValido(obj, e))
			return false;	// marco al que ya le volaron la puerta

		int tier = ExorRaidBBP.TierDe(obj);
		float necesarias = CantidadNecesaria(e, tier, clase, esBala);
		if (necesarias <= 0)
			return false;

		int ahora = GetGame().GetTime();

		// dedup por explosivo: el mismo estallido no cuenta dos veces
		if (fuente)
		{
			if (!s_UltimoImpacto)
				s_UltimoImpacto = new map<string, int>;
			string kf = string.Format("%1", fuente.GetID());
			int prev;
			if (s_UltimoImpacto.Find(kf, prev) && (ahora - prev) < DEDUP_MS)
				return false;
			s_UltimoImpacto.Set(kf, ahora);
		}

		string clave = ClavePos(obj.GetPosition());
		ExorRaidProgreso p = ProgresoDe(clave, ahora);
		p.progreso = p.progreso + (1.0 / necesarias);
		p.ultimoMs = ahora;
		p.Anotar(clase);

		if (r.log_cada_impacto)
		{
			// OJO: string.Format de Enforce NO entiende "%%" como un porcentaje literal,
			// se lo come y pega los textos ("98(hacen falta 450)"). Va la palabra.
			Print(string.Format("%1 RAID impacto %2 en %3 (T%4) -> %5 por ciento (hacen falta %6)",
				ExorStorageConstants.LOG, clase, obj.GetType(), tier,
				Math.Round(p.progreso * 100), necesarias));
		}

		if (p.progreso < 1.0)
			return false;

		Destruir(obj, e, p, tier, posImpacto);
		s_Progreso.Remove(clave);
		return true;
	}

	static void Destruir(Object obj, ExorCfgRaidEstructura e, ExorRaidProgreso p, int tier, vector posImpacto)
	{
		vector pos = obj.GetPosition();
		string tipo = e.tipo;
		string desglose = p.Desglose();

		// de quien era la base (misma etiqueta que el resto del audit)
		string grupoId = ExorTerritoryIndex.GrupoAjenoEn("", pos);
		string dueno = "sin territorio";
		if (grupoId != "")
			dueno = ExorAntiRaid.GroupLabel(grupoId);

		// quien fue: BEST EFFORT. Una granada lanzada ya no tiene dueno cuando estalla,
		// asi que se toma el jugador vivo mas cercano al impacto. Es un INDICIO, no una
		// prueba: si el raidero se alejo o hay un tercero mirando, puede errar.
		string sid = "";
		string nombre = "?";
		PlayerBase cerca = ExorRaidBBP.JugadorMasCercano(posImpacto, 40.0);
		if (cerca)
		{
			sid = ExorGroupManager.SteamId(cerca);
			if (cerca.GetIdentity())
				nombre = cerca.GetIdentity().GetName();
		}

		ExorRaidLog.Write("ESTRUCTURA_DESTRUIDA", sid, nombre, pos,
			string.Format("%1 T%2 de %3 | destruida con: %4 | jugador = el mas cercano al estallido (indicio, no prueba)",
				tipo, tier, dueno, desglose), true);

		Print(string.Format("%1 RAID: %2 T%3 destruida en <%4, %5> con %6",
			ExorStorageConstants.LOG, tipo, tier, Math.Round(pos[0]), Math.Round(pos[2]), desglose));

		ExorRaidBBP.DestruirPuerta(obj, e.destruir_pieza_completa != 0);
	}
}
