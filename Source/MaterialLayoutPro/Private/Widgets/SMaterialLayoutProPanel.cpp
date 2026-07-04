#include "Widgets/SMaterialLayoutProPanel.h"
#include "MaterialLayoutProTheme.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Model/MaterialParameterScanner.h"
#include "Model/MaterialParameterUsageAnalyzer.h"
#include "Widgets/SMaterialParameterRow.h"
#include "Widgets/SMaterialBulkRenameDialog.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "ScopedTransaction.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Styling/CoreStyle.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SMaterialLayoutProPanel"

SMaterialLayoutProPanel::~SMaterialLayoutProPanel()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
}

void SMaterialLayoutProPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::Background())
		.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadMD())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				BuildToolbar()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				BuildStatusBar()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FMLPTheme::Surface())
				.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
				[
					SAssignNew(GroupContainer, SVerticalBox)
				]
			]
		]
	];

	USelection::SelectionChangedEvent.AddSP(SharedThis(this), &SMaterialLayoutProPanel::OnSelectionChanged);
	OnSelectionChanged(nullptr);
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildToolbar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TargetLabel", "Target: "))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(FMLPTheme::PadSM())
		[
			SNew(STextBlock)
			.Text(this, &SMaterialLayoutProPanel::GetTargetMaterialName)
			.Font(FMLPTheme::FontHeading())
			.ColorAndOpacity(FMLPTheme::Foreground())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			SNew(SButton)
			.Text(LOCTEXT("Refresh", "Refresh"))
			.ToolTipText(LOCTEXT("RefreshTooltip", "Rescan parameters"))
			.OnClicked(this, &SMaterialLayoutProPanel::OnRefreshClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			SNew(SButton)
			.Text(LOCTEXT("Select", "Select"))
			.ToolTipText(LOCTEXT("SelectTooltip", "Locate material in Content Browser"))
			.OnClicked(this, &SMaterialLayoutProPanel::OnSelectMaterialClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			FMLPTheme::MakeSeparator()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GroupLabel", "Group: "))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		.VAlign(VAlign_Center)
		[
			SAssignNew(SetGroupTextBox, SEditableTextBox)
			.MinDesiredWidth(120.f)
			.Font(FMLPTheme::FontBody())
			.HintText(LOCTEXT("GroupHint", "selected"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			SNew(SButton)
			.Text(LOCTEXT("SetGroup", "Set"))
			.ToolTipText(LOCTEXT("SetGroupTooltip", "Set Group for selected parameters"))
			.OnClicked(this, &SMaterialLayoutProPanel::OnSetGroupClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			SNew(SButton)
			.Text(LOCTEXT("AutoGroup", "Auto Group"))
			.ToolTipText(LOCTEXT("AutoGroupTooltip", "Auto-group parameters by name prefix"))
			.OnClicked(this, &SMaterialLayoutProPanel::OnAutoGroupClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			FMLPTheme::MakeSeparator()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMLPTheme::PadH())
		[
			SNew(SButton)
			.Text(LOCTEXT("Archive", "Archive Unused"))
			.ToolTipText(LOCTEXT("ArchiveTooltip", "Move unused parameters to Deprecated group"))
			.OnClicked(this, &SMaterialLayoutProPanel::OnArchiveUnusedClicked)
		]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMLPTheme::PadH())
			[
				SNew(SButton)
				.Text(LOCTEXT("Delete", "Delete Unused"))
				.ToolTipText(LOCTEXT("DeleteTooltip", "Delete unused parameters"))
				.OnClicked(this, &SMaterialLayoutProPanel::OnDeleteUnusedClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMLPTheme::PadH())
			[
				SNew(SButton)
				.Text(LOCTEXT("BulkRename", "Bulk Rename"))
				.ToolTipText(LOCTEXT("BulkRenameTooltip", "Bulk rename selected parameters"))
				.OnClicked(this, &SMaterialLayoutProPanel::OnBulkRenameClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMLPTheme::PadH())
			[
				FMLPTheme::MakeSeparator()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMLPTheme::PadH())
			[
				SNew(SButton)
				.Text(LOCTEXT("Export", "Export"))
				.ToolTipText(LOCTEXT("ExportTooltip", "Export parameter list to CSV"))
				.OnClicked(this, &SMaterialLayoutProPanel::OnExportClicked)
			];
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildStatusBar()
{
	return SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::SurfaceAlt())
		.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadSM())
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SMaterialLayoutProPanel::GetStatusText)
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
		];
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildEmptyState()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyState", "Select a material in the Content Browser to view its parameters"))
				.Font(FMLPTheme::FontTitle())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
		];
}

FReply SMaterialLayoutProPanel::OnRefreshClicked()
{
	RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnSelectMaterialClicked()
{
	UObject* TargetObject = TargetMaterial.IsValid() ? (UObject*)TargetMaterial.Get() : (TargetMaterialInstance.IsValid() ? (UObject*)TargetMaterialInstance.Get() : nullptr);
	if (TargetObject)
	{
		TArray<FAssetData> Assets;
		Assets.Add(FAssetData(TargetObject));
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(Assets, true);
	}
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnArchiveUnusedClicked()
{
	ArchiveUnused();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnDeleteUnusedClicked()
{
	DeleteUnused();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnSetGroupClicked()
{
	if (SetGroupTextBox.IsValid())
	{
		FName NewGroup(*SetGroupTextBox->GetText().ToString());
		if (!NewGroup.IsNone())
		{
			ApplyGroupToSelected(NewGroup);
		}
	}
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnAutoGroupClicked()
{
	AutoGroup();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnBulkRenameClicked()
{
	if (!TargetMaterial.IsValid())
	{
		return FReply::Handled();
	}

	TArray<TSharedPtr<FMLPParameterInfo>> SelectedParameters;
	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param.IsValid() && Param->bSelected)
		{
			SelectedParameters.Add(Param);
		}
	}

	TArray<TSharedPtr<FMLPParameterInfo>> ParamsToRename = SelectedParameters.Num() > 0 ? SelectedParameters : Parameters;

	TSharedRef<SWindow> BulkRenameDialog = SNew(SMaterialBulkRenameDialog)
		.TargetMaterial(TargetMaterial)
		.Parameters(ParamsToRename);

	FSlateApplication::Get().AddWindow(BulkRenameDialog);
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnGroupByCommentClicked()
{
	GroupByComment();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnExportClicked()
{
	if (Parameters.Num() == 0)
	{
		return FReply::Handled();
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	const FString DefaultPath = FPaths::ProjectSavedDir();
	const FString DefaultFileName = TargetMaterial.IsValid()
		? FString::Printf(TEXT("%s_Parameters.csv"), *TargetMaterial->GetName())
		: TEXT("MaterialParameters.csv");

	TArray<FString> OutFilenames;
	bool bSaved = DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("ExportDialogTitle", "Export Parameters").ToString(),
		DefaultPath,
		DefaultFileName,
		TEXT("CSV files|*.csv"),
		EFileDialogFlags::None,
		OutFilenames);

	if (bSaved && OutFilenames.Num() > 0)
	{
		ExportParameters(OutFilenames[0]);
	}

	return FReply::Handled();
}

void SMaterialLayoutProPanel::ExportParameters(const FString& FilePath)
{
	FString CSV = TEXT("Name,Type,Group,SortPriority,Usage,Value\n");
	for (const TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (!Param.IsValid()) continue;

		CSV += FString::Printf(TEXT("%s,%s,%s,%d,%s,%s\n"),
			*Param->Name.ToString(),
			*Param->GetDisplayTypeName().ToString(),
			*Param->Group.ToString(),
			Param->SortPriority,
			*Param->GetUsageLabel().ToString(),
			*Param->ValueString);
	}

	FFileHelper::SaveStringToFile(CSV, *FilePath);
}

void SMaterialLayoutProPanel::OnSelectionChanged(UObject* Selection)
{
	UMaterial* NewMaterial = nullptr;
	UMaterialInstance* NewMaterialInstance = nullptr;

	if (GEditor)
	{
		USelection* SelectedObjects = GEditor->GetSelectedObjects();
		if (SelectedObjects)
		{
			for (FSelectionIterator It(*SelectedObjects); It; ++It)
			{
				UObject* Object = *It;
				if (!NewMaterial && Object && Object->IsA<UMaterial>())
				{
					NewMaterial = Cast<UMaterial>(Object);
					break;
				}
				if (!NewMaterialInstance && Object && Object->IsA<UMaterialInstance>())
				{
					NewMaterialInstance = Cast<UMaterialInstance>(Object);
				}
			}
		}
	}

	TargetMaterial = NewMaterial;
	TargetMaterialInstance = NewMaterialInstance;
	RefreshParameters();
}

void SMaterialLayoutProPanel::RefreshParameters()
{
	Parameters.Reset();
	Groups.Reset();
	LastClickedItem.Reset();

	if (TargetMaterial.IsValid())
	{
		Parameters = FMaterialParameterScanner::ScanMaterial(TargetMaterial.Get());
	}
	else if (TargetMaterialInstance.IsValid())
	{
		Parameters = FMaterialParameterScanner::ScanMaterialInstance(TargetMaterialInstance.Get());
	}

	if (TargetMaterial.IsValid())
	{
		FMaterialParameterUsageAnalyzer::Analyze(TargetMaterial.Get(), Parameters);
	}

	RebuildGroups();
	RebuildGroupList();
}

void SMaterialLayoutProPanel::RebuildGroups()
{
	Groups.Reset();

	TMap<FName, TSharedPtr<FMLPParameterGroup>> GroupMap;
	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (!Param.IsValid()) continue;

		FName GroupName = Param->Group.IsNone() ? FName(TEXT("(None)")) : Param->Group;
		TSharedPtr<FMLPParameterGroup>* Group = GroupMap.Find(GroupName);
		if (!Group)
		{
			TSharedPtr<FMLPParameterGroup> NewGroup = MakeShared<FMLPParameterGroup>();
			NewGroup->Name = GroupName;
			NewGroup->SortPriority = Param->SortPriority;
			NewGroup->bExpanded = true;
			GroupMap.Add(GroupName, NewGroup);
			Groups.Add(NewGroup);
			Group = &GroupMap.FindChecked(GroupName);
		}
		(*Group)->Parameters.Add(Param);
		(*Group)->SortPriority = FMath::Min((*Group)->SortPriority, Param->SortPriority);
	}

	Groups.Sort([](const TSharedPtr<FMLPParameterGroup>& A, const TSharedPtr<FMLPParameterGroup>& B)
	{
		if (A->SortPriority != B->SortPriority)
		{
			return A->SortPriority < B->SortPriority;
		}
		return A->Name.ToString() < B->Name.ToString();
	});

	for (TSharedPtr<FMLPParameterGroup>& Group : Groups)
	{
		Group->Parameters.Sort([](const TSharedPtr<FMLPParameterInfo>& A, const TSharedPtr<FMLPParameterInfo>& B)
		{
			if (A->SortPriority != B->SortPriority)
			{
				return A->SortPriority < B->SortPriority;
			}
			return A->Name.ToString() < B->Name.ToString();
		});
	}
}

void SMaterialLayoutProPanel::RebuildGroupList()
{
	if (!GroupContainer.IsValid())
	{
		return;
	}

	GroupContainer->ClearChildren();

	if (Parameters.Num() == 0)
	{
		GroupContainer->AddSlot()
			.FillHeight(1.0f)
			[
				BuildEmptyState()
			];
		return;
	}

	for (TSharedPtr<FMLPParameterGroup>& Group : Groups)
	{
		GroupContainer->AddSlot()
			.AutoHeight()
			[
				BuildGroupHeader(Group)
			];

		if (Group->bExpanded)
		{
			for (TSharedPtr<FMLPParameterInfo>& Param : Group->Parameters)
			{
				GroupContainer->AddSlot()
					.AutoHeight()
					[
						SNew(SMaterialParameterRow)
						.Item(Param)
						.bSelected(Param->bSelected)
						.OnClicked(this, &SMaterialLayoutProPanel::OnRowClicked)
						.OnGroupChanged(this, &SMaterialLayoutProPanel::OnParameterGroupChanged)
						.OnPriorityChanged(this, &SMaterialLayoutProPanel::OnParameterPriorityChanged)
					];
			}
		}
	}
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildGroupHeader(TSharedPtr<FMLPParameterGroup> Group)
{
	if (!Group.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("InvalidGroup", "Invalid Group"));
	}

	FText ExpandedText = Group->bExpanded ? FText::FromString(TEXT("\u25BC")) : FText::FromString(TEXT("\u25B6"));

	return SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::SurfaceAlt())
		.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadSM())
		.OnMouseButtonDown_Lambda([this, Group](const FGeometry&, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
			{
				ToggleGroupExpansion(Group);
				return FReply::Handled();
			}
			return FReply::Unhandled();
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(ExpandedText)
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(STextBlock)
				.Text(FText::FromName(Group->Name))
				.Font(FMLPTheme::FontHeading())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("GroupCount", "{0}"), FText::AsNumber(Group->Parameters.Num())))
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			]
		];
}

void SMaterialLayoutProPanel::ClearSelection()
{
	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param.IsValid())
		{
			Param->bSelected = false;
		}
	}
}

void SMaterialLayoutProPanel::ToggleGroupExpansion(TSharedPtr<FMLPParameterGroup> Group)
{
	if (Group.IsValid())
	{
		Group->bExpanded = !Group->bExpanded;
		RebuildGroupList();
	}
}

void SMaterialLayoutProPanel::OnRowClicked(TSharedPtr<FMLPParameterInfo> Item, const FPointerEvent& MouseEvent)
{
	if (!Item.IsValid())
	{
		return;
	}

	const bool bCtrlDown = MouseEvent.IsControlDown();
	const bool bShiftDown = MouseEvent.IsShiftDown();

	if (bShiftDown && LastClickedItem.IsValid())
	{
		// Range selection
		TSharedPtr<FMLPParameterInfo> Start = LastClickedItem.Pin();
		bool bInRange = false;
		for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
		{
			if (Param == Start || Param == Item)
			{
				bInRange = !bInRange;
				Param->bSelected = true;
			}
			else if (bInRange)
			{
				Param->bSelected = true;
			}
			else if (!bCtrlDown)
			{
				Param->bSelected = false;
			}
		}
	}
	else if (bCtrlDown)
	{
		Item->bSelected = !Item->bSelected;
	}
	else
	{
		ClearSelection();
		Item->bSelected = true;
	}

	LastClickedItem = Item;
	RebuildGroupList();
}

void SMaterialLayoutProPanel::OnParameterGroupChanged(TSharedPtr<FMLPParameterInfo> Item, FName NewGroup)
{
	ApplyGroupChange(Item, NewGroup);
}

void SMaterialLayoutProPanel::OnParameterPriorityChanged(TSharedPtr<FMLPParameterInfo> Item, int32 NewValue)
{
	ApplyPriorityChange(Item, NewValue);
}

void SMaterialLayoutProPanel::ApplyGroupChange(TSharedPtr<FMLPParameterInfo> Item, FName NewGroup)
{
	if (!Item.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ChangeParameterGroup", "Change Parameter Group"));
	ApplyGroupChangeInternal(Item, NewGroup);
	if (UMaterialExpression* Expression = Item->Expression.Get())
	{
		if (UMaterial* Material = Cast<UMaterial>(Expression->GetOuter()))
		{
			Material->PostEditChange();
			Material->MarkPackageDirty();
		}
	}
	RefreshParameters();
}

void SMaterialLayoutProPanel::ApplyGroupChangeInternal(TSharedPtr<FMLPParameterInfo> Item, FName NewGroup)
{
	if (!Item.IsValid())
	{
		return;
	}

	UMaterialExpression* Expression = Item->Expression.Get();
	if (!Expression)
	{
		return;
	}

	UMaterialExpressionParameter* ParamExpression = Cast<UMaterialExpressionParameter>(Expression);
	if (!ParamExpression)
	{
		return;
	}

	ParamExpression->Modify();
	ParamExpression->Group = NewGroup;
	Item->Group = NewGroup;
}

void SMaterialLayoutProPanel::ApplyPriorityChange(TSharedPtr<FMLPParameterInfo> Item, int32 NewValue)
{
	if (!Item.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ChangeParameterSortPriority", "Change Parameter Sort Priority"));
	ApplyPriorityChangeInternal(Item, NewValue);
	if (UMaterialExpression* Expression = Item->Expression.Get())
	{
		if (UMaterial* Material = Cast<UMaterial>(Expression->GetOuter()))
		{
			Material->PostEditChange();
			Material->MarkPackageDirty();
		}
	}
	RefreshParameters();
}

void SMaterialLayoutProPanel::ApplyPriorityChangeInternal(TSharedPtr<FMLPParameterInfo> Item, int32 NewValue)
{
	if (!Item.IsValid())
	{
		return;
	}

	UMaterialExpression* Expression = Item->Expression.Get();
	if (!Expression)
	{
		return;
	}

	UMaterialExpressionParameter* ParamExpression = Cast<UMaterialExpressionParameter>(Expression);
	if (!ParamExpression)
	{
		return;
	}

	ParamExpression->Modify();
	ParamExpression->SortPriority = NewValue;
	Item->SortPriority = NewValue;
}

void SMaterialLayoutProPanel::ApplyGroupToSelected(FName NewGroup)
{
	if (!TargetMaterial.IsValid() || GetSelectedCount() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetGroupSelected", "Set Group for Selected Parameters"));
	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param.IsValid() && Param->bSelected)
		{
			ApplyGroupChangeInternal(Param, NewGroup);
		}
	}
	if (UMaterial* Material = TargetMaterial.Get())
	{
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}
	RefreshParameters();
}

void SMaterialLayoutProPanel::ArchiveUnused()
{
	if (!TargetMaterial.IsValid() || !HasUnusedParameters())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ArchiveUnusedParameters", "Archive Unused Parameters"));
	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param->Usage == EMLPParameterUsage::Unused)
		{
			ApplyGroupChangeInternal(Param, FName(TEXT("Deprecated")));
		}
	}
	if (UMaterial* Material = TargetMaterial.Get())
	{
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}
	RefreshParameters();
}

void SMaterialLayoutProPanel::DeleteUnused()
{
	if (!TargetMaterial.IsValid() || !HasUnusedParameters())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteUnusedParameters", "Delete Unused Parameters"));
	UMaterial* Material = TargetMaterial.Get();
	Material->Modify();

	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param->Usage == EMLPParameterUsage::Unused && Param->Expression.IsValid())
		{
			Material->Expressions.Remove(Param->Expression.Get());
		}
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();
	RefreshParameters();
}

