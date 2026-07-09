#include "Widgets/SMaterialLayoutProPanel.h"
#include "MaterialLayoutProTheme.h"
#include "MaterialLayoutProSettings.h"
#include "Model/MaterialLayoutViewModel.h"
#include "Model/MaterialParameterScanner.h"
#include "Widgets/SMaterialParameterRow.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Engine/Texture.h"
#include "Widgets/SMaterialBulkRenameDialog.h"
#include "Widgets/SMaterialSortWorkbench.h"
#include "Widgets/SMaterialParameterEditor.h"

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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_STYLE FAppStyle
#else
#include "EditorStyleSet.h"
#define MLP_STYLE FEditorStyle
#endif

#define LOCTEXT_NAMESPACE "SMaterialLayoutProPanel"

SMaterialLayoutProPanel::~SMaterialLayoutProPanel()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
}

void SMaterialLayoutProPanel::Construct(const FArguments& InArgs)
{
	Session = MakeShared<FMLPSession>();

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::Background())
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(6.f, 4.f, 6.f, 4.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,6)) [ BuildToolbar() ]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,4)) [ BuildStatusBar() ]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(ModeSwitcher, SWidgetSwitcher)
				+ SWidgetSwitcher::Slot() [ BuildWideMode() ]
				+ SWidgetSwitcher::Slot() [ BuildNarrowMode() ]
			]
		]
	];

	USelection::SelectionChangedEvent.AddSP(SharedThis(this), &SMaterialLayoutProPanel::OnSelectionChanged);
	OnSelectionChanged(nullptr);
}

void SMaterialLayoutProPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	UpdateLayoutMode(AllottedGeometry.GetLocalSize().X);
}

void SMaterialLayoutProPanel::UpdateLayoutMode(float Width)
{
	// Hysteresis: >496 → wide, <464 → narrow (prevents jitter near the threshold).
	bool bNewMode = bIsWideMode;
	if (!bIsWideMode && Width > 496.f) bNewMode = true;
	else if (bIsWideMode && Width < 464.f) bNewMode = false;

	if (bNewMode != bIsWideMode)
	{
		bIsWideMode = bNewMode;
		if (ModeSwitcher.IsValid())
		{
			ModeSwitcher->SetActiveWidgetIndex(bIsWideMode ? 0 : 1);
		}
	}
	CachedWidth = Width;
}

// ============================================================================
// Layout builders
// ============================================================================

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildWideMode()
{
	return SNew(SHorizontalBox)
	+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(FMargin(0,0,4,0))
	[
		SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::SurfaceAlt())
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadSM())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,4))
			[
				SAssignNew(SearchBox, SEditableTextBox)
				.HintText(LOCTEXT("SearchHint", "搜索参数..."))
				.Font(FMLPTheme::FontSmall())
				.OnTextChanged(this, &SMaterialLayoutProPanel::OnSearchChanged)
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot() [ SAssignNew(LeftTreeContainer, SVerticalBox) ]
			]
		]
	]
	+ SHorizontalBox::Slot().FillWidth(0.58f)
	[
		SNew(SBorder)
		.BorderBackgroundColor(FMLPTheme::Surface())
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMLPTheme::PadMD())
		[
			SAssignNew(RightDetailContainer, SVerticalBox)
		]
	];
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildNarrowMode()
{
	return SNew(SBorder)
	.BorderBackgroundColor(FMLPTheme::SurfaceAlt())
	.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
	.Padding(FMLPTheme::PadSM())
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0,0,0,4))
		[
			SAssignNew(SearchBox, SEditableTextBox)
			.HintText(LOCTEXT("SearchHintN", "搜索参数..."))
			.Font(FMLPTheme::FontSmall())
			.OnTextChanged(this, &SMaterialLayoutProPanel::OnSearchChanged)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot() [ SAssignNew(NarrowListContainer, SVerticalBox) ]
		]
	];
}

