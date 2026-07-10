#include "MaterialLayoutPro.h"
#include "Style/MaterialLayoutProStyle.h"
#include "Commands/MaterialLayoutProCommands.h"
#include "Widgets/SMaterialLayoutProPanel.h"

#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Application/SlateApplication.h"
#include "Toolkits/IToolkitHost.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "LevelEditor.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/UIAction.h"
#include "EdGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "ScopedTransaction.h"
#include "IMaterialEditor.h"
#include "MaterialEditorModule.h"
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "FMaterialLayoutProModule"

static const FName MaterialLayoutProTabName(TEXT("MaterialLayoutPro"));
const FName FMaterialLayoutProModule::EmbeddedTabId(TEXT("MaterialLayoutProSidebar"));

void FMaterialLayoutProModule::StartupModule()
{
	bIsShuttingDown = false;

	UE_LOG(LogTemp, Warning, TEXT("[MLP] ========== Module StartupModule START =========="));

	RegisterStyle();
	RegisterCommands();

	PluginCommandList = MakeShareable(new FUICommandList);
	BindCommands();

	RegisterTabSpawners();

	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		RegisterMenus();
	}
	else
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMaterialLayoutProModule::RegisterMenus));
	}

	// Hook into ANY asset-editor open event (more reliable than IMaterialEditorModule events,
	// which don't always fire for all editor-open code paths in 4.26). Filter for materials inside.
	FAssetEditorManager::Get().OnAssetOpenedInEditor().AddRaw(this, &FMaterialLayoutProModule::OnAssetOpenedInEditor);
	UE_LOG(LogTemp, Warning, TEXT("[MLP] StartupModule: bound OnAssetOpenedInEditor"));

	// Add a toolbar extender at the MODULE level (IMaterialEditorModule::GetToolBarExtensibilityManager).
	// FMaterialEditor::ExtendToolbar() collects from this manager when building its toolbar, so every
	// material/material-instance editor will get the button automatically — no timing issues.
	RegisterMaterialEditorToolbarExtender();
	UE_LOG(LogTemp, Warning, TEXT("[MLP] StartupModule: registered module-level toolbar extender"));

	// Graph context-menu extender for "Sync Comment to Group".
	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
	GraphEditorModule.GetAllGraphEditorContextMenuExtender().Add(
		FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateRaw(this, &FMaterialLayoutProModule::GetGraphContextMenuExtender));

	UE_LOG(LogTemp, Warning, TEXT("[MLP] ========== Module StartupModule END =========="));
}

void FMaterialLayoutProModule::ShutdownModule()
{
	bIsShuttingDown = true;

	if (GEditor)
	{
		FAssetEditorManager::Get().OnAssetOpenedInEditor().RemoveAll(this);
	}

	// Remove the module-level toolbar extender so material editors don't dangle a reference.
	if (MaterialEditorToolbarExtender.IsValid() && FModuleManager::Get().IsModuleLoaded("MaterialEditor"))
	{
		IMaterialEditorModule::Get().GetToolBarExtensibilityManager()->RemoveExtender(MaterialEditorToolbarExtender);
		MaterialEditorToolbarExtender.Reset();
	}

	UToolMenus::UnRegisterStartupCallback(this);
	if (UToolMenus* TM = UToolMenus::TryGet())
	{
		TM->UnregisterOwner(this);
	}

	UnregisterTabSpawners();
	FMaterialLayoutProCommands::Unregister();
	UnregisterStyle();
}

void FMaterialLayoutProModule::RegisterStyle()
{
	FMaterialLayoutProStyle::Initialize();
}

void FMaterialLayoutProModule::UnregisterStyle()
{
	FMaterialLayoutProStyle::Shutdown();
}

void FMaterialLayoutProModule::RegisterCommands()
{
	FMaterialLayoutProCommands::Register();
}

void FMaterialLayoutProModule::UnregisterCommands()
{
	FMaterialLayoutProCommands::Unregister();
}

