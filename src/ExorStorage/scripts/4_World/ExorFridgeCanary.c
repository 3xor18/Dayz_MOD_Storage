// ============================================================================
// 3xor_Vanilla_Optimization - WIPE UNICO de neveras (limpieza de data corrupta)
// ============================================================================
// PROBLEMA: habia neveras con persistencia CORRUPTA (`Scripted variables corrupted upon
// "Exor_Fridge"` / `Corrupted inventory`) que hacian crashear el server al cargar. No se
// puede identificar cual desde script antes de que crashee (varios intentos fallaron).
//
// SOLUCION (elegida por el user): un WIPE UNICO. En la 1ra pasada tras este cambio, TODAS
// las neveras se DESCARTAN (return false en OnStoreLoad -> el motor no las carga -> se purgan
// de la persistencia en el guardado; el self-heal NO las recrea -> quedan eliminadas). Al
// completarse (apagado limpio, que garantiza el guardado ya purgado), se escribe un MARCADOR.
// De ahi en adelante las neveras cargan NORMAL y funcionan sin perder loot. La corrupcion no
// vuelve porque la nevera ya NO acepta contenedores anidados (ollas/Pot) en ExorCanStore.
//
// El wipe se dispara si: el marcador guardado < GEN (wipe pendiente) O el flag manual
// saltear_carga_neveras esta en true (valvula de emergencia). MarkDone corre en OnMissionFinish
// (apagado limpio) -> solo se "finaliza" DESPUES de un guardado durable (data ya purgada). Si el
// server se mata sin apagado limpio, el marcador NO se escribe -> re-wipea al proximo arranque
// (inofensivo: una nevera descartada nunca crashea). SOLO server.
//
// Para forzar OTRO wipe unico en el futuro: subir GEN.
// ============================================================================
class ExorFridgeWipe
{
	static const int GEN = 1;	// subir para forzar un nuevo wipe unico de neveras

	static string PathFor()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\fridges_wiped.txt";
	}

	// generacion de wipe ya COMPLETADA (0 si nunca se hizo)
	static int DoneGen()
	{
		string path = PathFor();
		if (!FileExist(path))
			return 0;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return 0;
		string line = "";
		FGets(fh, line);
		CloseFile(fh);
		line.Trim();
		if (line == "")
			return 0;
		return line.ToInt();
	}

	// marca el wipe GEN como completado. Se llama en OnMissionFinish (apagado limpio) -> la data
	// ya se purgo en el guardado -> a partir del proximo arranque las neveras cargan normal.
	static void MarkDone()
	{
		if (DoneGen() >= GEN)
			return;
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		FileHandle fh = OpenFile(PathFor(), FileMode.WRITE);
		if (fh == 0)
			return;
		FPrintln(fh, GEN.ToString());
		CloseFile(fh);
		Print(string.Format("%1 WIPE de neveras COMPLETADO (gen %2) -> de ahora en adelante cargan normal", ExorStorageConstants.LOG, GEN));
	}

	// true = hay que ELIMINAR las neveras (wipe unico pendiente, o el flag manual activo)
	static bool ShouldWipe()
	{
		if (GetExorConfig() && GetExorConfig().storage.saltear_carga_neveras)
			return true;
		return DoneGen() < GEN;
	}
}
