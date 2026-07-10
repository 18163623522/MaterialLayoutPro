#include "Widgets/SMaterialLayoutProPanel.h"
#include "MaterialLayoutProTheme.h"
#include "MaterialLayoutProSettings.h"
#include "Model/MaterialLayoutViewModel.h"
#include "Model/MaterialParameterScanner.h"
#include "Model/MaterialParameterInfo.h"
#include "Widgets/SMaterialParameterRow.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionComment.h"
#include "StaticParameterSet.h"
#include "Engine/Texture.h"
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
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Colors/SColorPicker.h"
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
		// Group header: colored bar + [sort number] [name (count)], larger bold font.
		TreeContainer->AddSlot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 2.f))
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(FMLPTheme::Accent().R, FMLPTheme::Accent().G, FMLPTheme::Accent().B, 0.18f))
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(FMargin(4.f, 3.f, 4.f, 3.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(36.f)
					[
						SNew(SNumericEntryBox<int32>)
						.Value(TOptional<int32>(Group->SortPriority))
						.Font(FMLPTheme::FontBody())
						.MinDesiredValueWidth(24.f)
						.AllowSpin(false)
						.ToolTipText(LOCTEXT("GroupSortTT", "排序号 (小的在前)"))
						.OnValueCommitted_Lambda([this, Group](int32 NewValue, ETextCommit::Type)
						{
							if (Group.IsValid() && NewValue != Group->SortPriority)
							{
								Group->SortPriority = NewValue;
								Session->Groups.Sort([](const TSharedPtr<FMLPGroupVM>& A, const TSharedPtr<FMLPGroupVM>& B)
								{
									if (A->SortPriority != B->SortPriority) return A->SortPriority < B->SortPriority;
									return A->Name.ToString() < B->Name.ToString();
								});
								RebuildTree();
							}
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s  (%d)"), *Group->DisplayName, VisibleCount)))
					.Font(FMLPTheme::FontHeading())
					.ColorAndOpacity(FMLPTheme::Foreground())
				]
			]
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
				.OnDoubleClicked(FOnRowDoubleClicked::CreateSP(SharedThis(this), &SMaterialLayoutProPanel::JumpToParam))
				.OnParamDropped(FOnParamDropped::CreateSP(SharedThis(this), &SMaterialLayoutProPanel::OnParamDropped))
				.IsSelectedQuery(FIsParamSelected::CreateSP(SharedThis(this), &SMaterialLayoutProPanel::IsSelected))
			];
		}
	}
}

// ============================================================================
// Selection + search
// ============================================================================

void SMaterialLayoutProPanel::SelectParam(TSharedPtr<FMLPParamVM> Param, bool bCtrl, bool bShift)
{
	if (!Param.IsValid())
	{
		if (SelectedParams.Num() > 0)
		{
			ClearSelection();
		}
		return;
	}

	// Update selection state only - no graph sync, no RebuildTree.

	if (bCtrl)
	{
		if (IsSelected(Param)) SelectedParams.Remove(Param);
		else SelectedParams.Add(Param);
	}
	else if (bShift && LastSelectedParam.IsValid())
	{
		// Range select: flatten all params into a linear list, find LastSelectedParam
		// and Param, select everything between them (inclusive).
		TArray<TSharedPtr<FMLPParamVM>> AllParams;
		for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
		{
			for (const TSharedPtr<FMLPParamVM>& P : Group->Parameters)
			{
				AllParams.Add(P);
			}
		}
		int32 StartIdx = AllParams.IndexOfByKey(LastSelectedParam);
		int32 EndIdx = AllParams.IndexOfByKey(Param);
		if (StartIdx != INDEX_NONE && EndIdx != INDEX_NONE)
		{
			if (StartIdx > EndIdx) Swap(StartIdx, EndIdx);
			for (int32 i = StartIdx; i <= EndIdx; ++i)
			{
				if (!IsSelected(AllParams[i])) SelectedParams.Add(AllParams[i]);
			}
		}
		else
		{
			// Fallback: just select the clicked param if anchor not found.
			if (!IsSelected(Param)) SelectedParams.Add(Param);
		}
	}
	else
	{
		// Single select - replace.
		SelectedParams.Reset();
		SelectedParams.Add(Param);
	}
	LastSelectedParam = Param;

	// Do NOT call AddToSelection here - it causes the material editor to grab focus,
	// which kills focus on the panel's text boxes. Graph selection sync is handled
	// by JumpToParam (double-click) only.
}