void FMaterialLayoutProModule::BindCommands()
{
	if (!PluginCommandList.IsValid())
	{
		return;
	}

	const auto& Commands = FMaterialLayoutProCommands::Get();

	PluginCommandList->MapAction(
		Commands.OpenPanel,
		FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(MaterialLayoutProTabName);
		}));
}

void FMaterialLayoutProModule::RegisterTabSpawners()
{
	// Standalone Nomad tab (fallback entry; not the primary UX in embedded mode).
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		MaterialLayoutProTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&) -> TSharedRef<SDockTab>
		{
			TSharedRef<SDockTab> Tab = SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("TabTitle", "材质布局 Pro"));
			Tab->SetContent(SNew(SMaterialLayoutProPanel));
			return Tab;
		}))
		.SetDisplayName(LOCTEXT("TabName", "材质布局 Pro"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FMaterialLayoutProModule::UnregisterTabSpawners()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MaterialLayoutProTabName);
}

void FMaterialLayoutProModule::RegisterMenus()
{
	if (bIsShuttingDown)
	{
		return;
	}

	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	if (WindowMenu)
	{
		FToolMenuSection& WindowSection = WindowMenu->FindOrAddSection("WindowLayout");
		WindowSection.AddMenuEntryWithCommandList(
			FMaterialLayoutProCommands::Get().OpenPanel,
			PluginCommandList,
			LOCTEXT("OpenPanelMenu", "材质布局 Pro"),
			LOCTEXT("OpenPanelMenuTip", "打开材质参数管理器面板。"),
			FSlateIcon(FMaterialLayoutProStyle::GetStyleSetName(), "MaterialLayoutPro.OpenPanel"));
	}

	// NOTE: toolbar buttons for the material/instance editors are added per-editor in
	// RegisterEmbeddedSidebar (via GetToolBarExtensibilityManager), not here — the material
	// editor's toolbar menu doesn't exist at module-startup time.
}

