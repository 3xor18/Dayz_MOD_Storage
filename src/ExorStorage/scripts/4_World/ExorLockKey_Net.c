// ============================================================================
// 3xor_Vanilla_Optimization - CLAVE de lockers: cache cliente del modal
// ============================================================================
// El server abre el modal con LOCK_MODAL_OPEN (Param1 int mode). El cliente cachea el
// modo aca y el menu (ExorLockKeyMenu) lo lee al dibujar. s_Version se incrementa en cada
// apertura para que el menu, si ya estaba abierto, se redibuje en el modo nuevo.
//   mode 0 = METER clave (para abrir): 1 input.
//   mode 1 = SETEAR/CAMBIAR clave: 2 inputs (nueva + repetir, deben coincidir).
// ============================================================================
class ExorLockKeyClient
{
	static const int MODE_ENTER = 0;	// meter la clave para abrir
	static const int MODE_SET   = 1;	// poner/cambiar la clave

	static int s_Mode;
	static int s_Version;

	static void Set(int mode)
	{
		s_Mode = mode;
		s_Version++;
	}
}
