// ============================================================================
// 3xor_Vanilla_Optimization - Forense de tumbas (SOLO server)
// Un JSON por tumba en tumbas\<id>.json con: quien murio, posicion, fecha/hora,
// los items al morir, y quien fue looteando cada item. Config en bodycadaver.json
// (forense_habilitado / forense_registrar_looteadores / forense_retener_dias).
// NO es persistencia de la tumba (esa la maneja el engine); es SOLO forense para
// moderacion. Se limpia por retencion al arrancar.
// ============================================================================
class ExorTumbaLoot
{
	string item;
	string looter;   // "nombre (steamid)"
	string hora;     // YYYY-MM-DD HH:MM:SS (UTC del server)
}

class ExorTumbaFile
{
	string id;
	float  x;
	float  y;
	float  z;
	string muerto;         // "nombre (steamid)"
	string fecha_hora;     // muerte (UTC del server)
	int    spawn_min;      // minuto-numero de la muerte (para la retencion)
	ref array<string> items_al_morir;
	ref array<ref ExorTumbaLoot> looteado;

	void ExorTumbaFile()
	{
		items_al_morir = new array<string>;
		looteado = new array<ref ExorTumbaLoot>;
	}
}

class ExorTumbaForense
{
	static string Path(string id)
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.TUMBAS_DIR, id);
	}

	// Escribe el JSON al crear la tumba (1 sola escritura por muerte).
	static void Registrar(string id, vector pos, string muertoNombre, string muertoSid, int spawnMin, array<ref ExorVO_ItemData> loot)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!GetExorConfig().bodycadaver.forense_habilitado || id == "")
			return;
		if (!FileExist(ExorStorageConstants.TUMBAS_DIR))
			MakeDirectory(ExorStorageConstants.TUMBAS_DIR);

		ExorTumbaFile f = new ExorTumbaFile();
		f.id = id;
		f.x = pos[0];
		f.y = pos[1];
		f.z = pos[2];
		f.muerto = string.Format("%1 (%2)", muertoNombre, muertoSid);
		f.fecha_hora = ExorRaidLog.TimeStamp();
		f.spawn_min = spawnMin;

		int i;
		if (loot)
		{
			for (i = 0; i < loot.Count(); i++)
				Flatten(loot.Get(i), f.items_al_morir);
		}
		JsonFileLoader<ExorTumbaFile>.JsonSaveFile(Path(id), f);
	}

	// Aplana un item (top-level + attachments + cargo anidado) a classnames.
	static void Flatten(ExorVO_ItemData d, array<string> outp)
	{
		if (!d)
			return;
		outp.Insert(d.type);
		int i;
		if (d.attachments)
		{
			for (i = 0; i < d.attachments.Count(); i++)
				Flatten(d.attachments.Get(i), outp);
		}
		if (d.cargo)
		{
			for (i = 0; i < d.cargo.Count(); i++)
				Flatten(d.cargo.Get(i), outp);
		}
	}

	// Registra que 'looter' saco 'itemType' de la tumba 'id'. Config-gated (si lagea, off).
	static void Looteo(string id, string itemType, string looterNombre, string looterSid)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!GetExorConfig().bodycadaver.forense_registrar_looteadores || id == "")
			return;
		string p = Path(id);
		if (!FileExist(p))
			return;

		ExorTumbaFile f = new ExorTumbaFile();
		JsonFileLoader<ExorTumbaFile>.JsonLoadFile(p, f);
		ExorTumbaLoot l = new ExorTumbaLoot();
		l.item = itemType;
		l.looter = string.Format("%1 (%2)", looterNombre, looterSid);
		l.hora = ExorRaidLog.TimeStamp();
		f.looteado.Insert(l);
		JsonFileLoader<ExorTumbaFile>.JsonSaveFile(p, f);
	}

	// Borra los JSON forenses mas viejos que forense_retener_dias (por spawn_min). Al arrancar.
	static void Limpiar()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		int dias = GetExorConfig().bodycadaver.forense_retener_dias;
		if (dias <= 0)
			return;	// 0 = nunca borrar
		if (!FileExist(ExorStorageConstants.TUMBAS_DIR))
			return;

		int corte = dias * 1440;	// minutos
		int now = ExorTimeUtil.NowMinutes();
		int borrados = 0;

		string pattern = string.Format("%1\\*.json", ExorStorageConstants.TUMBAS_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (h)
		{
			if (fileName != "" && LimpiarUno(fileName, now, corte))
				borrados++;
			while (FindNextFile(h, fileName, attr))
			{
				if (fileName != "" && LimpiarUno(fileName, now, corte))
					borrados++;
			}
			CloseFindFile(h);
		}
		if (borrados > 0)
			Print(string.Format("%1 Tumbas forense: %2 archivos caducados (>%3 dias) borrados al arrancar", ExorStorageConstants.LOG, borrados, dias));
	}

	static bool LimpiarUno(string fileName, int now, int corte)
	{
		string full = string.Format("%1\\%2", ExorStorageConstants.TUMBAS_DIR, fileName);
		ExorTumbaFile f = new ExorTumbaFile();
		JsonFileLoader<ExorTumbaFile>.JsonLoadFile(full, f);
		if (f.spawn_min <= 0)
			return false;	// sin fecha valida -> no tocar
		if (now - f.spawn_min > corte)
		{
			DeleteFile(full);
			return true;
		}
		return false;
	}
}
