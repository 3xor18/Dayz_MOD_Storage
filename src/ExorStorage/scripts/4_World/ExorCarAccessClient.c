// ============================================================================
// 3xor_Vanilla_Optimization - ACCESO al baul del auto (cache CLIENTE)
// ----------------------------------------------------------------------------
// El baul de un auto lockeado NO se le muestra al que no tiene acceso. La decision
// (CanDisplayCargo) corre en el CLIENTE y muchas veces, asi que tiene que ser O(1):
//   - un set de network-ids de autos a los que ESTE cliente tiene acceso (metio la
//     clave OK -> el server le manda un grant por RPC), y
//   - un bool "soy admin" (del config sincronizado), que ve todos los baules.
// Nada de lookups de grupo ni JSON aca: solo un Find en un set y una comparacion.
//
// El set es de SESION (se vacia al reconectar). Si te expulsan del clan estando online
// el gate de MANEJAR ya te frena; el baul se te vuelve a ocultar al reconectar (el set
// arranca vacio y no te llega grant porque ya no sos miembro). Ventana chica y benigna.
// ============================================================================
class ExorCarAccessClient
{
	static ref set<string> s_Access;	// network-ids "low_high" con acceso (metio la clave)
	static ref set<string> s_Member;	// network-ids de autos donde el player local ES MIEMBRO del clan dueño
	static bool s_IsAdmin;

	static void EnsureSet()
	{
		if (!s_Access)
			s_Access = new set<string>();
		if (!s_Member)
			s_Member = new set<string>();
	}

	// clave = network-id del objeto (estable dentro de la sesion)
	static string KeyOf(Object o)
	{
		if (!o)
			return "";
		int low, high;
		o.GetNetworkID(low, high);
		return string.Format("%1_%2", low, high);
	}

	static bool HasAccess(Object car)
	{
		if (s_IsAdmin)
			return true;
		EnsureSet();
		return s_Access.Find(KeyOf(car)) >= 0;
	}

	static void Grant(string key)
	{
		if (key == "")
			return;
		EnsureSet();
		if (s_Access.Find(key) < 0)
			s_Access.Insert(key);
	}

	// el player local es MIEMBRO del clan dueño de este auto? (para mostrar "Ingresar clave"
	// solo a miembros; el ajeno ve solo "Quitar Codelock"). Los admins cuentan como con acceso.
	static bool IsMember(Object car)
	{
		if (s_IsAdmin)
			return true;
		EnsureSet();
		return s_Member.Find(KeyOf(car)) >= 0;
	}

	static void MarkMember(string key)
	{
		if (key == "")
			return;
		EnsureSet();
		if (s_Member.Find(key) < 0)
			s_Member.Insert(key);
	}

	// el player local es admin del candado? (se llama al recibir el config sync)
	static void RefreshAdmin()
	{
		s_IsAdmin = false;
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (!p)
			return;
		string sid = ExorGroupManager.SteamId(p);
		s_IsAdmin = GetExorConfig().carlock.ExorEsAdmin(sid);
	}
}