void SMaterialLayoutProPanel::RebuildLeftTree()
{
	if (!LeftTreeContainer.IsValid()) return;
	LeftTreeContainer->ClearChildren();

	if (!Session.IsValid() || Session->Groups.Num() == 0)
	{
		LeftTreeContainer->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoParams", "未找到参数"))
			.ColorAndOpacity(FMLPTheme::Muted())
			.Font(FMLPTheme::FontBody())
		];
		return;
	}

	for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
	{
		if (!Group.IsValid()) continue;

		// Count visible params under this group.
		int32 VisibleCount = 0;
		for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
		{
			if (PassesFilter(Param)) ++VisibleCount;
		}
		if (VisibleCount == 0) continue; // hide empty groups when filtering

		// Group header.
		LeftTreeContainer->AddSlot().AutoHeight()
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(FMLPTheme::Accent().R, FMLPTheme::Accent().G, FMLPTheme::Accent().B, 0.15f))
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMargin(6.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("▼ %s (%d)"), *Group->Name.ToString(), VisibleCount)))
					.Font(FMLPTheme::FontHeading())
					.ColorAndOpacity(FMLPTheme::Foreground())
				]
			]
		];

		// Parameter rows.
		for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
		{
			if (!PassesFilter(Param)) continue;
			const bool bSel = (SelectedParam == Param);
			LeftTreeContainer->AddSlot().AutoHeight().Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
				.ContentPadding(FMargin(0.f))
				.HAlign(HAlign_Fill)
				.OnClicked_Lambda([this, Param]() -> FReply { SelectParam(Param); return FReply::Handled(); })
				[
					SNew(SMaterialParameterRow)
					.ParamVM(Param)
					.Session(Session)
					.bSelected(bSel)
				]
			];
		}
	}
}

void SMaterialLayoutProPanel::RebuildNarrowList()
{
	if (!NarrowListContainer.IsValid()) return;
	NarrowListContainer->ClearChildren();

	if (!Session.IsValid() || Session->Groups.Num() == 0)
	{
		NarrowListContainer->AddSlot()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoParamsN", "未找到参数"))
			.ColorAndOpacity(FMLPTheme::Muted())
			.Font(FMLPTheme::FontBody())
		];
		return;
	}

	for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
	{
		if (!Group.IsValid()) continue;
		int32 VisibleCount = 0;
		for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
		{
			if (PassesFilter(Param)) ++VisibleCount;
		}
		if (VisibleCount == 0) continue;

		NarrowListContainer->AddSlot().AutoHeight()
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(FMLPTheme::Accent().R, FMLPTheme::Accent().G, FMLPTheme::Accent().B, 0.15f))
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMargin(6.f, 3.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("▼ %s (%d)"), *Group->Name.ToString(), VisibleCount)))
				.Font(FMLPTheme::FontHeading())
				.ColorAndOpacity(FMLPTheme::Foreground())
			]
		];

		// In narrow mode the row shows its value editor inline (no separate detail pane).
		for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
		{
			if (!PassesFilter(Param)) continue;
			NarrowListContainer->AddSlot().AutoHeight().Padding(FMargin(4.f, 0.f, 0.f, 0.f))
			[
				SNew(SMaterialParameterRow)
				.ParamVM(Param)
				.Session(Session)
				.bSelected(false)
				.bDetailMode(true)
			];
		}
	}
}

void SMaterialLayoutProPanel::RebuildRightDetail()
{
	if (!RightDetailContainer.IsValid()) return;
	RightDetailContainer->ClearChildren();

	if (!SelectedParam.IsValid())
	{
		RightDetailContainer->AddSlot().Padding(FMLPTheme::PadMD())
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectPrompt", "从左侧选择一个参数查看详情"))
			.ColorAndOpacity(FMLPTheme::Muted())
			.Font(FMLPTheme::FontBody())
		];
		return;
	}

	// Title row: type pill + name + status badge.
	RightDetailContainer->AddSlot().AutoHeight().Padding(FMargin(0,0,0,8))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			FMLPTheme::MakeTypePill(
				FMLPTheme::GetTypeAbbrForType(SelectedParam->Type),
				FMLPTheme::GetTypeColorForType(SelectedParam->Type))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMLPTheme::PadSM())
		[
			SNew(STextBlock)
			.Text(FText::FromName(SelectedParam->Name))
			.Font(FMLPTheme::FontTitle())
			.ColorAndOpacity(FMLPTheme::Foreground())
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMLPTheme::PadSM())
		[
			SNew(SBorder)
			.BorderBackgroundColor(SelectedParam->GetUsageBgColor())
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMargin(4.f, 1.f))
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(SelectedParam->GetUsageLabel())
				.Font(FMLPTheme::FontBadge())
				.ColorAndOpacity(SelectedParam->GetUsageColor())
			]
		]
	];

	// Detail editor (group + priority editors visible).
	RightDetailContainer->AddSlot().FillHeight(1.0f)
	[
		SNew(SMaterialParameterRow)
		.ParamVM(SelectedParam)
		.Session(Session)
		.bSelected(true)
		.bDetailMode(true)
	];
}

