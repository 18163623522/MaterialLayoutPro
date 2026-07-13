#include "MaterialLayoutPro.h"
#include "Style/MaterialLayoutProStyle.h"
#include "Commands/MaterialLayoutProCommands.h"
#include "Widgets/SMaterialLayoutProPanel.h"
#include "Widgets/SMaterialInstanceGroupPanel.h"

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
const FName FMaterialLayoutProModule::InstanceSidebarTabId(TEXT("MaterialInstanceLayoutProSidebar"));

void FMaterialLayoutProModule::StartupModule()
{
	bIsShuttingDown = false;

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

	// Hook into ANY asset-editor open event. Filter for materials inside.
	FAssetEditorManager::Get().OnAssetOpenedInEditor().AddRaw(this, &FMaterialLayoutProModule::OnAssetOpenedInEditor);

	// Add a toolbar extender at the MODULE level - every material/material-instance editor gets the button.
	RegisterMaterialEditorToolbarExtender();

	// Graph context-menu extender for "Sync Comment to Group".
	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
	GraphEditorModule.GetAllGraphEditorContextMenuExtender().Add(
		FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateRaw(this, &FMaterialLayoutProModule::GetGraphContextMenuExtender));

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
									// Toggle purely on liveness: live → close, not live → open.
									// (Previously required IsForeground() too, which mis-fired when the
									// tab was live but obscured — clicking would re-invoke instead of close.)
									TSharedPtr<SDockTab> Tab = TM->FindExistingLiveTab(FMaterialLayoutProModule::EmbeddedTabId);
									if (Tab.IsValid())
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

			// Instance group panel button - toggles the dockable "实例分组" sidebar tab in the
			// focused material instance editor (same toggle pattern as "窗口布局Pro" above).
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
							if (!Asset->IsA<UMaterialInstance>()) continue;
							IAssetEditorInstance* Instance = AssetEditorSS->FindEditorForAsset(Asset, false);
							if (!Instance) continue;

							// Match by focused window.
							FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
							TSharedPtr<IToolkitHost> Host = Toolkit->GetToolkitHost();
							if (!Host.IsValid()) continue;
							TSharedRef<SWidget> HostWidget = Host->GetParentWidget();
							TSharedPtr<SWindow> EditorWindow = FSlateApplication::Get().FindWidgetWindow(HostWidget);
							if (ActiveWindow != EditorWindow) continue;

							IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(Instance);
							FMaterialLayoutProModule& Module = FModuleManager::GetModuleChecked<FMaterialLayoutProModule>("MaterialLayoutPro");
							Module.RegisterInstanceSidebar(MatEditor);

							if (TSharedPtr<FTabManager> TM = Toolkit->GetTabManager())
							{
								// Toggle purely on liveness: live → close, not live → open.
								// RegisterInstanceSidebar above is called with bAutoInvoke=false, so it
								// never opens the tab here — this toggle is the sole owner of open/close.
								TSharedPtr<SDockTab> Tab = TM->FindExistingLiveTab(FMaterialLayoutProModule::InstanceSidebarTabId);
								if (Tab.IsValid())
								{
									Tab->RequestCloseTab();
								}
								else
								{
									TM->TryInvokeTab(FMaterialLayoutProModule::InstanceSidebarTabId);
								}
							}
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
}

// ============================================================================
// Material Instance group window
// ============================================================================

void FMaterialLayoutProModule::RegisterInstanceSidebar(IMaterialEditor* InMaterialEditor, bool bAutoInvoke)
{
	if (!InMaterialEditor) return;

	FAssetEditorToolkit* AsToolkit = static_cast<FAssetEditorToolkit*>(InMaterialEditor);
	TWeakPtr<IMaterialEditor> WeakEditor = StaticCastSharedRef<IMaterialEditor>(AsToolkit->AsShared());

	TSharedPtr<FTabManager> TabManager = AsToolkit->GetTabManager();
	if (!TabManager.IsValid()) return;

	// Idempotent: unregister first, then register fresh (mirrors RegisterEmbeddedSidebar).
	TabManager->UnregisterTabSpawner(InstanceSidebarTabId);
	TabManager->RegisterTabSpawner(InstanceSidebarTabId, FOnSpawnTab::CreateRaw(this, &FMaterialLayoutProModule::OnSpawnInstanceSidebarTab, WeakEditor))
		.SetDisplayName(LOCTEXT("InstanceSidebarTabLabel", "实例分组"))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	// Auto-open ONLY from the editor-open path (bAutoInvoke=true). The toolbar toggle handler
	// calls this with bAutoInvoke=false; if it auto-invoked here, the just-opened tab would
	// immediately be closed by the toggle's own close branch (open→foreground→close deadlock).
	if (bAutoInvoke)
	{
		TSharedPtr<SDockTab> ExistingTab = TabManager->FindExistingLiveTab(InstanceSidebarTabId);
		if (!ExistingTab.IsValid())
		{
			TabManager->TryInvokeTab(InstanceSidebarTabId);
		}
	}
}

