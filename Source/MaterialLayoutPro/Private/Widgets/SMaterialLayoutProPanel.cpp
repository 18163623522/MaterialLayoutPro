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
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "IMaterialEditor.h"
#include "MaterialEditorModule.h"
#include "Toolkits/AssetEditorManager.h"
#include "Toolkits/AssetEditorToolkit.h"

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
	if (Session.IsValid() && MaterialChangedHandle.IsValid())
	{
		Session->OnMaterialChanged().Remove(MaterialChangedHandle);
	}
}

void SMaterialLayoutProPanel::Construct(const FArguments& InArgs)
{
	Session = MakeShared<FMLPSession>();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(2.f, 2.f, 2.f, 2.f)) [ BuildToolbar() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(2.f, 0.f, 2.f, 2.f))
		[
			SAssignNew(SearchBox, SEditableTextBox)
			.HintText(LOCTEXT("SearchHint", "搜索参数..."))
			.Font(FMLPTheme::FontSmall())
			.OnTextChanged(this, &SMaterialLayoutProPanel::OnSearchChanged)
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot() [ SAssignNew(TreeContainer, SVerticalBox) ]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(2.f, 2.f, 2.f, 2.f)) [ BuildStatusBar() ]
	];

	// Subscribe to session writes — when a value is pushed back to the material,
	// notify the bound material editor so its node UI / details panel refreshes too.
	if (Session.IsValid())
	{
		MaterialChangedHandle = Session->OnMaterialChanged().AddSP(SharedThis(this), &SMaterialLayoutProPanel::OnMaterialChangedBySession);
	}

	// Bind to a material editor (embedded mode) or GEditor selection (standalone mode).
	if (InArgs._OwningMaterialEditor.IsValid())
	{
		BindToMaterialEditor(InArgs._OwningMaterialEditor);
	}
	else
	{
		USelection::SelectionChangedEvent.AddSP(SharedThis(this), &SMaterialLayoutProPanel::OnSelectionChanged);
		ResolveTargetMaterial();
		RefreshParameters();
	}
}

// ============================================================================
// Data source resolution
// ============================================================================

void SMaterialLayoutProPanel::BindToMaterialEditor(TWeakPtr<IMaterialEditor> InEditor)
{
	OwningMaterialEditor = InEditor;
	// Embedded mode: re-resolve whenever this editor's asset changes (handled via Tick-like
	// polling in ResolveTargetMaterial; material editors don't broadcast asset swap cleanly
	// in 4.26 without hooking private delegates). For now we resolve on construct + refresh.
	ResolveTargetMaterial();
	RefreshParameters();
}

void SMaterialLayoutProPanel::OnMaterialChangedBySession()
{
	// The session wrote a new value/group/name back to the material. Tell the bound material
	// editor to refresh its graph nodes + details panel so the change is visible there too.
	if (OwningMaterialEditor.IsValid())
	{
		TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
		if (Editor.IsValid())
		{
			Editor->NotifyExternalMaterialChange();
		}
	}
}

void SMaterialLayoutProPanel::ResolveTargetMaterial()
{
	if (OwningMaterialEditor.IsValid())
	{
		// Embedded mode: ask the editor for the material it's editing.
		TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
		if (Editor.IsValid())
		{
			UMaterialInterface* MatInterface = Editor->GetMaterialInterface();
			if (UMaterial* Mat = Cast<UMaterial>(MatInterface))
			{
				TargetMaterial = Mat;
				TargetMaterialInstance = nullptr;
				return;
			}
			if (UMaterialInstance* MI = Cast<UMaterialInstance>(MatInterface))
			{
				TargetMaterialInstance = MI;
				TargetMaterial = MI ? MI->GetBaseMaterial() : nullptr;
				return;
			}
		}
		TargetMaterial.Reset();
		TargetMaterialInstance.Reset();
		return;
	}

	// Standalone mode: find any currently-open material editor and bind to it.
	UAssetEditorSubsystem* AssetEditorSS = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (AssetEditorSS)
	{
		TArray<UObject*> EditedAssets = AssetEditorSS->GetAllEditedAssets();
		for (UObject* Asset : EditedAssets)
		{
			if (!Asset) continue;
			if (Asset->IsA<UMaterialInterface>())
			{
				IAssetEditorInstance* EditorInstance = AssetEditorSS->FindEditorForAsset(Asset, false);
				if (EditorInstance)
				{
					// Recover a weak ptr via SharedFromThis (FAAssetEditorToolkit derives from it).
					FAssetEditorToolkit* AsToolkit = static_cast<FAssetEditorToolkit*>(EditorInstance);
					IMaterialEditor* MatEditor = static_cast<IMaterialEditor*>(EditorInstance);
					OwningMaterialEditor = StaticCastSharedRef<IMaterialEditor>(AsToolkit->AsShared());
					UMaterialInterface* MatInterface = MatEditor->GetMaterialInterface();
					if (UMaterial* Mat = Cast<UMaterial>(MatInterface))
					{
						TargetMaterial = Mat;
						TargetMaterialInstance = nullptr;
					}
					else if (UMaterialInstance* MI = Cast<UMaterialInstance>(MatInterface))
					{
						TargetMaterialInstance = MI;
						TargetMaterial = MI->GetBaseMaterial();
					}
					return;
				}
			}
		}
	}

	// Fallback: GEditor object selection (rarely hit for asset clicks, but covers actor selection).
	TargetMaterial.Reset();
	TargetMaterialInstance.Reset();
}

