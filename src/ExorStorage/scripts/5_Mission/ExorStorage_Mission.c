// ============================================================================
// 3xorStorage - Init del lado server: carga settings.json del profile
// ============================================================================
modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		ExorStorageSettings settings = GetExorStorageSettings();
		// OJO: este compilador no acepta expresiones partidas en varias lineas (ni ternarios)
		Print(string.Format("[3xorStorage] v%1 inicializado", ExorStorageConstants.MOD_VERSION));
		Print(string.Format("[3xorStorage] virtualizar_minutos=%1 multiplicador_comida=%2 stack_municion_default=%3", settings.virtualizar_minutos, settings.multiplicador_comida, settings.stack_municion_default));
	}
}
