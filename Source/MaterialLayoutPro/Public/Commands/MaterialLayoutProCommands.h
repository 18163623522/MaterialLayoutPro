#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/ISlateStyle.h"

class FMaterialLayoutProCommands : public TCommands<FMaterialLayoutProCommands>
{
public:
	FMaterialLayoutProCommands()
		: TCommands<FMaterialLayoutProCommands>(
			TEXT("MaterialLayoutPro"),
			NSLOCTEXT("Contexts", "MaterialLayoutPro", "材质布局 Pro"),
			NAME_None,
			FName(TEXT("MaterialLayoutProStyle")))
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OpenPanel;
};
