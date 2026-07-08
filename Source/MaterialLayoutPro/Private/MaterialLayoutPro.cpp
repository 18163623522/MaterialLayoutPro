#include "MaterialLayoutPro.h"
#include "Style/MaterialLayoutProStyle.h"
#include "Commands/MaterialLayoutProCommands.h"
#include "Widgets/SMaterialLayoutProPanel.h"

#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Commands/UICommandList.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "LevelEditor.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FMaterialLayoutProModule"

static const FName MaterialLayoutProTabName(TEXT("MaterialLayoutPro"));

void FMaterialLayoutProModule::StartupModule()
{
	bIsShuttingDown = false;

	RegisterStyle();
	RegisterCommands();

	PluginCommandList = MakeShareable(new FUICommandList);
	BindCommands();

	RegisterTabSpawners();

	// Register menus immediately if ToolMenus is already available, otherwise
	// wait for the startup callback. Some loading configurations fire the startup
	// event before PostEngineInit editor modules are initialized.
	if (UToolMenus* ToolMenus = UToolMenus::TryGet())
	{
		RegisterMenus();
	}
	else
	{
		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMaterialLayoutProModule::RegisterMenus));
	}

	// Register a graph context-menu extender to add "Sync Comment to Group" on material comments.
	FGraphEditorModule& GraphEditorModule = FModuleManager::LoadModuleChecked<FGraphEditorModule>("GraphEditor");
	GraphEditorModule.GetAllGraphEditorContextMenuExtender().Add(
		FGraphEditorModule::FGraphEditorMenuExtender_SelectedNode::CreateRaw(this, &FMaterialLayoutProModule::GetGraphContextMenuExtender));
}

void FMaterialLayoutProModule::ShutdownModule()
{
	bIsShuttingDown = true;

	// Note: the graph menu extender is not unregistered here because 4.26's delegate
	// array doesn't expose a stable handle-based removal. Plugin modules unload at the
	// same time the editor shuts down, so the extender array is torn down with it.
	UToolMenus::UnRegisterStartupCallback(this);
	if (UToolMenus* TM = UToolMenus::TryGet())
	{
		TM->UnregisterOwner(this);
	}

	UnregisterTabSpawners();
	UnregisterCommands();
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

	UToolMenu* ToolBarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (ToolBarMenu)
	{
		FToolMenuSection& ToolBarSection = ToolBarMenu->FindOrAddSection("MaterialLayoutPro");
		ToolBarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
			FMaterialLayoutProCommands::Get().OpenPanel,
			LOCTEXT("ToolbarOpenButton", "MLP"),
			LOCTEXT("ToolbarOpenButtonTip", "打开材质布局 Pro 面板"),
			FSlateIcon(FMaterialLayoutProStyle::GetStyleSetName(), "MaterialLayoutPro.OpenPanel")));
	}
}

TSharedRef<FExtender> FMaterialLayoutProModule::GetGraphContextMenuExtender(const TSharedRef<FUICommandList> InCommandList, const UEdGraph* InGraph, const UEdGraphNode* InNode, const UEdGraphPin* InPin, bool bIsDebugging)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	// Only add the action when the right-clicked node is a material comment box.
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
			if (!CommentNode || !InGraph)
			{
				return;
			}

			const UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(InGraph);
			if (!MaterialGraph || !MaterialGraph->Material)
			{
				return;
			}
			UMaterial* Material = MaterialGraph->Material;

			FString GroupName = CommentNode->NodeComment;
			GroupName.TrimStartAndEndInline();
			if (GroupName.IsEmpty())
			{
				return;
			}
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
				if (!Expression)
				{
					continue;
				}
				UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expression);
				if (!ParamExpr)
				{
					continue;
				}
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