void SMaterialLayoutProPanel::OnSelectionChanged(UObject* Selection)
{
	if (OwningMaterialEditor.IsValid()) return; // embedded mode ignores content-browser selection
	UMaterial* OldMat = TargetMaterial.Get();
	UMaterialInstance* OldMI = TargetMaterialInstance.Get();
	ResolveTargetMaterial();
	if (TargetMaterial.Get() == OldMat && TargetMaterialInstance.Get() == OldMI) return;
	RefreshParameters();
}

void SMaterialLayoutProPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// --- Graph→Panel selection sync (every frame, cheap) ---
	// When the user selects nodes in the material graph, highlight the matching row.
	// Skip for a short cooldown after a panel→graph sync to avoid feedback loops.
	if (OwningMaterialEditor.IsValid() && InCurrentTime > SyncCooldownUntil && SelectedParams.Num() <= 1)
	{
		TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
		if (Editor.IsValid() && Session.IsValid())
		{
			TSet<UObject*> GraphSelection = Editor->GetSelectedNodes();
			TSharedPtr<FMLPParamVM> NewSel;
			for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
			{
				for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
				{
					if (Param.IsValid() && Param->SourceExpression.IsValid())
					{
						if (GraphSelection.Contains(Param->SourceExpression.Get()))
						{
							NewSel = Param;
							break;
						}
					}
				}
				if (NewSel.IsValid()) break;
			}
				if (NewSel.IsValid() && !IsSelected(NewSel))
				{
					SelectedParams.Reset();
					SelectedParams.Add(NewSel);
					LastSelectedParam = NewSel;
					RebuildTree();
				}
				else if (!NewSel.IsValid() && SelectedParams.Num() > 0)
				{
					SelectedParams.Reset();
					LastSelectedParam.Reset();
					RebuildTree();
				}
		}
	}

	// --- Editor binding poll (every ~0.5s) ---
	if (LastPollTime.IsSet() && InCurrentTime - LastPollTime.GetValue() < 0.5)
	{
		return;
	}
	LastPollTime = InCurrentTime;

	if (OwningMaterialEditor.IsValid())
	{
		// Already bound — check if the editor is still alive and the material is still valid.
		TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
		if (!Editor.IsValid() || !Editor->GetMaterialInterface())
		{
			OwningMaterialEditor.Reset();
			ResolveTargetMaterial();
			RefreshParameters();
		}
		else
		{
			UMaterialInterface* MatInterface = Editor->GetMaterialInterface();
			UMaterial* CurrentMat = Cast<UMaterial>(MatInterface);
			if (!CurrentMat) CurrentMat = Cast<UMaterialInstance>(MatInterface) ? Cast<UMaterialInstance>(MatInterface)->GetBaseMaterial() : nullptr;
			if (CurrentMat && CurrentMat != TargetMaterial.Get())
			{
				RefreshParameters();
			}
		}
	}
	else
	{
		UMaterial* OldMat = TargetMaterial.Get();
		ResolveTargetMaterial();
		if (TargetMaterial.IsValid() && TargetMaterial.Get() != OldMat)
		{
			RefreshParameters();
		}
	}
}