void SMaterialLayoutProPanel::JumpToParam(TSharedPtr<FMLPParamVM> Param)
{
	if (!Param.IsValid() || !Param->SourceExpression.IsValid() || !OwningMaterialEditor.IsValid())
		return;
	TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
	if (Editor.IsValid())
	{
		SyncCooldownUntil = FSlateApplication::Get().GetCurrentTime() + 0.5;
		Editor->JumpToExpression(Param->SourceExpression.Get());
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

// ============================================================================
// Drag-drop reorder
// ============================================================================

void SMaterialLayoutProPanel::OnParamDropped(TSharedPtr<FMLPParamVM> DraggedParam, TSharedPtr<FMLPParamVM> TargetParam, bool bInsertBefore)
{
	if (!DraggedParam.IsValid() || !TargetParam.IsValid() || !Session.IsValid() || !TargetMaterial.IsValid())
		return;
	if (DraggedParam == TargetParam) return;

	// Find the dragged param's source group and the target param's group.
	TSharedPtr<FMLPGroupVM> SourceGroup;
	TSharedPtr<FMLPGroupVM> TargetGroup;
	int32 DraggedIndex = INDEX_NONE;

	for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
	{
		int32 Idx = Group->Parameters.IndexOfByPredicate([&](const TSharedPtr<FMLPParamVM>& P) { return P == DraggedParam; });
		if (Idx != INDEX_NONE)
		{
			SourceGroup = Group;
			DraggedIndex = Idx;
		}
		if (Group->Parameters.ContainsByPredicate([&](const TSharedPtr<FMLPParamVM>& P) { return P == TargetParam; }))
		{
			TargetGroup = Group;
		}
	}

	if (!SourceGroup.IsValid() || !TargetGroup.IsValid()) return;

	// Single transaction for undo.
	const FScopedTransaction Transaction(FText::FromString(TEXT("拖拽重排序参数")));
	UMaterial* M = TargetMaterial.Get();
	M->Modify();

	// 1. Remove from source group.
	SourceGroup->Parameters.RemoveAt(DraggedIndex);

	// 2. Find insert index in target group.
	int32 TargetIndex = TargetGroup->Parameters.IndexOfByPredicate([&](const TSharedPtr<FMLPParamVM>& P) { return P == TargetParam; });
	if (TargetIndex == INDEX_NONE) return;

	// If moving within the same group and the dragged item was before the target,
	// the removal shifted indices - but we already removed, so TargetIndex is correct
	// for the post-removal array. Adjust for insert position:
	if (!bInsertBefore) ++TargetIndex;

	// 3. Insert at target position.
	TargetGroup->Parameters.Insert(DraggedParam, FMath::Clamp(TargetIndex, 0, TargetGroup->Parameters.Num()));

	// 4. Update Group if cross-group move.
	if (SourceGroup != TargetGroup)
	{
		DraggedParam->Group = TargetGroup->Name;
	}

	// 5. Recompute SortPriority for the target group (0, 1, 2, ...).
	for (int32 i = 0; i < TargetGroup->Parameters.Num(); ++i)
	{
		TSharedPtr<FMLPParamVM>& Param = TargetGroup->Parameters[i];
		if (Param.IsValid())
		{
			Param->SortPriority = i;
			Param->bDirty = true;
			Param->PushToExpression();
		}
	}

	// If source group is different and still has params, recompute their priorities too.
	if (SourceGroup != TargetGroup)
	{
		for (int32 i = 0; i < SourceGroup->Parameters.Num(); ++i)
		{
			TSharedPtr<FMLPParamVM>& Param = SourceGroup->Parameters[i];
			if (Param.IsValid())
			{
				Param->SortPriority = i;
				Param->bDirty = true;
				Param->PushToExpression();
			}
		}
	}

	M->PostEditChange();
	M->MarkPackageDirty();
	RebuildTree();
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

	// Snapshot selected param names + group sort priorities before refresh.
	// PullAll creates new VM objects, so we need to rebind by name match.
	TArray<FName> SelectedNames;
	for (const TSharedPtr<FMLPParamVM>& Sel : SelectedParams)
	{
		if (Sel.IsValid()) SelectedNames.Add(Sel->Name);
	}
	TMap<FName, int32> OldGroupSortPriorities;
	for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
	{
		if (Group.IsValid() && Group->SortPriority != 0)
			OldGroupSortPriorities.Add(Group->Name, Group->SortPriority);
	}

	// In embedded mode, re-resolve the target (editor may have switched assets).
	if (OwningMaterialEditor.IsValid())
	{
		ResolveTargetMaterial();
	}

	if (TargetMaterial.IsValid())
	{
		Session->TargetMaterial = TargetMaterial;
		Session->PullAll();

		// Restore group sort priorities.
		for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
		{
			if (Group.IsValid())
			{
				const int32* OldPrio = OldGroupSortPriorities.Find(Group->Name);
				if (OldPrio) Group->SortPriority = *OldPrio;
			}
		}
		// Re-sort groups with restored priorities.
		Session->Groups.Sort([](const TSharedPtr<FMLPGroupVM>& A, const TSharedPtr<FMLPGroupVM>& B)
		{
			if (A->SortPriority != B->SortPriority) return A->SortPriority < B->SortPriority;
			return A->Name.ToString() < B->Name.ToString();
		});

		// Rebind SelectedParams to the new VMs by name match.
		TArray<TSharedPtr<FMLPParamVM>> NewSelection;
		FName LastName = LastSelectedParam.IsValid() ? LastSelectedParam->Name : NAME_None;
		TSharedPtr<FMLPParamVM> NewLastSelected;
		for (const FName& SelName : SelectedNames)
		{
			for (const TSharedPtr<FMLPGroupVM>& Group : Session->Groups)
			{
				for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
				{
					if (Param.IsValid() && Param->Name == SelName)
					{
						NewSelection.Add(Param);
						if (Param->Name == LastName) NewLastSelected = Param;
						break;
					}
				}
			}
		}
		SelectedParams = MoveTemp(NewSelection);
		LastSelectedParam = NewLastSelected;
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
		// Instance group panel button - only visible when editing a material instance.
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SBox)
			.Visibility(TAttribute<EVisibility>::Create([this]() -> EVisibility {
				return TargetMaterialInstance.IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
			}))
			[
				SNew(SButton).ButtonStyle(MLP_STYLE::Get(),"FlatButton").ContentPadding(FMLPTheme::PadBtn())
				.ButtonColorAndOpacity(FMLPTheme::ButtonPrimary()).ForegroundColor(FMLPTheme::ButtonTextOnColor())
				.Text(LOCTEXT("IGP","实例分组")).ToolTipText(LOCTEXT("IGPT","打开材质实例参数分组面板"))
				.OnClicked(this, &SMaterialLayoutProPanel::OnInstanceGroupClicked)
			]
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

FReply SMaterialLayoutProPanel::OnArchiveUnusedClicked()
{
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
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnAutoGroupClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	const auto* S = GetDefault<UMaterialLayoutProSettings>();
	if (!S) return FReply::Handled();
	const FScopedTransaction T(LOCTEXT("AG","自动分组参数"));
	auto* M = TargetMaterial.Get(); M->Modify();
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
	int32 Grouped = 0;
	for (auto& P : Params)
	{
		if (!P.IsValid()) continue;
		const FString N = P->Name.ToString();
		bool bMatched = false;

		// 1. Try user-defined prefix rules first (highest priority).
		for (const auto& R : S->AutoGroupRules)
		{
			if (N.StartsWith(R.Prefix))
			{
				if (auto* E = Cast<UMaterialExpressionParameter>(P->Expression.Get()))
				{ E->Modify(); E->Group = FName(*R.Group); ++Grouped; }
				bMatched = true;
				break;
			}
		}
		if (bMatched) continue;

		// 2. Fallback: extract group name from the first underscore segment.
		//    e.g. "basecolor_strength" -> group "basecolor"
		//    e.g. "basecolor_texture"  -> group "basecolor"
		int32 UnderScoreIdx;
		if (N.FindChar('_', UnderScoreIdx) && UnderScoreIdx > 0)
		{
			FString GroupName = N.Left(UnderScoreIdx);
			GroupName.TrimStartAndEndInline();
			if (!GroupName.IsEmpty())
			{
				if (auto* E = Cast<UMaterialExpressionParameter>(P->Expression.Get()))
				{ E->Modify(); E->Group = FName(*GroupName); ++Grouped; }
			}
		}
	}
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnGroupByCommentClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto* M = TargetMaterial.Get(); const FScopedTransaction T(LOCTEXT("GBC","按注释分组参数"));
	M->Modify();
	// Scan once - reuse for all comment boxes.
	auto Params = FMaterialParameterScanner::ScanMaterial(M);
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
		for (auto& P : Params) { if (!P.IsValid()||!P->Expression.IsValid()) continue;
			const FVector2D Pos(P->Expression->MaterialExpressionEditorX,P->Expression->MaterialExpressionEditorY);
			if (Pos.X>=Min.X&&Pos.X<=Max.X&&Pos.Y>=Min.Y&&Pos.Y<=Max.Y)
				if (auto* E2 = Cast<UMaterialExpressionParameter>(P->Expression.Get())) { E2->Modify(); E2->Group = GN; } }
	}
	M->PostEditChange(); M->MarkPackageDirty(); RefreshParameters();
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
	if (!Session.IsValid() || Session->Groups.Num() == 0) return LOCTEXT("NP2","未找到参数");

	int32 Total = 0;
	for (const auto& G : Session->Groups) if (G.IsValid()) Total += G->Parameters.Num();
	if (Total == 0) return LOCTEXT("NP2","未找到参数");

	FString Base = FString::Printf(TEXT("%d 参数 | %d 分组"), Total, Session->Groups.Num());
	if (Session->HasDirty()) Base += TEXT("  ● 未提交");
	return FText::FromString(Base);
}

// ============================================================================
// Instance mode - tabbed group panel for Material Instance editors
// ============================================================================

void SMaterialLayoutProPanel::PullFromInstance()
{
	InstanceParams.Reset();
	InstanceTabNames.Reset();

	UMaterialInstance* MI = TargetMaterialInstance.Get();
	UMaterial* BaseMat = TargetMaterial.Get();
	if (!MI || !BaseMat) return;

	// Scan parent material for parameter list + groups.
	auto ScannedParams = FMaterialParameterScanner::ScanMaterial(BaseMat);

	for (const auto& P : ScannedParams)
	{
		if (!P.IsValid() || !P->Expression.IsValid()) continue;

		TSharedPtr<FMLPInstanceParamVM> VM = MakeShared<FMLPInstanceParamVM>();
		VM->Name = P->Name;
		VM->Group = P->Group.IsNone() ? FName(TEXT("(None)")) : P->Group;
		VM->ExpressionGUID = P->Guid;
		VM->Type = (int32)P->Type;

		// Check if this parameter has an override on the instance.
		FHashedMaterialParameterInfo ParamInfo(VM->Name);
		if (P->Type == EMLPParameterType::Scalar)
		{
			float OutVal;
			VM->bOverridden = MI->GetScalarParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->ScalarValue = OutVal;
			else
			{
				if (auto* ScalarExpr = Cast<UMaterialExpressionScalarParameter>(P->Expression.Get()))
					VM->ScalarValue = ScalarExpr->DefaultValue;
			}
		}
		else if (P->Type == EMLPParameterType::Vector)
		{
			FLinearColor OutVal;
			VM->bOverridden = MI->GetVectorParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->VectorValue = OutVal;
			else
			{
				if (auto* VecExpr = Cast<UMaterialExpressionVectorParameter>(P->Expression.Get()))
					VM->VectorValue = VecExpr->DefaultValue;
			}
		}
		else if (P->Type == EMLPParameterType::Texture)
		{
			UTexture* OutVal;
			VM->bOverridden = MI->GetTextureParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->TextureValue = OutVal;
			else
			{
				if (auto* TexExpr = Cast<UMaterialExpressionTextureSampleParameter>(P->Expression.Get()))
					VM->TextureValue = TexExpr->Texture;
				else if (auto* TexObjExpr = Cast<UMaterialExpressionTextureObjectParameter>(P->Expression.Get()))
					VM->TextureValue = TexObjExpr->Texture;
			}
		}
		else if (P->Type == EMLPParameterType::StaticBool || P->Type == EMLPParameterType::StaticSwitch)
		{
			bool bOutVal;
			FGuid OutGuid;
			VM->bOverridden = MI->GetStaticSwitchParameterValue(ParamInfo, bOutVal, OutGuid, true);
			if (VM->bOverridden) VM->BoolValue = bOutVal;
			else
			{
				if (auto* BoolExpr = Cast<UMaterialExpressionStaticBoolParameter>(P->Expression.Get()))
					VM->BoolValue = BoolExpr->DefaultValue;
			}
		}

		InstanceParams.Add(VM);

		// Collect unique group names.
		if (!InstanceTabNames.Contains(VM->Group))
			InstanceTabNames.Add(VM->Group);
	}

	// Sort tab names alphabetically (None group last).
	InstanceTabNames.Sort([](const FName& A, const FName& B)
	{
		if (A == TEXT("(None)")) return false;
		if (B == TEXT("(None)")) return true;
		return A.ToString() < B.ToString();
	});

	// Default to first tab.
	if (!InstanceTabNames.Contains(CurrentTab))
	{
		CurrentTab = InstanceTabNames.Num() > 0 ? InstanceTabNames[0] : NAME_None;
	}
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildInstanceContent()
{
	// Build a fresh top-level container that holds (1) the tab bar and (2) a scrollable
	// list of parameter rows for the current tab. RebuildInstanceContent() will later
	// clear + repopulate THIS container when state changes (tab switch / value edit).
	TSharedPtr<SVerticalBox> ContentBox = SNew(SVerticalBox);
	InstanceContentContainer = ContentBox;

	RebuildInstanceContent();

	return ContentBox.ToSharedRef();
}

void SMaterialLayoutProPanel::RebuildInstanceContent()
{
	if (!InstanceContentContainer.IsValid()) return;

	InstanceContentContainer->ClearChildren();

	// --- Tab bar row ---
	InstanceContentContainer->AddSlot().AutoHeight().Padding(FMargin(0, 2, 0, 4))
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::Border().R, FMLPTheme::Border().G, FMLPTheme::Border().B, 0.5f))
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.Padding(FMargin(2, 1))
		[
			BuildInstanceTabBar()
		]
	];

	// --- Parameter rows for the current tab (inside a scroll box) ---
	TSharedRef<SScrollBox> Scroller = SNew(SScrollBox);
	BuildInstanceRows(Scroller);

	InstanceContentContainer->AddSlot().FillHeight(1.0f)
	[
		Scroller
	];
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildInstanceTabBar()
{
	// The tab bar lives in the standalone instance SWindow which can outlive this panel —
	// capture a weak ptr, never raw `this`.
	TWeakPtr<SMaterialLayoutProPanel, ESPMode::NotThreadSafe> WeakPanel = StaticCastSharedRef<SMaterialLayoutProPanel>(AsShared());

	TSharedRef<SHorizontalBox> TabBar = SNew(SHorizontalBox);

	for (const FName& TabName : InstanceTabNames)
	{
		const bool bActive = (TabName == CurrentTab);
		TabBar->AddSlot().AutoWidth().Padding(FMargin(1, 0))
		[
			SNew(SButton)
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ButtonColorAndOpacity(bActive ? FMLPTheme::AccentBg() : FLinearColor::Transparent)
			.ForegroundColor(bActive ? FMLPTheme::Accent() : FMLPTheme::Muted())
			.ContentPadding(FMargin(8, 2))
			.OnClicked_Lambda([WeakPanel, TabName]() -> FReply {
				if (auto Panel = WeakPanel.Pin()) return Panel->OnTabClicked(TabName);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromName(TabName))
				.Font(FMLPTheme::FontBody())
			]
		];
	}

	// [+] button to add a new group.
	TabBar->AddSlot().AutoWidth().Padding(FMargin(2, 0))
	[
		SNew(SButton)
		.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
		.ContentPadding(FMargin(6, 2))
		.ToolTipText(LOCTEXT("AddTabTT", "新建分组"))
		.OnClicked_Lambda([WeakPanel]() -> FReply {
			if (auto Panel = WeakPanel.Pin()) return Panel->OnAddTabClicked();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("+")))
			.Font(FMLPTheme::FontBody())
		]
	];

	return TabBar;
}

