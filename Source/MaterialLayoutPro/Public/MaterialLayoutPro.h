#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Docking/TabManager.h"

class IMaterialEditor;
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

	/** Called when a Material Editor opens — registers an embedded sidebar tab. */
	void OnMaterialEditorOpened(TWeakPtr<IMaterialEditor> InMaterialEditor);
	/** Called when a Material Instance Editor opens — registers an embedded sidebar tab. */
	void OnMaterialInstanceEditorOpened(TWeakPtr<IMaterialEditor> InMaterialEditor);
	/** Spawns the embedded sidebar tab bound to a specific material editor. */
	TSharedRef<SDockTab> OnSpawnEmbeddedTab(const FSpawnTabArgs& Args, TWeakPtr<IMaterialEditor> InMaterialEditor);
	/** Register the sidebar tab into a material editor's tab manager. */
	void RegisterEmbeddedSidebar(TWeakPtr<IMaterialEditor> InMaterialEditor);

	// --- Graph context menu ---

	/** Extend the material graph's right-click menu with a "Sync Comment to Group" action. */
	TSharedRef<FExtender> GetGraphContextMenuExtender(const TSharedRef<FUICommandList> InCommandList, const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, bool bIsDebugging);
	void OnSyncCommentToGroup(FMenuBuilder& MenuBuilder, const UEdGraph* InGraph, const UEdGraphNode* InNode);

	/** Handle to our registered graph menu extender (for cleanup). */
	FDelegateHandle GraphMenuExtenderHandle;

	TSharedPtr<class FUICommandList> PluginCommandList;
	bool bIsShuttingDown;

	/** Material-editor-opened delegate handles (for cleanup on shutdown). */
	FDelegateHandle MaterialEditorOpenedHandle;
	FDelegateHandle MaterialInstanceEditorOpenedHandle;
};