void FMaterialLayoutProModule::RegisterMaterialEditorToolbarExtender()
{
	// Build one shared extender. FMaterialEditor::ExtendToolbar() pulls all extenders from
	// IMaterialEditorModule::GetToolBarExtensibilityManager() when it builds its toolbar, so
	// registering here covers every material / material-instance editor with no timing issues.
	MaterialEditorToolbarExtender = MakeShareable(new FExtender());
	MaterialEditorToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		PluginCommandList,
		FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& Builder)
		{
			Builder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([]()
					{
						// Toggle the panel tab in the currently focused material editor.
						// If we can't determine focus, toggle in all editors.
						UAssetEditorSubsystem* AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
						if (!AssetEditorSS) return;

						TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
						bool bHandled = false;

						for (UObject* Asset : AssetEditorSS->GetAllEditedAssets())
						{
							if (!Asset || !Asset->IsA<UMaterialInterface>()) continue;
							IAssetEditorInstance* Instance = AssetEditorSS->FindEditorForAsset(Asset, false);
							if (!Instance) continue;

							FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
							TSharedPtr<IToolkitHost> Host = Toolkit->GetToolkitHost();
							if (!Host.IsValid()) continue;
							TSharedRef<SWidget> HostWidget = Host->GetParentWidget();
							TSharedPtr<SWindow> EditorWindow = FSlateApplication::Get().FindWidgetWindow(HostWidget);

							// If this is the focused editor, toggle its tab.
							if (ActiveWindow == EditorWindow)
							{
								IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(Instance);
								FMaterialLayoutProModule& Module = FModuleManager::GetModuleChecked<FMaterialLayoutProModule>("MaterialLayoutPro");
								Module.RegisterEmbeddedSidebar(MatEditor);

								if (TSharedPtr<FTabManager> TM = Toolkit->GetTabManager())
								{
									TSharedPtr<SDockTab> Tab = TM->FindExistingLiveTab(FMaterialLayoutProModule::EmbeddedTabId);
									if (Tab.IsValid() && Tab->IsForeground())
									{
										Tab->RequestCloseTab();
									}
									else
									{
										TM->TryInvokeTab(FMaterialLayoutProModule::EmbeddedTabId);
									}
								}
								bHandled = true;
								break;
							}
						}

						// Fallback: no focused editor found - try all material editors.
						if (!bHandled)
						{
							for (UObject* Asset : AssetEditorSS->GetAllEditedAssets())
							{
								if (!Asset || !Asset->IsA<UMaterialInterface>()) continue;
								IAssetEditorInstance* Instance = AssetEditorSS->FindEditorForAsset(Asset, false);
								if (!Instance) continue;
								IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(Instance);
								FMaterialLayoutProModule& Module = FModuleManager::GetModuleChecked<FMaterialLayoutProModule>("MaterialLayoutPro");
								Module.RegisterEmbeddedSidebar(MatEditor);
								FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
								if (TSharedPtr<FTabManager> TM = Toolkit->GetTabManager())
								{
									TM->TryInvokeTab(FMaterialLayoutProModule::EmbeddedTabId);
								}
							}
						}
					}),
					FCanExecuteAction::CreateLambda([]() -> bool
					{
						// Enabled when any material editor is open.
						UAssetEditorSubsystem* AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
						if (!AssetEditorSS) return false;
						for (UObject* Asset : AssetEditorSS->GetAllEditedAssets())
						{
							if (Asset && Asset->IsA<UMaterialInterface>()) return true;
						}
						return false;
					}),
					FIsActionChecked::CreateLambda([]() -> bool
					{
						UAssetEditorSubsystem* AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
						if (!AssetEditorSS) return false;
						for (UObject* Asset : AssetEditorSS->GetAllEditedAssets())
						{
							if (!Asset || !Asset->IsA<UMaterialInterface>()) continue;
							IAssetEditorInstance* Instance = AssetEditorSS->FindEditorForAsset(Asset, false);
							if (!Instance) continue;
							FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
							if (TSharedPtr<FTabManager> TM = Toolkit->GetTabManager())
							{
								TSharedPtr<SDockTab> Tab = TM->FindExistingLiveTab(FMaterialLayoutProModule::EmbeddedTabId);
								if (Tab.IsValid() && Tab->IsForeground()) return true;
							}
						}
						return false;
					})
				),
				NAME_None,
				FText::FromString(TEXT("窗口布局Pro")),
				FText::FromString(TEXT("参数布局")),
#if ENGINE_MAJOR_VERSION >= 5
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "DetailsPanel")
#else
				FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.ToggleDetails")
#endif
			);

			// Instance group panel button - opens a standalone window for material instance parameter grouping.
			Builder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([]()
					{
						UAssetEditorSubsystem* AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
						if (!AssetEditorSS) return;
						TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
						for (UObject* Asset : AssetEditorSS->GetAllEditedAssets())
						{
							if (!Asset) continue;
							UMaterialInstance* MI = Cast<UMaterialInstance>(Asset);
							if (!MI) continue;
							IAssetEditorInstance* Instance = AssetEditorSS->FindEditorForAsset(Asset, false);
							if (!Instance) continue;

							// Match by focused window.
							FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
							TSharedPtr<IToolkitHost> Host = Toolkit->GetToolkitHost();
							if (!Host.IsValid()) continue;
							TSharedRef<SWidget> HostWidget = Host->GetParentWidget();
							TSharedPtr<SWindow> EditorWindow = FSlateApplication::Get().FindWidgetWindow(HostWidget);
							if (ActiveWindow != EditorWindow) continue;

							// Create the panel widget and open it in a window.
							TSharedRef<SMaterialLayoutProPanel> Panel = SNew(SMaterialLayoutProPanel)
								.OwningMaterialEditor(StaticCastSharedRef<IMaterialEditor>(Toolkit->AsShared()));
							Panel->OnInstanceGroupClicked();
							return;
						}
					}),
					FCanExecuteAction::CreateLambda([]() -> bool
					{
						UAssetEditorSubsystem* AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
						if (!AssetEditorSS) return false;
						TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
						for (UObject* Asset : AssetEditorSS->GetAllEditedAssets())
						{
							if (!Asset || !Asset->IsA<UMaterialInstance>()) continue;
							IAssetEditorInstance* Instance = AssetEditorSS->FindEditorForAsset(Asset, false);
							if (!Instance) continue;
							FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
							TSharedPtr<IToolkitHost> Host = Toolkit->GetToolkitHost();
							if (!Host.IsValid()) continue;
							TSharedRef<SWidget> HostWidget = Host->GetParentWidget();
							TSharedPtr<SWindow> EditorWindow = FSlateApplication::Get().FindWidgetWindow(HostWidget);
							if (ActiveWindow == EditorWindow) return true;
						}
						return false;
					})
				),
				NAME_None,
				FText::FromString(TEXT("实例分组")),
				FText::FromString(TEXT("材质实例参数分组")),