void SMaterialLayoutProPanel::RebuildTree()
{
	if (!TreeContainer.IsValid()) return;
	TreeContainer->ClearChildren();

	if (!Session.IsValid() || Session->Groups.Num() == 0)
	{
		TreeContainer->AddSlot()
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

		int32 VisibleCount = 0;
		for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
		{
			if (PassesFilter(Param)) ++VisibleCount;
		}
		if (VisibleCount == 0) continue;

		// Group header — compact, muted (original-style: small gray text, no colored bar).
		TreeContainer->AddSlot().AutoHeight().Padding(FMargin(2.f, 6.f, 0.f, 2.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%s  (%d)"), *Group->Name.ToString(), VisibleCount)))
			.Font(FMLPTheme::FontSmall())
			.ColorAndOpacity(FMLPTheme::Muted())
		];

		// Inline parameter rows — clickable background (not a button) so internal
		// editable boxes / value controls handle their own clicks without conflict.
		for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
		{
			if (!PassesFilter(Param)) continue;
			const bool bSel = IsSelected(Param);
			TreeContainer->AddSlot().AutoHeight()
			[
				SNew(SMaterialParameterRow)
				.ParamVM(Param)
				.Session(Session)
				.bSelected(bSel)
				.bDetailMode(true)
				.OnClicked(FOnRowClicked::CreateSP(SharedThis(this), &SMaterialLayoutProPanel::SelectParam))
			];
		}
	}
}

// ============================================================================
// Selection + search
// ============================================================================

void SMaterialLayoutProPanel::SelectParam(TSharedPtr<FMLPParamVM> Param, bool bCtrl, bool bShift)
{
	UE_LOG(LogTemp, Warning, TEXT("[MLP-Sel] SelectParam bCtrl=%d bShift=%d Param=%s"), bCtrl, bShift, Param.IsValid() ? *Param->Name.ToString() : TEXT("null"));

	if (!Param.IsValid())
	{
		ClearSelection();
		RebuildTree();
		return;
	}

	if (bCtrl)
	{
		// Toggle this param in the selection.
		if (IsSelected(Param)) SelectedParams.Remove(Param);
		else SelectedParams.Add(Param);
	}
	else if (bShift && LastSelectedParam.IsValid())
	{
		// Range select from LastSelectedParam to Param.
		bool bFoundStart = false;
		for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
		{
			for (const TSharedPtr<FMLPParamVM>& P : Group->Parameters)
			{
				if (P == LastSelectedParam || P == Param)
				{
					if (!IsSelected(P)) SelectedParams.Add(P);
					bFoundStart = true;
					continue;
				}
				if (bFoundStart)
				{
					if (!IsSelected(P)) SelectedParams.Add(P);
					// Stop if we reached the target.
					if (P == Param) break;
				}
			}
			if (bFoundStart) break;
		}
	}
	else
	{
		// Single select - replace.
		SelectedParams.Reset();
		SelectedParams.Add(Param);
	}
	LastSelectedParam = Param;
	RebuildTree();

	// Sync to material graph. JumpToExpression causes the graph to pan/zoom to the
	// node - only do that for single select. Multi-select (Ctrl/Shift) just adds to
	// selection without jumping, so the viewport doesn't jump around.
	if (Param->SourceExpression.IsValid() && OwningMaterialEditor.IsValid())
	{
		TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
		if (Editor.IsValid())
		{
			SyncCooldownUntil = FSlateApplication::Get().GetCurrentTime() + 0.5;
			Editor->AddToSelection(Param->SourceExpression.Get());
			if (!bCtrl && !bShift)
			{
				Editor->JumpToExpression(Param->SourceExpression.Get());
			}
		}
	}
}

void SMaterialLayoutProPanel::ClearSelection()
{
	SelectedParams.Reset();
	LastSelectedParam.Reset();
}

bool SMaterialLayoutProPanel::IsSelected(TSharedPtr<FMLPParamVM> Param) const
{
	return SelectedParams.Contains(Param);
}

void SMaterialLayoutProPanel::OnSearchChanged(const FText& NewText)
{
	SearchText = NewText.ToString();
	RebuildTree();
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

	// In embedded mode, re-resolve the target (editor may have switched assets).
	if (OwningMaterialEditor.IsValid())
	{
		ResolveTargetMaterial();
	}

	if (TargetMaterial.IsValid())
	{
		Session->TargetMaterial = TargetMaterial;
		Session->PullAll();
	}
	else
	{
		Session->Groups.Reset();
		ClearSelection();
	}
	RebuildTree();
}

// ============================================================================
// Toolbar + status
// ============================================================================

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildToolbar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SMaterialLayoutProPanel::GetTargetMaterialName)
			.Font(FMLPTheme::FontHeading())
			.ColorAndOpacity(FMLPTheme::Foreground())
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadH())
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("R","刷新")).ToolTipText(LOCTEXT("RT","重新扫描参数")).OnClicked(this,&SMaterialLayoutProPanel::OnRefreshClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("AG","自动分组")).ToolTipText(LOCTEXT("AGT","按名称前缀自动分组")).OnClicked(this,&SMaterialLayoutProPanel::OnAutoGroupClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("GBC","按注释")).ToolTipText(LOCTEXT("GBCT","按注释框分组")).OnClicked(this,&SMaterialLayoutProPanel::OnGroupByCommentClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("AU","归档未用")).ToolTipText(LOCTEXT("AUT","将未使用的参数移至已废弃分组")).OnClicked(this,&SMaterialLayoutProPanel::OnArchiveUnusedClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton")
			.ButtonColorAndOpacity(FMLPTheme::ButtonDanger()).ForegroundColor(FMLPTheme::ButtonTextOnColor())
			.ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("DU","删除未用")).ToolTipText(LOCTEXT("DUT","删除未使用的参数")).OnClicked(this,&SMaterialLayoutProPanel::OnDeleteUnusedClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("SW","排序")).ToolTipText(LOCTEXT("SWT","排序工作台")).OnClicked(this,&SMaterialLayoutProPanel::OnSortWorkbenchClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("PE","编辑器")).ToolTipText(LOCTEXT("PET","参数编辑器")).OnClicked(this,&SMaterialLayoutProPanel::OnParameterEditorClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("EX","导出")).ToolTipText(LOCTEXT("EXT","导出 CSV")).OnClicked(this,&SMaterialLayoutProPanel::OnExportClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("IM","导入")).ToolTipText(LOCTEXT("IMT","导入 CSV")).OnClicked(this,&SMaterialLayoutProPanel::OnImportClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		// Set-group for multi-selection: input box + apply button.
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0,0,2,0))
		[
			SAssignNew(SetGroupInput, SEditableTextBox)
			.HintText(LOCTEXT("SetGroupHint", "设分组..."))
			.Font(FMLPTheme::FontSmall())
			.MinDesiredWidth(80.f)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("SG","设分组")).ToolTipText(LOCTEXT("SGT","将选中的参数分配到输入的分组"))
			.OnClicked(this, &SMaterialLayoutProPanel::OnSetGroupForSelectionClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(2,2)).VAlign(VAlign_Center)[FMLPTheme::MakeSeparator()]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton")
			.ButtonColorAndOpacity(FMLPTheme::ButtonPrimary()).ForegroundColor(FMLPTheme::ButtonTextOnColor())
			.ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("Apply","应用")).ToolTipText(LOCTEXT("ApplyT","将改动写回材质")).OnClicked(this,&SMaterialLayoutProPanel::OnApplyChangesClicked)
		]
		;
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildStatusBar()
{
	return SNew(STextBlock)
		.Text(this, &SMaterialLayoutProPanel::GetStatusText)
		.Font(FMLPTheme::FontSmall())
		.ColorAndOpacity(FMLPTheme::Muted());
}