// ============================================================================
// Selection + search
// ============================================================================

void SMaterialLayoutProPanel::SelectParam(TSharedPtr<FMLPParamVM> Param)
{
	SelectedParam = Param;
	RebuildLeftTree();
	RebuildRightDetail();
}

void SMaterialLayoutProPanel::OnSearchChanged(const FText& NewText)
{
	SearchText = NewText.ToString();
	RebuildLeftTree();
	RebuildNarrowList();
}

bool SMaterialLayoutProPanel::PassesFilter(const TSharedPtr<FMLPParamVM>& Param) const
{
	if (!Param.IsValid()) return false;
	if (SearchText.IsEmpty()) return true;
	return Param->Name.ToString().Contains(SearchText);
}

// ============================================================================
// Data refresh
// ============================================================================

void SMaterialLayoutProPanel::RefreshParameters()
{
	if (!Session.IsValid()) return;

	if (TargetMaterial.IsValid())
	{
		Session->TargetMaterial = TargetMaterial;
		Session->PullAll();
	}
	else
	{
		Session->Groups.Reset();
		SelectedParam.Reset();
	}
	RebuildLeftTree();
	RebuildNarrowList();
	RebuildRightDetail();
}

void SMaterialLayoutProPanel::OnSelectionChanged(UObject* Selection)
{
	UMaterial* NewMat = nullptr;
	UMaterialInstance* NewMI = nullptr;
	if (GEditor)
	{
		USelection* Sel = GEditor->GetSelectedObjects();
		if (Sel) for (FSelectionIterator It(*Sel); It; ++It)
		{
			UObject* O = *It;
			if (!NewMat && O && O->IsA<UMaterial>()) { NewMat = Cast<UMaterial>(O); break; }
			if (!NewMI && O && O->IsA<UMaterialInstance>()) NewMI = Cast<UMaterialInstance>(O);
		}
	}
	if (NewMat == TargetMaterial.Get() && NewMI == TargetMaterialInstance.Get()) return;
	TargetMaterial = NewMat; TargetMaterialInstance = NewMI;
	RefreshParameters();
}