#if ENGINE_MAJOR_VERSION >= 5
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "MaterialInstanceEditor.ToggleProperties")
#else
				FSlateIcon(FEditorStyle::GetStyleSetName(), "MaterialEditor.ToggleProperties")
#endif
			);
		}));

	IMaterialEditorModule& MaterialEditorModule = IMaterialEditorModule::Get();
	MaterialEditorModule.GetToolBarExtensibilityManager()->AddExtender(MaterialEditorToolbarExtender);
	UE_LOG(LogTemp, Warning, TEXT("[MLP] RegisterMaterialEditorToolbarExtender: added to module toolbar manager"));
}

// ============================================================================
// Material Editor embedding
// ============================================================================

void FMaterialLayoutProModule::OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* Instance)
{
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnAssetOpenedInEditor: asset=%s"), Asset ? *Asset->GetName() : TEXT("null"));

	// Only act on materials / material instances.
	if (!Asset || !Instance) return;
	if (!Asset->IsA<UMaterialInterface>()) return;

	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnAssetOpenedInEditor: material detected, casting"));

	// The instance IS the material editor (IMaterialEditor derives from FAssetEditorToolkit
	// which implements IAssetEditorInstance). static_cast down.
	IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(Instance);

	RegisterEmbeddedSidebar(MatEditor);
}

void FMaterialLayoutProModule::RegisterEmbeddedSidebar(IMaterialEditor* InMaterialEditor)
{
	if (!InMaterialEditor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MLP] RegisterEmbeddedSidebar: null editor"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MLP] RegisterEmbeddedSidebar: editor valid, getting TabManager"));

	// Recover a weak ptr to the editor via SharedFromThis (FAAssetEditorToolkit derives from it).
	FAssetEditorToolkit* AsToolkit = static_cast<FAssetEditorToolkit*>(InMaterialEditor);
	TWeakPtr<IMaterialEditor> WeakEditor = StaticCastSharedRef<IMaterialEditor>(AsToolkit->AsShared());

	TSharedPtr<FTabManager> TabManager = AsToolkit->GetTabManager();
	if (!TabManager.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MLP] RegisterEmbeddedSidebar: TabManager invalid"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[MLP] RegisterEmbeddedSidebar: TabManager valid, registering spawner"));

	// Register a sidebar tab spawner on this editor's tab manager.
	const FText TabLabel = LOCTEXT("SidebarTabLabel", "参数布局");
	TabManager->RegisterTabSpawner(EmbeddedTabId, FOnSpawnTab::CreateRaw(this, &FMaterialLayoutProModule::OnSpawnEmbeddedTab, WeakEditor))
		.SetDisplayName(TabLabel)
		.SetMenuType(ETabSpawnerMenuType::Enabled); // Enabled so it appears in the editor's Window menu as a fallback.

	// NOTE: toolbar button is added at the module level (RegisterMaterialEditorToolbarExtender),
	// which FMaterialEditor::ExtendToolbar collects automatically. No per-editor extender needed.

	// Invoke the tab so it opens automatically beside the graph canvas.
	TSharedPtr<SDockTab> InvokedTab = TabManager->TryInvokeTab(EmbeddedTabId);
	UE_LOG(LogTemp, Warning, TEXT("[MLP] RegisterEmbeddedSidebar: TryInvokeTab result = %s"), InvokedTab.IsValid() ? TEXT("OK") : TEXT("NULL"));
}

