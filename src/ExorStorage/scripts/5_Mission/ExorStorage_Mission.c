// ============================================================================
// 3xorStorage - Init del lado server: carga settings.json del profile
// ============================================================================
modded class MissionServer
{
	override void OnInit()
	{
		super.OnInit();

		ExorStorageSettings settings = GetExorStorageSettings();
		Print("[3xorStorage] v" + ExorStorageConstants.MOD_VERSION + " inicializado");
		Print("[3xorStorage] virtualizar_minutos=" + settings.virtualizar_minutos
			+ " multiplicador_comida=" + settings.multiplicador_comida
			+ " stack_municion_default=" + settings.stack_municion_default);
	}
}