// ============================================================================
// Toolbar + status
// ============================================================================

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildToolbar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[ SNew(STextBlock).Text(LOCTEXT("TL","目标：")).Font(FMLPTheme::FontBody()).ColorAndOpacity(FMLPTheme::Muted()) ]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMLPTheme::PadSM())
		[ SNew(STextBlock).Text(this,&SMaterialLayoutProPanel::GetTargetMaterialName).Font(FMLPTheme::FontHeading()).ColorAndOpacity(FMLPTheme::Foreground()) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadH())
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("R","刷新")).ToolTipText(LOCTEXT("RT","重新扫描参数")).OnClicked(this,&SMaterialLayoutProPanel::OnRefreshClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadH())
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("S","定位")).ToolTipText(LOCTEXT("ST","在内容浏览器中定位材质")).OnClicked(this,&SMaterialLayoutProPanel::OnSelectMaterialClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadH())
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("OE","在编辑器中打开")).ToolTipText(LOCTEXT("OET","在材质编辑器中打开此材质")).OnClicked(this,&SMaterialLayoutProPanel::OnOpenMaterialEditorClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMLPTheme::PadH())
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("AG","自动分组")).ToolTipText(LOCTEXT("AGT","按名称前缀自动分组参数")).OnClicked(this,&SMaterialLayoutProPanel::OnAutoGroupClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMLPTheme::PadH())
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("GBC","按注释分组")).ToolTipText(LOCTEXT("GBCT","按包含参数的注释框对参数进行分组")).OnClicked(this,&SMaterialLayoutProPanel::OnGroupByCommentClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("AU","归档未使用")).ToolTipText(LOCTEXT("AUT","将未使用的参数移至已废弃分组")).OnClicked(this,&SMaterialLayoutProPanel::OnArchiveUnusedClicked) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton")
			.ButtonColorAndOpacity(FMLPTheme::ButtonDanger()).ForegroundColor(FMLPTheme::ButtonTextOnColor())
			.ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("DU","删除未使用")).ToolTipText(LOCTEXT("DUT","删除未使用的参数")).OnClicked(this,&SMaterialLayoutProPanel::OnDeleteUnusedClicked) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("BR","批量重命名")).ToolTipText(LOCTEXT("BRT","批量重命名选中的参数")).OnClicked(this,&SMaterialLayoutProPanel::OnBulkRenameClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("EX","导出")).ToolTipText(LOCTEXT("EXT","将参数列表导出为 CSV")).OnClicked(this,&SMaterialLayoutProPanel::OnExportClicked) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("IM","导入")).ToolTipText(LOCTEXT("IMT","从 CSV 文件导入分组/排序优先级回材质")).OnClicked(this,&SMaterialLayoutProPanel::OnImportClicked) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("SW","排序工作台")).ToolTipText(LOCTEXT("SWT","打开独立窗口重新排列分组和排序优先级")).OnClicked(this,&SMaterialLayoutProPanel::OnSortWorkbenchClicked) ]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("PE","参数编辑器")).ToolTipText(LOCTEXT("PET","打开 Houdini 风格的参数编辑器")).OnClicked(this,&SMaterialLayoutProPanel::OnParameterEditorClicked) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[ SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton")
			.ButtonColorAndOpacity(FMLPTheme::ButtonPrimary()).ForegroundColor(FMLPTheme::ButtonTextOnColor())
			.ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("Apply","应用更改")).ToolTipText(LOCTEXT("ApplyT","将未提交的参数改动写回材质")).OnClicked(this,&SMaterialLayoutProPanel::OnApplyChangesClicked) ]
		;
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildStatusBar()
{
	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::SurfaceAlt().R,FMLPTheme::SurfaceAlt().G,FMLPTheme::SurfaceAlt().B,0.7f))
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(8,3))
		[
			SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[ SNew(STextBlock).Text(this,&SMaterialLayoutProPanel::GetStatusText).Font(FMLPTheme::FontSmall()).ColorAndOpacity(FMLPTheme::Muted()) ]
		];
}

// ============================================================================
// Toolbar handlers
// ============================================================================

FReply SMaterialLayoutProPanel::OnRefreshClicked() { RefreshParameters(); return FReply::Handled(); }