void SMaterialLayoutProPanel::AutoGroup()
{
	if (!TargetMaterial.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AutoGroupParameters", "Auto Group Parameters"));

	// Simple prefix-based rules.
	struct FAutoGroupRule
	{
		FString Prefix;
		FString Group;
	};
	static const FAutoGroupRule Rules[] =
	{
		{ TEXT("MF_"),  TEXT("Master Faders") },
		{ TEXT("Tex_"), TEXT("Textures") },
		{ TEXT("Color_"),TEXT("Colors") },
		{ TEXT("R_"),   TEXT("Channels") },
		{ TEXT("G_"),   TEXT("Channels") },
		{ TEXT("B_"),   TEXT("Channels") },
		{ TEXT("A_"),   TEXT("Channels") },
		{ TEXT("N_"),   TEXT("Normals") },
		{ TEXT("Em_"),  TEXT("Emissive") },
		{ TEXT("Rgh_"), TEXT("Roughness") },
		{ TEXT("Met_"), TEXT("Metallic") },
		{ TEXT("Sp_"),  TEXT("Specular") },
	};

	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (!Param.IsValid()) continue;

		const FString NameStr = Param->Name.ToString();
		for (const FAutoGroupRule& Rule : Rules)
		{
			if (NameStr.StartsWith(Rule.Prefix))
			{
				ApplyGroupChangeInternal(Param, FName(*Rule.Group));
				break;
			}
		}
	}

	if (UMaterial* Material = TargetMaterial.Get())
	{
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}
	RefreshParameters();
}

