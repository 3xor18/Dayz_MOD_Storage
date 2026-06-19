// ============================================================================
// 3xor_Vanilla_Optimization - Log de auditoria del server (SOLO server)
// Escribe 1 archivo por dia en $profile:3xorVanillaOptimization\ServerAuditLog\:
//   audit_YYYY-MM-DD.txt   (1 evento por linea)
// Auto-purga: al arrancar borra los archivos mas viejos que log_dias_retener.
// Formato de linea:
//   [YYYY-MM-DD HH:MM:SS] EVENTO | steam=<id> | jugador=<nombre> | pos=<X, Z> | <detalle>
// OJO: la hora/fecha es la del RELOJ DEL HOST del server (igual que el .ADM/.RPT).
// ============================================================================
class ExorRaidLog
{
	// Crea la carpeta y purga lo viejo. Llamar 1 vez en MissionServer.OnInit.
	static void Init()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!FileExist(ExorStorageConstants.AUDITLOG_DIR))
			MakeDirectory(ExorStorageConstants.AUDITLOG_DIR);
		PurgeOld();
	}

	static string Pad2(int v)
	{
		if (v < 10)
			return "0" + v.ToString();
		return v.ToString();
	}

	// YYYY-MM-DD (para el nombre de archivo del dia)
	static string DayStamp()
	{
		int y, mo, d;
		GetYearMonthDay(y, mo, d);
		return string.Format("%1-%2-%3", y, Pad2(mo), Pad2(d));
	}

	// YYYY-MM-DD HH:MM:SS (para la marca de tiempo de la linea)
	static string TimeStamp()
	{
		int y, mo, d;
		GetYearMonthDay(y, mo, d);
		int h, mi, s;
		GetHourMinuteSecond(h, mi, s);
		return string.Format("%1-%2-%3 %4:%5:%6", y, Pad2(mo), Pad2(d), Pad2(h), Pad2(mi), Pad2(s));
	}

	static string FilePath(string dayStamp)
	{
		return string.Format("%1\\audit_%2.txt", ExorStorageConstants.AUDITLOG_DIR, dayStamp);
	}

	// Escribe una linea de evento. pos se loguea como <X, Z> (convencion del .ADM).
	static void Write(string evento, string steamid, string nombre, vector pos, string detalle)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!FileExist(ExorStorageConstants.AUDITLOG_DIR))
			MakeDirectory(ExorStorageConstants.AUDITLOG_DIR);

		string path = FilePath(DayStamp());
		FileHandle fh = OpenFile(path, FileMode.APPEND);
		if (!fh)
			return;

		int rx = Math.Round(pos[0]);
		int rz = Math.Round(pos[2]);
		string line = string.Format("[%1] %2 | steam=%3 | jugador=%4 | pos=<%5, %6> | %7",
			TimeStamp(), evento, steamid, nombre, rx, rz, detalle);
		FPrintln(fh, line);
		CloseFile(fh);
	}

	// ------------------------- retencion -------------------------
	static void PurgeOld()
	{
		int keepDays = GetExorConfig().party.proteccion.log_dias_retener;
		if (keepDays <= 0)
			return;	// 0 = nunca borrar

		int today = ExorTimeUtil.TodayNumber();
		string pattern = string.Format("%1\\audit_*.txt", ExorStorageConstants.AUDITLOG_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (h)
		{
			if (fileName != "")
				PurgeIfOld(fileName, today, keepDays);
			while (FindNextFile(h, fileName, attr))
			{
				if (fileName != "")
					PurgeIfOld(fileName, today, keepDays);
			}
			CloseFindFile(h);
		}
	}

	static void PurgeIfOld(string fileName, int today, int keepDays)
	{
		// fileName esperado: raid_YYYY-MM-DD.txt  (longitud 19)
		int y, mo, d;
		if (!ParseDate(fileName, y, mo, d))
			return;
		int fileDay = ExorTimeUtil.DayNumber(y, mo, d);
		if (today - fileDay >= keepDays)
		{
			DeleteFile(string.Format("%1\\%2", ExorStorageConstants.AUDITLOG_DIR, fileName));
			Print(string.Format("%1 RaidLog: %2 borrado (mas de %3 dias)", ExorStorageConstants.LOG, fileName, keepDays));
		}
	}

	// Extrae Y/M/D de "audit_YYYY-MM-DD.txt" por posiciones fijas (prefijo "audit_" = 6 chars).
	static bool ParseDate(string fileName, out int y, out int mo, out int d)
	{
		if (fileName.Length() < 20)
			return false;
		string sy = fileName.Substring(6, 4);
		string sm = fileName.Substring(11, 2);
		string sd = fileName.Substring(14, 2);
		y = sy.ToInt();
		mo = sm.ToInt();
		d = sd.ToInt();
		if (y <= 0 || mo <= 0 || d <= 0)
			return false;
		return true;
	}
}
