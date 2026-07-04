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
}

void FMaterialLayoutProModule::ShutdownModule()
{
	bIsShuttingDown = true;

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
				.Label(LOCTEXT("TabTitle", "Material Layout Pro"));
			Tab->SetContent(SNew(SMaterialLayoutProPanel));
			return Tab;
		}))
		.SetDisplayName(LOCTEXT("TabName", "Material Layout Pro"))
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
			LOCTEXT("OpenPanelMenu", "Material Layout Pro"),
			LOCTEXT("OpenPanelMenuTip", "Open the material parameter manager panel."),
			FSlateIcon(FMaterialLayoutProStyle::GetStyleSetName(), "MaterialLayoutPro.OpenPanel"));
	}

	UToolMenu* ToolBarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (ToolBarMenu)
	{
		FToolMenuSection& ToolBarSection = ToolBarMenu->FindOrAddSection("MaterialLayoutPro");
		ToolBarSection.AddEntry(FToolMenuEntry::InitToolBarButton(
			FMaterialLayoutProCommands::Get().OpenPanel,
			LOCTEXT("ToolbarOpenButton", "MLP"),
			LOCTEXT("ToolbarOpenButtonTip", "Open the Material Layout Pro panel"),
			FSlateIcon(FMaterialLayoutProStyle::GetStyleSetName(), "MaterialLayoutPro.OpenPanel")));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMaterialLayoutProModule, MaterialLayoutPro)