FReply SMaterialLayoutProPanel::OnApplyChangesClicked()
{
	if (Session.IsValid())
	{
		Session->PushDirty();
		RefreshParameters();
	}
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnSelectMaterialClicked()
{
	UObject* O = TargetMaterial.IsValid() ? (UObject*)TargetMaterial.Get() : (TargetMaterialInstance.IsValid() ? (UObject*)TargetMaterialInstance.Get() : nullptr);
	if (O) { TArray<FAssetData> A; A.Add(FAssetData(O)); auto& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser"); CB.Get().SyncBrowserToAssets(A, true); }
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnOpenMaterialEditorClicked()
{
	UObject* O = TargetMaterial.IsValid() ? (UObject*)TargetMaterial.Get() : (TargetMaterialInstance.IsValid() ? (UObject*)TargetMaterialInstance.Get() : nullptr);
	if (O) GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(O);
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnArchiveUnusedClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const auto* S = GetDefault<UMaterialLayoutProSettings>();
	const FName Dep(S ? *S->DeprecatedGroupName : TEXT("Deprecated"));
	const FScopedTransaction T(LOCTEXT("AU","归档未使用的参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	FMaterialParameterUsageAnalyzer::Analyze(M, Params);
	for (auto& P : Params) if (P->Usage == EMLPParameterUsage::Unused)
		if (auto* E = Cast<UMaterialExpressionParameter>(P->Expression.Get())) { E->Modify(); E->Group = Dep; }
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnDeleteUnusedClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const FScopedTransaction T(LOCTEXT("DU","删除未使用的参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	FMaterialParameterUsageAnalyzer::Analyze(M, Params);
	for (auto& P : Params) if (P->Usage == EMLPParameterUsage::Unused && P->Expression.IsValid())
#if ENGINE_MAJOR_VERSION >= 5
		M->GetExpressionCollection().RemoveExpression(P->Expression.Get());
#else
		M->Expressions.Remove(P->Expression.Get());
#endif
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnAutoGroupClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const auto* S = GetDefault<UMaterialLayoutProSettings>(); if (!S) return FReply::Handled();
	const FScopedTransaction T(LOCTEXT("AG","自动分组参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	for (auto& P : Params) { if (!P.IsValid()) continue; const FString N = P->Name.ToString();
		for (const auto& R : S->AutoGroupRules) if (N.StartsWith(R.Prefix)) {
			if (auto* E = Cast<UMaterialExpressionParameter>(P->Expression.Get())) { E->Modify(); E->Group = FName(*R.Group); } break; } }
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnGroupByCommentClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto* M = TargetMaterial.Get(); const FScopedTransaction T(LOCTEXT("GBC","按注释分组参数"));
	M->Modify();
#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* E : M->GetExpressions())
#else
	for (UMaterialExpression* E : M->Expressions)
#endif
	{
		auto* C = Cast<UMaterialExpressionComment>(E); if (!C) continue;
		FString Title = C->Text; Title.TrimStartAndEndInline(); if (Title.IsEmpty()) continue;
		Title.ReplaceInline(TEXT(" "),TEXT("_")); Title.ReplaceInline(TEXT("-"),TEXT("_")); FName GN(*Title);
		const FVector2D Min(C->MaterialExpressionEditorX,C->MaterialExpressionEditorY), Max(C->MaterialExpressionEditorX+C->SizeX,C->MaterialExpressionEditorY+C->SizeY);
		auto Params = FMaterialParameterScanner::ScanMaterial(M);
		for (auto& P : Params) { if (!P.IsValid()||!P->Expression.IsValid()) continue;
			const FVector2D Pos(P->Expression->MaterialExpressionEditorX,P->Expression->MaterialExpressionEditorY);
			if (Pos.X>=Min.X&&Pos.X<=Max.X&&Pos.Y>=Min.Y&&Pos.Y<=Max.Y)
				if (auto* E2 = Cast<UMaterialExpressionParameter>(P->Expression.Get())) { E2->Modify(); E2->Group = GN; } }
	}
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnBulkRenameClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto Params = FMaterialParameterScanner::ScanMaterial(TargetMaterial.Get());
	FSlateApplication::Get().AddWindow(SNew(SMaterialBulkRenameDialog).TargetMaterial(TargetMaterial).Parameters(Params)
		.OnRenamed(FSimpleDelegate::CreateLambda([this](){ RefreshParameters(); })));
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnExportClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto Params = FMaterialParameterScanner::ScanMaterial(TargetMaterial.Get());
	FMaterialParameterUsageAnalyzer::Analyze(TargetMaterial.Get(), Params);
	if (Params.Num()==0) return FReply::Handled();
	auto* DP = FDesktopPlatformModule::Get(); if (!DP) return FReply::Handled();
	TArray<FString> F;
	if (DP->SaveFileDialog(nullptr, LOCTEXT("ED","导出参数").ToString(), FPaths::ProjectSavedDir(),
		FString::Printf(TEXT("%s_Parameters.csv"), *TargetMaterial->GetName()), TEXT("CSV files|*.csv"), EFileDialogFlags::None, F) && F.Num()>0)
	{
		FString CSV = TEXT("Name,Type,Group,SortPriority,Usage,Value\n");
		for (auto& P : Params) if (P.IsValid())
			CSV += FString::Printf(TEXT("%s,%s,%s,%d,%s,%s\n"), *P->Name.ToString(), *P->GetDisplayTypeName().ToString(),
				*P->Group.ToString(), P->SortPriority, *P->GetUsageLabel().ToString(), *P->ValueString);
		FFileHelper::SaveStringToFile(CSV, *F[0]);
	}
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnImportClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto* DP = FDesktopPlatformModule::Get(); if (!DP) return FReply::Handled();
	TArray<FString> F;
	if (DP->OpenFileDialog(nullptr, LOCTEXT("ID","从 CSV 导入参数").ToString(), FPaths::ProjectSavedDir(), TEXT(""), TEXT("CSV files|*.csv"), EFileDialogFlags::None, F) && F.Num()>0)
	{
		FString C; if (!FFileHelper::LoadFileToString(C, *F[0])) return FReply::Handled();
		TArray<FString> L; C.ParseIntoArrayLines(L); if (L.Num()<2) return FReply::Handled();
		TMap<FName, UMaterialExpressionParameter*> M;
#if ENGINE_MAJOR_VERSION >= 5
		for (auto* E : TargetMaterial->GetExpressions())
#else
		for (auto* E : TargetMaterial->Expressions)
#endif
			if (auto* P = Cast<UMaterialExpressionParameter>(E)) M.Add(P->ParameterName, P);
		const FScopedTransaction T(LOCTEXT("IP","从 CSV 导入参数"));
		auto* Mat = TargetMaterial.Get(); Mat->Modify();
		for (int32 i=1;i<L.Num();++i) { if (L[i].IsEmpty()) continue;
			TArray<FString> Fl; L[i].ParseIntoArray(Fl, TEXT(","), false); if (Fl.Num()<4) continue;
			auto** P = M.Find(FName(*Fl[0])); if (!P||!*P) continue;
			(*P)->Modify(); (*P)->Group = FName(*Fl[2]); (*P)->SortPriority = FCString::Atoi(*Fl[3]); }
		Mat->PostEditChange(); Mat->MarkPackageDirty(); RefreshParameters();
	}
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnSortWorkbenchClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto Params = FMaterialParameterScanner::ScanMaterial(TargetMaterial.Get());
	FSlateApplication::Get().AddWindow(SNew(SMaterialSortWorkbench).TargetMaterial(TargetMaterial).Parameters(Params)
		.OnApplied(FSimpleDelegate::CreateLambda([this](){ RefreshParameters(); })));
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnParameterEditorClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	// Share the main panel's session snapshot — value edits land on the VM, flushed on Apply.
	FSlateApplication::Get().AddWindow(SNew(SMaterialParameterEditor).Session(Session).TargetInstance(TargetMaterialInstance)
		.OnApplied(FSimpleDelegate::CreateLambda([this](){ RefreshParameters(); })));
	return FReply::Handled();
}

// ============================================================================
// Status text
// ============================================================================

FText SMaterialLayoutProPanel::GetTargetMaterialName() const
{
	if (TargetMaterial.IsValid()) return FText::Format(LOCTEXT("TM","{0}"), FText::FromString(TargetMaterial->GetName()));
	if (TargetMaterialInstance.IsValid()) return FText::Format(LOCTEXT("TMI","{0}（实例）"), FText::FromString(TargetMaterialInstance->GetName()));
	return LOCTEXT("NT","选择一个材质");
}

FText SMaterialLayoutProPanel::GetStatusText() const
{
	if (!TargetMaterial.IsValid() && !TargetMaterialInstance.IsValid()) return LOCTEXT("NS","未选择材质");
	if (!Session.IsValid() || Session->Groups.Num() == 0) return LOCTEXT("NP","未找到参数");

	int32 Total = 0;
	for (const auto& G : Session->Groups) if (G.IsValid()) Total += G->Parameters.Num();
	if (Total == 0) return LOCTEXT("NP2","未找到参数");

	FString Base = FString::Printf(TEXT("%d 个参数 | %d 个分组"), Total, Session->Groups.Num());
	if (Session->HasDirty()) Base += TEXT("  |  ● 有未提交改动");
	return FText::FromString(Base);
}

#undef LOCTEXT_NAMESPACE
