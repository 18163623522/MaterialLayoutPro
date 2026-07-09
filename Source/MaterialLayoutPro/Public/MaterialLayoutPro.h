#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Docking/TabManager.h"

class IMaterialEditor;
class IAssetEditorInstance;
class SDockTab;

class FMaterialLayoutProModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Tab id of the embedded sidebar inside each Material Editor window. */
	static const FName EmbeddedTabId;

private:
	void RegisterStyle();
	void UnregisterStyle();

	void RegisterCommands();
	void UnregisterCommands();

	void BindCommands();

	void RegisterTabSpawners();
	void UnregisterTabSpawners();

	void RegisterMenus();

	// --- Material Editor embedding ---

	/** Called when ANY asset editor opens — filters for materials and injects the sidebar. */
	void OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* Instance);
	/** Spawns the embedded sidebar tab bound to a specific material editor. */
	TSharedRef<SDockTab> OnSpawnEmbeddedTab(const FSpawnTabArgs& Args, TWeakPtr<IMaterialEditor> InMaterialEditor);
	/** Register the sidebar tab into a material editor's tab manager. */
	void RegisterEmbeddedSidebar(IMaterialEditor* InMaterialEditor);

	// --- Graph context menu ---

	/** Extend the material graph's right-click menu with a "Sync Comment to Group" action. */
	TSharedRef<FExtender> GetGraphContextMenuExtender(const TSharedRef<FUICommandList> InCommandList, const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, bool bIsDebugging);
	void OnSyncCommentToGroup(FMenuBuilder& MenuBuilder, const UEdGraph* InGraph, const UEdGraphNode* InNode);

	/** Handle to our registered graph menu extender (for cleanup). */
	FDelegateHandle GraphMenuExtenderHandle;

	/** Asset-editor-opened delegate handle (for cleanup on shutdown). */
	FDelegateHandle AssetOpenedHandle;

	TSharedPtr<class FUICommandList> PluginCommandList;
	bool bIsShuttingDown;
};
