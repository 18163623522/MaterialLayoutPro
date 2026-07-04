#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMaterialLayoutProModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterStyle();
	void UnregisterStyle();

	void RegisterCommands();
	void UnregisterCommands();

	void BindCommands();

	void RegisterTabSpawners();
	void UnregisterTabSpawners();

	void RegisterMenus();

	TSharedPtr<class FUICommandList> PluginCommandList;
	bool bIsShuttingDown;
};
