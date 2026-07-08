#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

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

	/** Extend the material graph's right-click menu with a "Sync Comment to Group" action. */
	TSharedRef<FExtender> GetGraphContextMenuExtender(const TSharedRef<FUICommandList> InCommandList, const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, bool bIsDebugging);
	void OnSyncCommentToGroup(FMenuBuilder& MenuBuilder, const UEdGraph* InGraph, const UEdGraphNode* InNode);

	/** Handle to our registered graph menu extender (for cleanup). */
	FDelegateHandle GraphMenuExtenderHandle;

	TSharedPtr<class FUICommandList> PluginCommandList;
	bool bIsShuttingDown;
};