void SMaterialLayoutProPanel::BulkRename(const FString& Find, const FString& Replace, bool bRegex)
{
	// TODO: implement bulk rename
}

void SMaterialLayoutProPanel::GroupByComment()
{
	if (!TargetMaterial.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("GroupByComment", "Group Parameters by Comment"));
	UMaterial* Material = TargetMaterial.Get();
	Material->Modify();

	for (UMaterialExpression* Expression : Material->Expressions)
	{
		UMaterialExpressionComment* Comment = Cast<UMaterialExpressionComment>(Expression);
		if (!Comment)
		{
			continue;
		}

		FString CommentTitle = Comment->Text;
		CommentTitle.TrimStartAndEndInline();
		if (CommentTitle.IsEmpty())
		{
			continue;
		}
		// Clean title to make a valid FName/Group.
		CommentTitle.ReplaceInline(TEXT(" "), TEXT("_"));
		CommentTitle.ReplaceInline(TEXT("-"), TEXT("_"));
		FName GroupName(*CommentTitle);

		const FVector2D CommentMin(Comment->MaterialExpressionEditorX, Comment->MaterialExpressionEditorY);
		const FVector2D CommentMax(Comment->MaterialExpressionEditorX + Comment->SizeX, Comment->MaterialExpressionEditorY + Comment->SizeY);

		for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
		{
			if (!Param.IsValid() || !Param->Expression.IsValid()) continue;

			UMaterialExpression* ParamExpression = Param->Expression.Get();
			const FVector2D Pos(ParamExpression->MaterialExpressionEditorX, ParamExpression->MaterialExpressionEditorY);
			if (Pos.X >= CommentMin.X && Pos.X <= CommentMax.X && Pos.Y >= CommentMin.Y && Pos.Y <= CommentMax.Y)
			{
				ApplyGroupChangeInternal(Param, GroupName);
			}
		}
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();
	RefreshParameters();
}

FText SMaterialLayoutProPanel::GetTargetMaterialName() const
{
	if (TargetMaterial.IsValid())
	{
		return FText::Format(LOCTEXT("TargetMaterial", "{0}"), FText::FromString(TargetMaterial->GetName()));
	}
	if (TargetMaterialInstance.IsValid())
	{
		return FText::Format(LOCTEXT("TargetMaterialInstance", "{0} (Instance)"), FText::FromString(TargetMaterialInstance->GetName()));
	}
	return LOCTEXT("NoTarget", "Select a material");
}

FText SMaterialLayoutProPanel::GetStatusText() const
{
	if (Parameters.Num() == 0)
	{
		if (!TargetMaterial.IsValid() && !TargetMaterialInstance.IsValid())
		{
			return LOCTEXT("NoSelectionStatus", "No material selected");
		}
		return LOCTEXT("NoParametersStatus", "No parameters found");
	}

	int32 UnusedCount = GetUnusedCount();
	int32 SelectedCount = GetSelectedCount();

	return FText::Format(
		LOCTEXT("ParameterCountStatus", "{0} parameters | {1} unused | {2} selected | {3} groups"),
		FText::AsNumber(Parameters.Num()),
		FText::AsNumber(UnusedCount),
		FText::AsNumber(SelectedCount),
		FText::AsNumber(Groups.Num()));
}

int32 SMaterialLayoutProPanel::GetSelectedCount() const
{
	int32 Count = 0;
	for (const TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param.IsValid() && Param->bSelected)
		{
			++Count;
		}
	}
	return Count;
}

int32 SMaterialLayoutProPanel::GetUnusedCount() const
{
	int32 Count = 0;
	for (const TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param.IsValid() && Param->Usage == EMLPParameterUsage::Unused)
		{
			++Count;
		}
	}
	return Count;
}

bool SMaterialLayoutProPanel::HasUnusedParameters() const
{
	return GetUnusedCount() > 0;
}

#undef LOCTEXT_NAMESPACE
