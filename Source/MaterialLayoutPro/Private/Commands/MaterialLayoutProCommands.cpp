#include "Commands/MaterialLayoutProCommands.h"

#define LOCTEXT_NAMESPACE "FMaterialLayoutProCommands"

void FMaterialLayoutProCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenPanel,
		"Material Layout Pro",
		"Open the Material Layout Pro parameter manager panel.",
		EUserInterfaceActionType::Button,
		FInputChord());
}

#undef LOCTEXT_NAMESPACE