TSharedRef<SDockTab> FMaterialLayoutProModule::OnSpawnEmbeddedTab(const FSpawnTabArgs& Args, TWeakPtr<IMaterialEditor> InMaterialEditor)
{
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnSpawnEmbeddedTab: spawning sidebar tab"));

	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(LOCTEXT("SidebarTabTitle", "参数布局"));

	Tab->SetContent
	(
		SNew(SMaterialLayoutProPanel)
		.OwningMaterialEditor(InMaterialEditor)
	);

	return Tab;
}

// ============================================================================
// Graph context menu
// ============================================================================

TSharedRef<FExtender> FMaterialLayoutProModule::GetGraphContextMenuExtender(const TSharedRef<FUICommandList> InCommandList, const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, bool bIsDebugging)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	if (InNode && InNode->IsA<UEdGraphNode_Comment>())
	{
		Extender->AddMenuExtension(
			"GraphNodeContext",
			EExtensionHook::After,
			InCommandList,
			FMenuExtensionDelegate::CreateRaw(this, &FMaterialLayoutProModule::OnSyncCommentToGroup, InGraph, InNode));
	}

	return Extender;
}

void FMaterialLayoutProModule::OnSyncCommentToGroup(FMenuBuilder& MenuBuilder, const UEdGraph* InGraph, const UEdGraphNode* InNode)
{
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("同步注释到参数分组")),
		FText::FromString(TEXT("将此注释内的每个参数分配到以该注释命名的材质分组")),
		FSlateIcon(),
		FExecuteAction::CreateLambda([InGraph, InNode]()
		{
			const UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(InNode);
			if (!CommentNode || !InGraph) return;

			const UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(InGraph);
			if (!MaterialGraph || !MaterialGraph->Material) return;
			UMaterial* Material = MaterialGraph->Material;

			FString GroupName = CommentNode->NodeComment;
			GroupName.TrimStartAndEndInline();
			if (GroupName.IsEmpty()) return;
			GroupName.ReplaceInline(TEXT(" "), TEXT("_"));
			GroupName.ReplaceInline(TEXT("-"), TEXT("_"));
			const FName GroupNameFName(*GroupName);

			const FVector2D CommentMin(CommentNode->NodePosX, CommentNode->NodePosY);
			const FVector2D CommentMax(CommentNode->NodePosX + CommentNode->NodeWidth, CommentNode->NodePosY + CommentNode->NodeHeight);

			const FScopedTransaction Transaction(FText::FromString(TEXT("同步注释到参数分组")));
			Material->Modify();

#if ENGINE_MAJOR_VERSION >= 5
			for (UMaterialExpression* Expression : Material->GetExpressions())
#else
			for (UMaterialExpression* Expression : Material->Expressions)
#endif
			{
				if (!Expression) continue;
				UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression);
				if (!ParamExpr) continue;
				const FVector2D Pos(Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY);
				if (Pos.X >= CommentMin.X && Pos.X <= CommentMax.X && Pos.Y >= CommentMin.Y && Pos.Y <= CommentMax.Y)
				{
					ParamExpr->Modify();
					ParamExpr->Group = GroupNameFName;
				}
			}

			Material->PostEditChange();
			Material->MarkPackageDirty();
		}));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMaterialLayoutProModule, MaterialLayoutPro)
