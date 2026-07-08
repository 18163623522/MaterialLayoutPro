#include "Commands/MaterialLayoutProCommands.h"

#define LOCTEXT_NAMESPACE "FMaterialLayoutProCommands"

void FMaterialLayoutProCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenPanel,
		"材质布局 Pro",
		"Open the Material Layout Pro parameter manager panel.",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