void SMaterialLayoutProPanel::BuildInstanceRows(TSharedRef<SScrollBox> ContentBox)
{
	// The instance content lives inside a standalone SWindow that can outlive this panel
	// (e.g. the embedded tab is closed while the window stays open). All row lambdas must
	// therefore capture a weak ptr to the panel, never raw `this`, to avoid use-after-free.
	TWeakPtr<SMaterialLayoutProPanel, ESPMode::NotThreadSafe> WeakPanel = StaticCastSharedRef<SMaterialLayoutProPanel>(AsShared());

	int32 VisibleCount = 0;
	for (const auto& P : InstanceParams)
	{
		if (P->Group != CurrentTab) continue;
		++VisibleCount;

		TWeakPtr<FMLPInstanceParamVM> WeakVM = P;

		// --- Value editor built per type ---
		// Each editor writes back through OnInstanceScalarChanged / OnInstanceVectorChanged /
		// OnInstanceTextureChanged / OnInstanceBoolChanged, which update the instance's
		// override array AND call RebuildInstanceContent() so the row reflects the new value.
		TSharedRef<SWidget> ValueEditor = [WeakPanel, WeakVM]() -> TSharedRef<SWidget>
		{
			auto V = WeakVM.Pin();
			if (!V.IsValid()) return SNew(STextBlock).Text(FText::GetEmpty());

			const bool bEditable = V->bOverridden;

			switch (V->Type)
			{
			case (int32)EMLPParameterType::Scalar:
			{
				// SNumericEntryBox is read-only when not overridden (Value_TAttribute returns unset),
				// editable when overridden. Matches the UE details-panel scalar row.
				auto WeakV2 = WeakVM;
				return SNew(SNumericEntryBox<float>)
					.Value_Lambda([WeakV2]() -> TOptional<float> {
						auto V2 = WeakV2.Pin();
						if (!V2.IsValid() || !V2->bOverridden) return TOptional<float>();
						return TOptional<float>(V2->ScalarValue);
					})
					.Font(FMLPTheme::FontSmall())
					.AllowSpin(bEditable)
					.MinValue(TOptional<float>()).MaxValue(TOptional<float>())
					.MinSliderValue(TOptional<float>()).MaxSliderValue(TOptional<float>())
					.MinDesiredValueWidth(80.f)
					.IsEnabled(bEditable)
					// Live update during spinner drag — just updates the VM value without
					// rebuilding (rebuilding would steal focus from the spinner).
					.OnValueChanged_Lambda([WeakV2](float NewVal) {
						auto V2 = WeakV2.Pin();
						if (V2.IsValid()) V2->ScalarValue = NewVal;
					})
					// Commit (Enter / focus loss) — write through to the instance + rebuild.
					.OnValueCommitted_Lambda([WeakPanel, WeakV2](float NewVal, ETextCommit::Type) {
						auto Panel = WeakPanel.Pin();
						auto V2 = WeakV2.Pin();
						if (Panel.IsValid() && V2.IsValid())
						{
							Panel->OnInstanceScalarChanged(V2, NewVal, ETextCommit::Default);
						}
					});
			}
			case (int32)EMLPParameterType::Vector:
			{
				auto WeakV2 = WeakVM;
				// Color swatch (click opens picker) + RGBA text. Disabled when not overridden.
				return SNew(SHorizontalBox)
					.IsEnabled(bEditable)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					[
						SNew(SBox).WidthOverride(28.f).HeightOverride(16.f)
						[
							SNew(SBorder)
							.BorderBackgroundColor_Lambda([WeakV2]() -> FLinearColor {
								auto V2 = WeakV2.Pin();
								if (!V2.IsValid()) return FLinearColor::Transparent;
								FLinearColor C = V2->VectorValue;
								if (C.A <= 0.001f) C.A = 1.0f; // UE4.26 VectorParam DefaultValue often A=0
								return C;
							})
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.OnMouseButtonDown_Lambda([WeakPanel, WeakV2](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply {
								if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
								auto V2 = WeakV2.Pin();
								if (!V2.IsValid()) return FReply::Unhandled();
								FColorPickerArgs Args;
								Args.bUseAlpha = true;
							#if ENGINE_MAJOR_VERSION >= 5
								Args.InitialColor = V2->VectorValue;
							#else
								Args.InitialColorOverride = V2->VectorValue;
							#endif
								TWeakPtr<FMLPInstanceParamVM, ESPMode::NotThreadSafe> WeakParam = V2;
								Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([WeakPanel, WeakParam](FLinearColor NewColor) {
									auto Panel = WeakPanel.Pin();
									auto Param = WeakParam.Pin();
									if (Panel.IsValid() && Param.IsValid())
									{
										Panel->OnInstanceVectorChanged(Param, NewColor);
									}
								});
								OpenColorPicker(Args);
								return FReply::Handled();
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([WeakV2]() -> FText {
							auto V2 = WeakV2.Pin();
							if (!V2.IsValid()) return FText::GetEmpty();
							const FLinearColor& C = V2->VectorValue;
							return FText::FromString(FString::Printf(TEXT("R:%.2f G:%.2f B:%.2f A:%.2f"), C.R, C.G, C.B, C.A));
						})
						.Font(FMLPTheme::FontSmall())
						.ColorAndOpacity(FMLPTheme::Muted())
					];
			}
			case (int32)EMLPParameterType::Texture:
			{
				// Click button -> Content Browser asset picker. Disabled when not overridden.
				return SNew(SButton)
					.IsEnabled(bEditable)
					.Text_Lambda([WeakVM]() -> FText {
						auto V = WeakVM.Pin();
						if (V.IsValid() && V->TextureValue.IsValid())
							return FText::FromString(V->TextureValue->GetName());
						return FText::FromString(TEXT("(无)"));
					})
					.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
					.ContentPadding(FMargin(2.f, 0.f))
					.HAlign(HAlign_Left)
					.OnClicked_Lambda([WeakPanel, WeakVM]() -> FReply {
						auto Panel = WeakPanel.Pin();
						if (!Panel.IsValid()) return FReply::Handled();
						FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						FAssetPickerConfig Config;
					#if ENGINE_MAJOR_VERSION >= 5
						Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
					#else
						Config.Filter.ClassNames.Add(TEXT("Texture2D"));
					#endif
						auto WeakV2 = WeakVM;
						TWeakPtr<SMaterialLayoutProPanel, ESPMode::NotThreadSafe> WeakPanel2 = WeakPanel;
						Config.OnAssetSelected = FOnAssetSelected::CreateLambda([WeakPanel2, WeakV2](const FAssetData& AssetData) -> void {
							auto Panel2 = WeakPanel2.Pin();
							auto V2 = WeakV2.Pin();
							if (!Panel2.IsValid() || !V2.IsValid() || !AssetData.IsValid()) { FSlateApplication::Get().DismissAllMenus(); return; }
							UObject* Asset = AssetData.GetAsset();
							if (!Asset) { const FString Path = AssetData.PackageName.ToString() / AssetData.AssetName.ToString(); Asset = LoadObject<UObject>(nullptr, *Path); }
							if (Asset) Panel2->OnInstanceTextureChanged(V2, Asset);
							FSlateApplication::Get().DismissAllMenus();
						});
						Config.bAllowNullSelection = false;
						Config.InitialAssetViewType = EAssetViewType::List;
						TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
						FSlateApplication::Get().PushMenu(Panel->AsShared(), FWidgetPath(), Picker, FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
						return FReply::Handled();
					});
			}
			case (int32)EMLPParameterType::StaticBool:
			case (int32)EMLPParameterType::StaticSwitch:
			{
				// Checkbox toggles bool value; disabled when not overridden.
				auto WeakV2 = WeakVM;
				return SNew(SCheckBox)
					.IsEnabled(bEditable)
					.IsChecked_Lambda([WeakV2]() -> ECheckBoxState {
						auto V2 = WeakV2.Pin();
						if (!V2.IsValid()) return ECheckBoxState::Unchecked;
						return V2->BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([WeakPanel, WeakV2](ECheckBoxState State) {
						auto Panel = WeakPanel.Pin();
						auto V2 = WeakV2.Pin();
						if (Panel.IsValid() && V2.IsValid())
						{
							Panel->OnInstanceBoolChanged(V2, State == ECheckBoxState::Checked);
						}
					});
			}
			default:
				return SNew(STextBlock)
					.Text(FText::FromString(TEXT("(不支持)")))
					.Font(FMLPTheme::FontSmall())
					.ColorAndOpacity(FMLPTheme::Muted());
			}
		}();

		ContentBox->AddSlot().Padding(FMargin(2, 1))
		[
			SNew(SHorizontalBox)
			// Override checkbox
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([WeakVM]() -> ECheckBoxState {
					auto V = WeakVM.Pin();
					return (V.IsValid() && V->bOverridden) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([WeakPanel, WeakVM](ECheckBoxState State) {
					auto Panel = WeakPanel.Pin();
					auto V = WeakVM.Pin();
					if (Panel.IsValid() && V.IsValid()) Panel->OnToggleOverride(V);
				})
				.ToolTipText(LOCTEXT("OverrideTT", "勾选=覆盖实例值"))
			]
			// Parameter name
			+ SHorizontalBox::Slot().FillWidth(0.32f).VAlign(VAlign_Center).Padding(FMargin(4, 0))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakVM]() -> FText {
					auto V = WeakVM.Pin();
					return V.IsValid() ? FText::FromName(V->Name) : FText::GetEmpty();
				})
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity_Lambda([WeakVM]() -> FSlateColor {
					auto V = WeakVM.Pin();
					if (!V.IsValid()) return FMLPTheme::Muted();
					return V->bOverridden ? FMLPTheme::Foreground() : FMLPTheme::Muted();
				})
			]
			// Value editor (type-matched)
			+ SHorizontalBox::Slot().FillWidth(0.58f).VAlign(VAlign_Center)
			[
				ValueEditor
			]
			// Override indicator dot
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4, 0))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakVM]() -> FText {
					auto V = WeakVM.Pin();
					return (V.IsValid() && V->bOverridden) ? FText::FromString(TEXT("●")) : FText::GetEmpty();
				})
				.ColorAndOpacity(FMLPTheme::Accent())
				.Font(FMLPTheme::FontSmall())
			]
		];
	}

	if (VisibleCount == 0)
	{
		ContentBox->AddSlot().Padding(FMargin(4, 8))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTabParams", "该分组没有参数"))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
		];
	}
}