// ============================================================================
// Toolbar handlers
// ============================================================================

FReply SMaterialLayoutProPanel::OnRefreshClicked() { RefreshParameters(); return FReply::Handled(); }

FReply SMaterialLayoutProPanel::OnApplyChangesClicked()
{
	if (Session.IsValid()) { Session->PushDirty(); RefreshParameters(); }
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnSetGroupForSelectionClicked()
{
	if (SelectedParams.Num() == 0 || !Session.IsValid() || !TargetMaterial.IsValid())
		return FReply::Handled();

	FName NewGroup = SetGroupInput.IsValid() ? FName(*SetGroupInput->GetText().ToString()) : NAME_None;

	const FScopedTransaction Transaction(FText::FromString(TEXT("批量设分组")));
	UMaterial* M = TargetMaterial.Get();
	M->Modify();

	for (const TSharedPtr<FMLPParamVM>& Param : SelectedParams)
	{
		if (!Param.IsValid() || !Param->SourceExpression.IsValid()) continue;
		if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Param->SourceExpression.Get()))
		{
			ParamExpr->Modify();
			ParamExpr->Group = NewGroup;
			Param->Group = NewGroup;
			Param->bDirty = true;
		}
	}

	M->PostEditChange();
	M->MarkPackageDirty();
	if (SetGroupInput.IsValid()) SetGroupInput->SetText(FText::GetEmpty());
	RefreshParameters();
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
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnArchiveUnusedClicked: TargetMaterial valid=%d"), TargetMaterial.IsValid());
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const auto* S = GetDefault<UMaterialLayoutProSettings>();
	const FName Dep(S ? *S->DeprecatedGroupName : TEXT("Deprecated"));
	const FScopedTransaction T(LOCTEXT("AU","归档未使用的参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	FMaterialParameterUsageAnalyzer::Analyze(M, Params);
	int32 Archived = 0;
	for (auto& P : Params) if (P->Usage == EMLPParameterUsage::Unused)
		if (auto* E = Cast<UMaterialExpressionParameter>(P->Expression.Get())) { E->Modify(); E->Group = Dep; ++Archived; }
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnArchiveUnused: scanned=%d archived=%d -> %s"), Params.Num(), Archived, *Dep.ToString());
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnDeleteUnusedClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnDeleteUnusedClicked: TargetMaterial valid=%d"), TargetMaterial.IsValid());
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const FScopedTransaction T(LOCTEXT("DU","删除未使用的参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	FMaterialParameterUsageAnalyzer::Analyze(M, Params);
	int32 Deleted = 0;
	for (auto& P : Params) if (P->Usage == EMLPParameterUsage::Unused && P->Expression.IsValid())
	{
#if ENGINE_MAJOR_VERSION >= 5
		M->GetExpressionCollection().RemoveExpression(P->Expression.Get());
#else
		M->Expressions.Remove(P->Expression.Get());
#endif
		++Deleted;
	}
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnDeleteUnused: scanned=%d deleted=%d"), Params.Num(), Deleted);
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnAutoGroupClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnAutoGroupClicked: TargetMaterial valid=%d"), TargetMaterial.IsValid());
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const auto* S = GetDefault<UMaterialLayoutProSettings>();
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnAutoGroup: Settings=%p rules=%d"), S, S ? S->AutoGroupRules.Num() : -1);
	if (!S) return FReply::Handled();
	const FScopedTransaction T(LOCTEXT("AG","自动分组参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	int32 Grouped = 0;
	for (auto& P : Params) { if (!P.IsValid()) continue; const FString N = P->Name.ToString();
		for (const auto& R : S->AutoGroupRules) if (N.StartsWith(R.Prefix)) {
			if (auto* E = Cast<UMaterialExpressionParameter>(P->Expression.Get())) { E->Modify(); E->Group = FName(*R.Group); ++Grouped; } break; } }
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnAutoGroup: scanned=%d grouped=%d"), Params.Num(), Grouped);
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnGroupByCommentClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnGroupByCommentClicked: TargetMaterial valid=%d"), TargetMaterial.IsValid());
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
	UE_LOG(LogTemp, Warning, TEXT("[MLP] OnBulkRenameClicked: TargetMaterial valid=%d"), TargetMaterial.IsValid());
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
	return LOCTEXT("NT","未绑定材质");
}

FText SMaterialLayoutProPanel::GetStatusText() const
{
	if (!TargetMaterial.IsValid() && !TargetMaterialInstance.IsValid()) return LOCTEXT("NS","未选择材质");
	if (!Session.IsValid() || Session->Groups.Num() == 0)
	{
		// Diagnostic: distinguish scanner-found-0 from VM-built-0.
		int32 Scanned = 0;
		if (TargetMaterial.IsValid())
		{
			TArray<TSharedPtr<FMLPParameterInfo>> Params = FMaterialParameterScanner::ScanMaterial(TargetMaterial.Get());
			Scanned = Params.Num();
		}
		return FText::FromString(FString::Printf(TEXT("[diag] 扫描=%d 组=%d 锁=%d"), Scanned, Session.IsValid() ? Session->Groups.Num() : -1, Session.IsValid() ? Session->InteractingCount : -1));
	}

	int32 Total = 0;
	for (const auto& G : Session->Groups) if (G.IsValid()) Total += G->Parameters.Num();
	if (Total == 0) return LOCTEXT("NP2","未找到参数");

	FString Base = FString::Printf(TEXT("%d 参数 | %d 分组"), Total, Session->Groups.Num());
	if (Session->HasDirty()) Base += TEXT("  ● 未提交");
	return FText::FromString(Base);
}

#undef LOCTEXT_NAMESPACE
