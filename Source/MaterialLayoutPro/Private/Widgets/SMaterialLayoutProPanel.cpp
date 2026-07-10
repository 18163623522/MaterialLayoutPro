#include "Widgets/SMaterialLayoutProPanel.h"
#include "MaterialLayoutPro.h"
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
	// The session wrote a new value/group/name back to the material. Push the change through
	// the material editor's sync path so it lands on OriginalMaterial (the on-disk copy).
	NotifyMaterialEditorChanged();
}

void SMaterialLayoutProPanel::NotifyMaterialEditorChanged()
{
	if (OwningMaterialEditor.IsValid())
	{
		TSharedPtr<IMaterialEditor> Editor = OwningMaterialEditor.Pin();
		if (Editor.IsValid())
		{
			// UpdateMaterialAfterGraphChange re-links expressions from the graph, refreshes the
			// preview, marks the package dirty, AND — critically — calls SetMaterialDirty(). That
			// sets bMaterialDirty, which the editor checks on close/save to decide whether to
			// duplicate the preview copy onto OriginalMaterial. Without it, edits made here are
			// visible live but lost on save/reopen (the saved OriginalMaterial is never updated).
			Editor->UpdateMaterialAfterGraphChange();
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
	NotifyMaterialEditorChanged();
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
	NotifyMaterialEditorChanged();
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
	M->PostEditChange(); M->MarkPackageDirty(); NotifyMaterialEditorChanged(); RefreshParameters();
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
	M->PostEditChange(); M->MarkPackageDirty(); NotifyMaterialEditorChanged(); RefreshParameters();
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
	M->PostEditChange(); M->MarkPackageDirty(); NotifyMaterialEditorChanged(); RefreshParameters();
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
	M->PostEditChange(); M->MarkPackageDirty(); NotifyMaterialEditorChanged(); RefreshParameters();
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
		Mat->PostEditChange(); Mat->MarkPackageDirty(); NotifyMaterialEditorChanged(); RefreshParameters();
	}
	return FReply::Handled();
}

FReply SMaterialLayoutProPanel::OnSortWorkbenchClicked()
{
	if (!TargetMaterial.IsValid()) return FReply::Handled();
	auto Params = FMaterialParameterScanner::ScanMaterial(TargetMaterial.Get());
	FSlateApplication::Get().AddWindow(SNew(SMaterialSortWorkbench).TargetMaterial(TargetMaterial).Parameters(Params)
		.OnApplied(FSimpleDelegate::CreateLambda([this](){
			// SortWorkbench writes Group/SortPriority directly to expressions; sync to the
			// editor's OriginalMaterial so the edits survive save/reopen.
			NotifyMaterialEditorChanged();
			RefreshParameters();
		})));
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
// Instance group panel - opens as a separate window
// ============================================================================

FReply SMaterialLayoutProPanel::OnInstanceGroupClicked()
{
	// Resolve the bound instance (panel may be in a material-instance editor) and delegate to
	// the module's window helper. The window's content IS the SMaterialInstanceGroupPanel,
	// so the panel lives exactly as long as the window — its tab/override/value callbacks
	// are therefore safe. (The old implementation built a loose child SVerticalBox as window
	// content and let this panel be destroyed, leaving every callback resolving to a dead
	// weak ptr — Tab clicks and override toggles silently did nothing.)
	ResolveTargetMaterial();
	FMaterialLayoutProModule::OpenInstanceGroupWindow(TargetMaterialInstance.Get());
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