TSharedRef<SDockTab> FMaterialLayoutProModule::OnSpawnInstanceSidebarTab(const FSpawnTabArgs& Args, TWeakPtr<IMaterialEditor> InMaterialEditor)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		.Label(LOCTEXT("InstanceSidebarTabTitle", "实例分组"));

	Tab->SetContent
	(
		SNew(SMaterialInstanceGroupPanel)
		.OwningInstanceEditor(InMaterialEditor)
	);

	return Tab;
}

// ============================================================================
// Material Editor embedding
// ============================================================================

void FMaterialLayoutProModule::OnAssetOpenedInEditor(UObject* Asset, IAssetEditorInstance* Instance)
{
	if (!Asset || !Instance) return;

	IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(Instance);

	// Route by asset type so each editor only gets its relevant sidebar:
	//  - UMaterial (parent material)        -> "参数布局" sidebar (expression editing)
	//  - UMaterialInstance                  -> "实例分组" sidebar (override editing + custom grouping)
	if (Asset->IsA<UMaterial>())
	{
		RegisterEmbeddedSidebar(MatEditor);
	}
	else if (Asset->IsA<UMaterialInstance>())
	{
		// bAutoInvoke=true: pre-create the tab when the instance editor opens, so the user sees
		// the panel immediately without clicking the toolbar button.
		RegisterInstanceSidebar(MatEditor, true);
	}
}

void FMaterialLayoutProModule::RegisterEmbeddedSidebar(IMaterialEditor* InMaterialEditor)
{
	if (!InMaterialEditor) return;

	FAssetEditorToolkit* AsToolkit = static_cast<FAssetEditorToolkit*>(InMaterialEditor);
	TWeakPtr<IMaterialEditor> WeakEditor = StaticCastSharedRef<IMaterialEditor>(AsToolkit->AsShared());

	TSharedPtr<FTabManager> TabManager = AsToolkit->GetTabManager();
	if (!TabManager.IsValid()) return;

	// Idempotent: only register if not already registered for this TabManager.
	// UnregisterTabSpawner is safe to call even if not registered.
	TabManager->UnregisterTabSpawner(EmbeddedTabId);
	TabManager->RegisterTabSpawner(EmbeddedTabId, FOnSpawnTab::CreateRaw(this, &FMaterialLayoutProModule::OnSpawnEmbeddedTab, WeakEditor))
		.SetDisplayName(LOCTEXT("SidebarTabLabel", "参数布局"))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	// Auto-open on first registration only.
	TSharedPtr<SDockTab> ExistingTab = TabManager->FindExistingLiveTab(EmbeddedTabId);
	if (!ExistingTab.IsValid())
	{
		TabManager->TryInvokeTab(EmbeddedTabId);
	}
}

TSharedRef<SDockTab> FMaterialLayoutProModule::OnSpawnEmbeddedTab(const FSpawnTabArgs& Args, TWeakPtr<IMaterialEditor> InMaterialEditor)
{
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
			Material->SetFlags(RF_Transactional);
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
					ParamExpr->SetFlags(RF_Transactional);
					ParamExpr->Modify();
					ParamExpr->Group = GroupNameFName;
				}
			}

			Material->PostEditChange();
			Material->MarkPackageDirty();

			// This runs in the material editor's right-click menu, so the editor owning this
			// material is currently open. Notify it so the Group edit syncs from the preview copy
			// onto OriginalMaterial — otherwise it's lost on save/reopen (see notes in
			// SMaterialLayoutProPanel::NotifyMaterialEditorChanged).
			//
			// NOTE: `Material` here is MaterialGraph->Material, which is the editor's transient
			// PREVIEW copy — it is NOT registered with AssetEditorSubsystem (only OriginalMaterial
			// is). So FindEditorForAsset(Material) returns null. We instead match the editor by
			// pointer-comparing its GetMaterialInterface() (which returns the preview copy too).
			if (GEditor)
			{
				if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
				{
					for (UObject* EditedAsset : AssetEditorSS->GetAllEditedAssets())
					{
						if (!EditedAsset || !EditedAsset->IsA<UMaterialInterface>()) continue;
						if (IAssetEditorInstance* EditorInst = AssetEditorSS->FindEditorForAsset(EditedAsset, false))
						{
							IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(EditorInst);
							if (MatEditor->GetMaterialInterface() == Material)
							{
								MatEditor->UpdateMaterialAfterGraphChange();
								break;
							}
						}
					}
				}
			}
		}));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMaterialLayoutProModule, MaterialLayoutPro)