FReply SMaterialLayoutProPanel::OnTabClicked(FName GroupName)
{
	CurrentTab = GroupName;
	RebuildInstanceContent();
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnAddTabClicked()
{
	// Simple: open a dialog to input group name, then move selected params to it.
	// For now, just set all non-grouped params to a new group.
	if (!TargetMaterial.IsValid()) return FReply::Handled();

	// Use the search box text as group name if available, else auto-name.
	FString NewName = FString::Printf(TEXT("Group_%d"), InstanceTabNames.Num());

	auto* M = TargetMaterial.Get();
	const FScopedTransaction T(FText::FromString(TEXT("新建分组")));
	M->Modify();
#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* E : M->GetExpressions())
#else
	for (UMaterialExpression* E : M->Expressions)
#endif
	{
		if (auto* P = Cast<UMaterialExpressionParameter>(E))
		{
			if (P->Group.IsNone())
			{
				P->Modify();
				P->Group = FName(*NewName);
			}
		}
	}
	M->PostEditChange();
	M->MarkPackageDirty();
	PullFromInstance();
	CurrentTab = FName(*NewName);
	RebuildInstanceContent();
	return FReply::Handled();
}

void SMaterialLayoutProPanel::OnDeleteTab(FName GroupName)
{
	if (!TargetMaterial.IsValid()) return;
	auto* M = TargetMaterial.Get();
	const FScopedTransaction T(FText::FromString(TEXT("删除分组")));
	M->Modify();
#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* E : M->GetExpressions())
#else
	for (UMaterialExpression* E : M->Expressions)
#endif
	{
		if (auto* P = Cast<UMaterialExpressionParameter>(E))
		{
			if (P->Group == GroupName)
			{
				P->Modify();
				P->Group = NAME_None;
			}
		}
	}
	M->PostEditChange();
	M->MarkPackageDirty();
	PullFromInstance();
	if (CurrentTab == GroupName) CurrentTab = InstanceTabNames.Num() > 0 ? InstanceTabNames[0] : NAME_None;
	RebuildInstanceContent();
}

void SMaterialLayoutProPanel::OnRenameTab(FName OldName, const FText& NewName, ETextCommit::Type)
{
	if (!TargetMaterial.IsValid() || NewName.IsEmptyOrWhitespace()) return;
	FName NewGroupName(*NewName.ToString());
	auto* M = TargetMaterial.Get();
	const FScopedTransaction T(FText::FromString(TEXT("重命名分组")));
	M->Modify();
#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* E : M->GetExpressions())
#else
	for (UMaterialExpression* E : M->Expressions)
#endif
	{
		if (auto* P = Cast<UMaterialExpressionParameter>(E))
		{
			if (P->Group == OldName)
			{
				P->Modify();
				P->Group = NewGroupName;
			}
		}
	}
	M->PostEditChange();
	M->MarkPackageDirty();
	PullFromInstance();
	CurrentTab = NewGroupName;
	RebuildInstanceContent();
}

void SMaterialLayoutProPanel::OnToggleOverride(TSharedPtr<FMLPInstanceParamVM> Param)
{
	if (!Param.IsValid() || !TargetMaterialInstance.IsValid()) return;
	UMaterialInstance* MI = TargetMaterialInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("切换参数覆盖")));
	MI->Modify();

	if (Param->bOverridden)
	{
		// Remove override.
		switch (Param->Type)
		{
		case (int32)EMLPParameterType::Scalar:
			MI->ScalarParameterValues.RemoveAll([&](const FScalarParameterValue& V) { return V.ParameterInfo.Name == Param->Name; });
			break;
		case (int32)EMLPParameterType::Vector:
			MI->VectorParameterValues.RemoveAll([&](const FVectorParameterValue& V) { return V.ParameterInfo.Name == Param->Name; });
			break;
		case (int32)EMLPParameterType::Texture:
			MI->TextureParameterValues.RemoveAll([&](const FTextureParameterValue& V) { return V.ParameterInfo.Name == Param->Name; });
			break;
		case (int32)EMLPParameterType::StaticBool:
		case (int32)EMLPParameterType::StaticSwitch:
			// Static switch overrides live in StaticParameters, not the typed value arrays.
			SetStaticSwitchOverride(Param, false, Param->BoolValue);
			break;
		default: break;
		}
		Param->bOverridden = false;
	}
	else
	{
		// Add override with current value.
		switch (Param->Type)
		{
		case (int32)EMLPParameterType::Scalar:
			{
				FScalarParameterValue SV;
				SV.ParameterInfo = FMaterialParameterInfo(Param->Name);
				SV.ParameterValue = Param->ScalarValue;
				SV.ExpressionGUID = Param->ExpressionGUID;
				MI->ScalarParameterValues.Add(SV);
			}
			break;
		case (int32)EMLPParameterType::Vector:
			{
				FVectorParameterValue VV;
				VV.ParameterInfo = FMaterialParameterInfo(Param->Name);
				VV.ParameterValue = Param->VectorValue;
				VV.ExpressionGUID = Param->ExpressionGUID;
				MI->VectorParameterValues.Add(VV);
			}
			break;
		case (int32)EMLPParameterType::Texture:
			{
				FTextureParameterValue TV;
				TV.ParameterInfo = FMaterialParameterInfo(Param->Name);
				TV.ParameterValue = Param->TextureValue.Get();
				TV.ExpressionGUID = Param->ExpressionGUID;
				MI->TextureParameterValues.Add(TV);
			}
			break;
		case (int32)EMLPParameterType::StaticBool:
		case (int32)EMLPParameterType::StaticSwitch:
			SetStaticSwitchOverride(Param, true, Param->BoolValue);
			break;
		default: break;
		}
		Param->bOverridden = true;
	}

	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialLayoutProPanel::OnInstanceScalarChanged(TSharedPtr<FMLPInstanceParamVM> Param, float NewValue, ETextCommit::Type)
{
	if (!Param.IsValid() || !TargetMaterialInstance.IsValid()) return;
	Param->ScalarValue = NewValue;
	UMaterialInstance* MI = TargetMaterialInstance.Get();
	MI->Modify();
	// Update existing override or add new.
	bool bFound = false;
	for (auto& V : MI->ScalarParameterValues)
	{
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = NewValue; bFound = true; break; }
	}
	if (!bFound)
	{
		FScalarParameterValue SV;
		SV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		SV.ParameterValue = NewValue;
		SV.ExpressionGUID = Param->ExpressionGUID;
		MI->ScalarParameterValues.Add(SV);
		Param->bOverridden = true;
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialLayoutProPanel::OnInstanceVectorChanged(TSharedPtr<FMLPInstanceParamVM> Param, FLinearColor NewColor)
{
	if (!Param.IsValid() || !TargetMaterialInstance.IsValid()) return;
	Param->VectorValue = NewColor;
	UMaterialInstance* MI = TargetMaterialInstance.Get();
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->VectorParameterValues)
	{
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = NewColor; bFound = true; break; }
	}
	if (!bFound)
	{
		FVectorParameterValue VV;
		VV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		VV.ParameterValue = NewColor;
		VV.ExpressionGUID = Param->ExpressionGUID;
		MI->VectorParameterValues.Add(VV);
		Param->bOverridden = true;
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialLayoutProPanel::OnInstanceTextureChanged(TSharedPtr<FMLPInstanceParamVM> Param, UObject* NewTexture)
{
	if (!Param.IsValid() || !TargetMaterialInstance.IsValid()) return;
	Param->TextureValue = Cast<UTexture>(NewTexture);
	UMaterialInstance* MI = TargetMaterialInstance.Get();
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->TextureParameterValues)
	{
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = Cast<UTexture>(NewTexture); bFound = true; break; }
	}
	if (!bFound)
	{
		FTextureParameterValue TV;
		TV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		TV.ParameterValue = Cast<UTexture>(NewTexture);
		TV.ExpressionGUID = Param->ExpressionGUID;
		MI->TextureParameterValues.Add(TV);
		Param->bOverridden = true;
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialLayoutProPanel::OnInstanceBoolChanged(TSharedPtr<FMLPInstanceParamVM> Param, bool bNewValue)
{
	if (!Param.IsValid() || !TargetMaterialInstance.IsValid()) return;
	Param->BoolValue = bNewValue;
	UMaterialInstance* MI = TargetMaterialInstance.Get();
	// Static switch parameters live in StaticParameters.StaticSwitchParameters, not the typed
	// value arrays. Update via UpdateStaticPermutation so the permutation recompiles.
	SetStaticSwitchOverride(Param, Param->bOverridden, bNewValue);
}

void SMaterialLayoutProPanel::SetStaticSwitchOverride(TSharedPtr<FMLPInstanceParamVM> Param, bool bOverride, bool bNewValue)
{
	if (!Param.IsValid() || !TargetMaterialInstance.IsValid()) return;
	UMaterialInstance* MI = TargetMaterialInstance.Get();

	const FScopedTransaction T(FText::FromString(TEXT("修改静态开关参数")));
	MI->Modify();

	// Get the current static parameter set (parent + overrides), mutate the matching entry,
	// and push it back with UpdateStaticPermutation. This is the same path the UE details
	// panel uses for static switch editing.
	FStaticParameterSet ParamSet;
	MI->GetStaticParameterValues(ParamSet);

	bool bChanged = false;
	for (FStaticSwitchParameter& SP : ParamSet.StaticSwitchParameters)
	{
		if (SP.ParameterInfo.Name == Param->Name)
		{
			SP.bOverride = bOverride;
			SP.Value = bNewValue;
			bChanged = true;
			break;
		}
	}
	if (!bChanged && bOverride)
	{
		// Parameter exists in parent but wasn't in the instance set yet — append.
		FStaticSwitchParameter SP(FMaterialParameterInfo(Param->Name), bNewValue, true, Param->ExpressionGUID);
		ParamSet.StaticSwitchParameters.Add(SP);
		bChanged = true;
	}

	if (bChanged)
	{
		// UpdateStaticPermutation recompiles the instance permutation; passing nullptr for
		// the update context lets it allocate one internally.
		MI->UpdateStaticPermutation(ParamSet);
		Param->bOverridden = bOverride;
	}

	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

// ============================================================================
// Instance group panel - opens as a separate window
// ============================================================================

FReply SMaterialLayoutProPanel::OnInstanceGroupClicked()
{
	// Ensure target is resolved (panel may be freshly created).
	ResolveTargetMaterial();
	if (!TargetMaterialInstance.IsValid())
	{
		// Show error window if no instance found.
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(FText::FromString(TEXT("材质实例参数分组")))
			.ClientSize(FVector2D(400, 100))
			.SizingRule(ESizingRule::UserSized)
			[
				SNew(SBorder)
				.Padding(FMargin(16))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoInstance", "未找到材质实例"))
					.Font(FMLPTheme::FontBody())
					.ColorAndOpacity(FMLPTheme::Muted())
				]
			];
		FSlateApplication::Get().AddWindow(Window);
		return FReply::Handled();
	}

	PullFromInstance();

	// Default to first tab.
	if (!InstanceTabNames.Contains(CurrentTab))
		CurrentTab = InstanceTabNames.Num() > 0 ? InstanceTabNames[0] : NAME_None;

	// Build content and open as a standalone window.
	TSharedRef<SWidget> Content = BuildInstanceContent();

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(FString::Printf(TEXT("参数分组 - %s"), *TargetMaterialInstance->GetName())))
		.ClientSize(FVector2D(600, 400))
		.SizingRule(ESizingRule::UserSized)
		[
			Content
		];

	FSlateApplication::Get().AddWindow(Window);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
