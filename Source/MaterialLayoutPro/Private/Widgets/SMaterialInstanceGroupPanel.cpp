#include "Widgets/SMaterialInstanceGroupPanel.h"
#include "MaterialLayoutProTheme.h"
#include "MaterialInstanceGroupData.h"
#include "Model/MaterialParameterScanner.h"
#include "Model/MaterialParameterInfo.h"
#include "Model/MaterialInstanceParamDragDrop.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "StaticParameterSet.h"
#include "Engine/Texture.h"

#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/MessageDialog.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Styling/CoreStyle.h"
#include "IMaterialEditor.h"
#include "Toolkits/AssetEditorToolkit.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_STYLE FAppStyle
#else
#include "EditorStyleSet.h"
#define MLP_STYLE FEditorStyle
#endif

#define LOCTEXT_NAMESPACE "SMaterialInstanceGroupPanel"

// ============================================================================
// SInstanceParamDragSource
// ============================================================================

void SInstanceParamDragSource::Construct(const FArguments& InArgs)
{
	Param = InArgs._Param;
	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SInstanceParamDragSource::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton);
	}
	return FReply::Unhandled();
}

FReply SInstanceParamDragSource::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && Param.IsValid())
	{
		TSharedPtr<FMLPInstanceParamDragDrop> DragOp = MakeShared<FMLPInstanceParamDragDrop>(Param);
		return FReply::Handled().BeginDragDrop(DragOp.ToSharedRef());
	}
	return FReply::Unhandled();
}

void SMaterialInstanceGroupPanel::Construct(const FArguments& InArgs)
{
	if (InArgs._OwningInstanceEditor.IsValid())
	{
		BindToInstanceEditor(InArgs._OwningInstanceEditor);
	}

	ChildSlot
	[
		BuildInitialContent()
	];
}

// ============================================================================
// Editor binding / resolution
// ============================================================================

void SMaterialInstanceGroupPanel::BindToInstanceEditor(TWeakPtr<IMaterialEditor> InEditor)
{
	OwningInstanceEditor = InEditor;
	ResolveTarget();
}

void SMaterialInstanceGroupPanel::ResolveTarget()
{
	if (OwningInstanceEditor.IsValid())
	{
		TSharedPtr<IMaterialEditor> Editor = OwningInstanceEditor.Pin();
		if (Editor.IsValid())
		{
			UMaterialInterface* MatInterface = Editor->GetMaterialInterface();
			if (UMaterialInstance* MI = Cast<UMaterialInstance>(MatInterface))
			{
				TargetInstance = MI;
				TargetMaterial = MI->GetBaseMaterial();
				GroupData = UMaterialInstanceGroupData::GetOrCreate(MI);
				return;
			}
			// Not an instance — clear.
			TargetInstance.Reset();
			TargetMaterial.Reset();
			GroupData = nullptr;
		}
	}
}

void SMaterialInstanceGroupPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// Cache our geometry each tick so OnDrop/OnDragOver can map absolute pointer coords to
	// group-title-bar rectangles (reliable drag-drop without depending on HittestGrid).
	PanelGeometry = AllottedGeometry;

	// Re-resolve every ~0.5s in case the editor switched assets.
	if (LastPollTime.IsSet() && InCurrentTime - LastPollTime.GetValue() < 0.5) return;
	LastPollTime = InCurrentTime;

	if (OwningInstanceEditor.IsValid())
	{
		TSharedPtr<IMaterialEditor> Editor = OwningInstanceEditor.Pin();
		if (!Editor.IsValid() || !Editor->GetMaterialInterface())
		{
			OwningInstanceEditor.Reset();
			TargetInstance.Reset();
			TargetMaterial.Reset();
			GroupData = nullptr;
		}
		else
		{
			UMaterialInstance* MI = Cast<UMaterialInstance>(Editor->GetMaterialInterface());
			if (MI && MI != TargetInstance.Get())
			{
				ResolveTarget();
				PullFromInstance();
				RebuildInstanceContent();
			}
		}
	}
}

FName SMaterialInstanceGroupPanel::FindGroupAtPosition(const FVector2D& AbsolutePos) const
{
	// Each group's drop zone spans from its title bar DOWN to the NEXT group's title bar, so
	// dropping anywhere within a group (its title bar, parameter rows, or empty area) targets
	// that group. The previous version only matched the thin title-bar rectangle, so dropping
	// on a parameter row or the group's body silently did nothing — the drag "appeared broken"
	// even though the drop event itself was being delivered correctly.
	//
	// Title-bar absolute rects are recomputed every Tick (they account for scroll offset), so
	// the Y-partition stays correct while scrolling mid-drag.
	if (GroupTitleWidgets.Num() == 0) return NAME_None;

	struct FEntry { float TopY; FName Name; };
	TArray<FEntry> Entries;
	Entries.Reserve(GroupTitleWidgets.Num());
	for (const auto& Pair : GroupTitleWidgets)
	{
		TSharedPtr<SWidget> W = Pair.Value.Pin();
		if (!W.IsValid()) continue;
		Entries.Add(FEntry{ W->GetCachedGeometry().GetLayoutBoundingRect().Top, Pair.Key });
	}
	if (Entries.Num() == 0) return NAME_None;
	Entries.Sort([](const FEntry& A, const FEntry& B) { return A.TopY < B.TopY; });

	// Cursor above the FIRST title bar (toolbar) = no target (lets the user drop there to cancel).
	if (AbsolutePos.Y < Entries[0].TopY) return NAME_None;

	// The owning group is the last title bar at or above the cursor Y.
	FName Hit = NAME_None;
	for (const FEntry& E : Entries)
	{
		if (AbsolutePos.Y >= E.TopY) Hit = E.Name;
		else break;
	}
	return Hit;
}

void SMaterialInstanceGroupPanel::ComputeDropTarget(const FVector2D& AbsolutePos, FName& OutGroup, int32& OutInsertInGroup) const
{
	OutGroup = NAME_None;
	OutInsertInGroup = INDEX_NONE;

	// Disable drop targeting while a search filter is active: the filtered row set makes the
	// insertion index relative to the visible subset, not the full group, so renumbering would
	// scramble order. Clear the search first, then drag.
	if (!SearchText.ToString().TrimStartAndEnd().IsEmpty()) return;

	// 1. Which group? (whole-group Y-partition by title bars — already verified to work.)
	const FName Group = FindGroupAtPosition(AbsolutePos);
	if (Group.IsNone()) return;

	// 2. Collect THIS group's rows (in render order = ParamSort order) with their geometry.
	struct FRowEntry { float MidY; };
	TArray<FRowEntry> Rows;
	for (const auto& Pair : ParamRowWidgets)
	{
		if (Pair.Key != Group) continue;
		TSharedPtr<SWidget> W = Pair.Value.Pin();
		if (!W.IsValid()) continue;
		const FSlateRect R = W->GetCachedGeometry().GetLayoutBoundingRect();
		Rows.Add(FRowEntry{ (R.Top + R.Bottom) * 0.5f });
	}

	OutGroup = Group;

	// 3. Insertion index = number of rows whose MIDPOINT is above the cursor. So landing on the
	//    top half of row N → insert at N (before row N); bottom half of row N → insert at N+1.
	//    Above the first row's mid → 0; below the last row's mid → Rows.Num() (append).
	int32 Idx = 0;
	for (const FRowEntry& E : Rows)
	{
		if (AbsolutePos.Y >= E.MidY) ++Idx;
		else break;
	}
	OutInsertInGroup = Idx;
}

void SMaterialInstanceGroupPanel::HandleParamDropped(const FVector2D& AbsolutePos, TSharedPtr<FMLPInstanceParamVM> Param)
{
	DragOverGroup = NAME_None;
	DragOverInsertIndex = INDEX_NONE;

	FName TargetGroup; int32 InsertIdx;
	ComputeDropTarget(AbsolutePos, TargetGroup, InsertIdx);
	if (TargetGroup != NAME_None && Param.IsValid() && InsertIdx != INDEX_NONE)
	{
		OnParamInsertedAt(Param, TargetGroup, InsertIdx);
	}
}

void SMaterialInstanceGroupPanel::HandleDragOverPos(const FVector2D& AbsolutePos)
{
	FName Group; int32 InsertIdx;
	ComputeDropTarget(AbsolutePos, Group, InsertIdx);
	const FName OldGroup = DragOverGroup;
	const int32 OldIdx = DragOverInsertIndex;
	DragOverGroup = Group;
	DragOverInsertIndex = InsertIdx;
	// Rebuild to (re)draw the blue-line indicator when the drop slot changes.
	if (OldGroup != Group || OldIdx != InsertIdx)
	{
		RebuildInstanceContent();
	}
}

// ============================================================================
// SInstanceDropArea
// ============================================================================

void SInstanceDropArea::Construct(const FArguments& InArgs)
{
	OnDroppedDelegate = InArgs._OnDropped;
	OnDragOverDelegate = InArgs._OnDragOverPos;

	// Paint a (fully transparent) background so this SBorder is in the HittestGrid and receives
	// drag-over/drop. SBorder subclasses are registered as hit-test targets because they paint.
	SBorder::Construct(SBorder::FArguments()
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::Transparent)
		.Padding(FMargin(0.f))
		[
			InArgs._Content.Widget
		]);
}

FReply SInstanceDropArea::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FMLPInstanceParamDragDrop> DragOp = DragDropEvent.GetOperationAs<FMLPInstanceParamDragDrop>();
	if (DragOp.IsValid() && DragOp->IsValid())
	{
		OnDragOverDelegate.ExecuteIfBound(DragDropEvent.GetScreenSpacePosition());
		return FReply::Handled();
	}
	return SBorder::OnDragOver(MyGeometry, DragDropEvent);
}

FReply SInstanceDropArea::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FMLPInstanceParamDragDrop> DragOp = DragDropEvent.GetOperationAs<FMLPInstanceParamDragDrop>();
	if (DragOp.IsValid() && DragOp->IsValid())
	{
		OnDroppedDelegate.ExecuteIfBound(DragDropEvent.GetScreenSpacePosition(), DragOp->Param);
		return FReply::Handled();
	}
	return SBorder::OnDrop(MyGeometry, DragDropEvent);
}

void SInstanceDropArea::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SBorder::OnDragLeave(DragDropEvent);
}

// ============================================================================
// Data load
// ============================================================================

void SMaterialInstanceGroupPanel::PullFromInstance()
{
	AllParams.Reset();
	Groups.Reset();

	UMaterialInstance* MI = TargetInstance.Get();
	UMaterial* BaseMat = TargetMaterial.Get();
	if (!MI || !BaseMat) return;

	auto ScannedParams = FMaterialParameterScanner::ScanMaterial(BaseMat);

	for (const auto& P : ScannedParams)
	{
		if (!P.IsValid() || !P->Expression.IsValid()) continue;

		TSharedPtr<FMLPInstanceParamVM> VM = MakeShared<FMLPInstanceParamVM>();
		VM->Name = P->Name;
		VM->BaseGroup = P->Group.IsNone() ? FName(TEXT("(None)")) : P->Group;
		// Effective group = custom override (AssetUserData) if present, else base.
		if (GroupData.IsValid())
		{
			UMaterialInstanceGroupData* GD = GroupData.Get();
			VM->EffectiveGroup = GD->ResolveGroup(VM->Name, VM->BaseGroup);
		}
		else
		{
			VM->EffectiveGroup = VM->BaseGroup;
		}
		VM->ExpressionGUID = P->Guid;
		VM->Type = (int32)P->Type;
		VM->bHasDuplicateName = P->bHasDuplicateName;

		// Fold in instance override values.
		FHashedMaterialParameterInfo ParamInfo(VM->Name);
		if (P->Type == EMLPParameterType::Scalar)
		{
			float OutVal;
			VM->bOverridden = MI->GetScalarParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->ScalarValue = OutVal;
			else if (auto* E = Cast<UMaterialExpressionScalarParameter>(P->Expression.Get())) VM->ScalarValue = E->DefaultValue;
		}
		else if (P->Type == EMLPParameterType::Vector)
		{
			FLinearColor OutVal;
			VM->bOverridden = MI->GetVectorParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->VectorValue = OutVal;
			else if (auto* E = Cast<UMaterialExpressionVectorParameter>(P->Expression.Get())) VM->VectorValue = E->DefaultValue;
		}
		else if (P->Type == EMLPParameterType::Texture)
		{
			UTexture* OutVal;
			VM->bOverridden = MI->GetTextureParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->TextureValue = OutVal;
			else if (auto* TS = Cast<UMaterialExpressionTextureSampleParameter>(P->Expression.Get())) VM->TextureValue = TS->Texture;
			else if (auto* TO = Cast<UMaterialExpressionTextureObjectParameter>(P->Expression.Get())) VM->TextureValue = TO->Texture;
		}
		else if (P->Type == EMLPParameterType::StaticBool || P->Type == EMLPParameterType::StaticSwitch)
		{
			bool bOutVal; FGuid OutGuid;
			VM->bOverridden = MI->GetStaticSwitchParameterValue(ParamInfo, bOutVal, OutGuid, true);
			if (VM->bOverridden) VM->BoolValue = bOutVal;
			else if (auto* E = Cast<UMaterialExpressionStaticBoolParameter>(P->Expression.Get())) VM->BoolValue = E->DefaultValue;
		}

		AllParams.Add(VM);
	}

	// Build groups from EffectiveGroup.
	TMap<FName, TSharedPtr<FMLPInstanceGroupVM>> GroupMap;
	for (const auto& VM : AllParams)
	{
		TSharedPtr<FMLPInstanceGroupVM>* Found = GroupMap.Find(VM->EffectiveGroup);
		if (!Found)
		{
			auto NewGroup = MakeShared<FMLPInstanceGroupVM>();
			NewGroup->Name = VM->EffectiveGroup;
			Found = &GroupMap.Add(VM->EffectiveGroup, NewGroup);
		}
		(*Found)->Parameters.Add(VM);
	}

	// Group order: use the user's custom GroupOrder (AssetUserData) when present and PRESERVE it
	// (do NOT re-sort alphabetically — that would clobber the user's manual ordering). Only fall
	// back to alphabetical-with-(None)-last when there is no custom order yet. Any newly-seen
	// groups not in GroupOrder are appended at the end.
	TArray<FName> Order;
	const bool bHasCustomOrder = GroupData.IsValid() && GroupData->GetGroupOrder().Num() > 0;
	if (bHasCustomOrder)
	{
		Order = GroupData->GetGroupOrder();
		for (auto& Pair : GroupMap)
		{
			if (!Order.Contains(Pair.Key)) Order.Add(Pair.Key);
		}
	}
	else
	{
		for (auto& Pair : GroupMap) Order.Add(Pair.Key);
		Order.Sort([](const FName& A, const FName& B)
		{
			if (A == TEXT("(None)")) return false;
			if (B == TEXT("(None)")) return true;
			return A.ToString() < B.ToString();
		});
	}
	for (int32 i = 0; i < Order.Num(); ++i)
	{
		if (auto* G = GroupMap.Find(Order[i]))
		{
			(*G)->SortPriority = i;
			Groups.Add(*G);
		}
		else
		{
			// Group exists in the custom GroupOrder but has no parameters yet (e.g. just
			// created via "新建分组"). Show it as an empty group so the user can move
			// params into it via the per-row group dropdown.
			auto EmptyGroup = MakeShared<FMLPInstanceGroupVM>();
			EmptyGroup->Name = Order[i];
			EmptyGroup->SortPriority = i;
			Groups.Add(EmptyGroup);
		}
	}

	// Sort params within each group by ParamSort (AssetUserData), then by name.
	if (GroupData.IsValid())
	{
		UMaterialInstanceGroupData* GD = GroupData.Get();
		for (auto& G : Groups)
		{
			G->Parameters.Sort([GD](const TSharedPtr<FMLPInstanceParamVM>& A, const TSharedPtr<FMLPInstanceParamVM>& B)
			{
				const int32 Sa = GD->GetParamSort(A->Name);
				const int32 Sb = GD->GetParamSort(B->Name);
				if (Sa != Sb) return Sa < Sb;
				return A->Name.ToString() < B->Name.ToString();
			});
		}
	}
}

// ============================================================================
// UI build / refresh
// ============================================================================

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildInitialContent()
{
	if (!TargetInstance.IsValid())
	{
		return SNew(SBorder)
			.Padding(FMargin(16)).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoInstance", "未找到材质实例"))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Muted())
			];
	}

	PullFromInstance();

	TSharedPtr<SVerticalBox> Inner = SNew(SVerticalBox);
	ContentContainer = Inner;

	// Toolbar + status persist; group list is rebuilt.
	Inner->AddSlot().AutoHeight().Padding(FMargin(2))[ BuildToolbar() ];
	// Search/filter row — case-insensitive substring match on param names; rebuilds content live.
	Inner->AddSlot().AutoHeight().Padding(FMargin(2, 0, 2, 2))
	[
		SNew(SSearchBox)
		.HintText(LOCTEXT("SearchHint", "搜索参数名..."))
		.ToolTipText(LOCTEXT("SearchTT", "按参数名过滤(不区分大小写)。清空显示全部。"))
		.OnTextChanged(this, &SMaterialInstanceGroupPanel::OnSearchChanged)
	];
	Inner->AddSlot().FillHeight(1.0f)
	[
		SAssignNew(ParamScroll, SScrollBox)
	];
	Inner->AddSlot().AutoHeight().Padding(FMargin(2))[ BuildStatusBar() ];

	RebuildInstanceContent();

	// Wrap everything in SInstanceDropArea (an SBorder subclass that paints → in HittestGrid →
	// reliably receives drag-over/drop). It delegates to the panel's geometry hit-test.
	return SNew(SInstanceDropArea)
		.OnDropped(SInstanceDropArea::FOnParamDropped::CreateSP(SharedThis(this), &SMaterialInstanceGroupPanel::HandleParamDropped))
		.OnDragOverPos(SInstanceDropArea::FOnDragOverPos::CreateSP(SharedThis(this), &SMaterialInstanceGroupPanel::HandleDragOverPos))
		[
			Inner.ToSharedRef()
		];
}

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildToolbar()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SMaterialInstanceGroupPanel::GetInstanceName)
			.Font(FMLPTheme::FontHeading())
			.ColorAndOpacity(FMLPTheme::Foreground())
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(), "FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("R", "刷新")).ToolTipText(LOCTEXT("RT", "重新扫描参数"))
			.OnClicked(this, &SMaterialInstanceGroupPanel::OnRefreshClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(), "FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("CollapseAll", "全折叠")).ToolTipText(LOCTEXT("CollapseAllTT", "折叠所有分组"))
			.OnClicked(this, &SMaterialInstanceGroupPanel::OnCollapseAllGroupsClicked)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton).ButtonStyle(MLP_STYLE::Get(), "FlatButton").ContentPadding(FMLPTheme::PadBtn())
			.Text(LOCTEXT("ExpandAll", "全展开")).ToolTipText(LOCTEXT("ExpandAllTT", "展开所有分组"))
			.OnClicked(this, &SMaterialInstanceGroupPanel::OnExpandAllGroupsClicked)
		]
		// "更多 ▾" - less-frequently-used actions grouped in a dropdown to keep the toolbar narrow.
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SComboButton)
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ContentPadding(FMLPTheme::PadBtn())
			.ToolTipText(LOCTEXT("MoreTT", "定位资产、新建分组、覆盖管理等更多操作"))
			.OnGetMenuContent(this, &SMaterialInstanceGroupPanel::BuildMoreMenu)
			.ButtonContent()
			[
				SNew(STextBlock).Text(LOCTEXT("More", "更多 ▾"))
			]
		];
}

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildMoreMenu()
{
	FMenuBuilder Menu(true, nullptr);

	// --- 资产 / 分组 ---
	Menu.AddMenuEntry(LOCTEXT("Locate", "定位资产"), LOCTEXT("LocateTT", "在内容浏览器中选中并定位此材质实例"), FSlateIcon(),
		FExecuteAction::CreateLambda([this]() { OnLocateAssetClicked(); }));
	Menu.AddMenuEntry(LOCTEXT("AG", "新建分组"), LOCTEXT("AGT", "新建一个空分组(可在参数行的组下拉里把参数移过来)"), FSlateIcon(),
		FExecuteAction::CreateLambda([this]() { OnAddGroupClicked(); }));

	Menu.AddMenuSeparator();

	// --- 覆盖管理 ---
	Menu.AddMenuEntry(LOCTEXT("EnableAll", "全部启用覆盖"), LOCTEXT("EnableAllTT", "用当前值为所有参数启用覆盖"), FSlateIcon(),
		FExecuteAction::CreateLambda([this]() { OnEnableAllOverridesClicked(); }));
	Menu.AddMenuEntry(LOCTEXT("ResetAll", "重置全部覆盖"), LOCTEXT("ResetAllTT", "清除所有参数覆盖,回退到父材质默认值"), FSlateIcon(),
		FExecuteAction::CreateLambda([this]() { OnResetAllOverridesClicked(); }));

	return Menu.MakeWidget();
}

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildStatusBar()
{
	return SNew(STextBlock)
		.Text(this, &SMaterialInstanceGroupPanel::GetStatusText)
		.Font(FMLPTheme::FontSmall())
		.ColorAndOpacity(FMLPTheme::Muted());
}

void SMaterialInstanceGroupPanel::RebuildInstanceContent()
{
	if (!ParamScroll.IsValid()) return;

	// Clear cached group-title widgets (rebuilt below) and the shared combo options list.
	GroupTitleWidgets.Reset();
	ParamRowWidgets.Reset();
	CachedGroupNames.Reset();
	for (const auto& G : Groups) CachedGroupNames.Add(MakeShared<FName>(G->Name));

	ParamScroll->ClearChildren();

	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox);
	BuildGroupSections(Content);

	ParamScroll->AddSlot()
	[
		Content
	];
}

bool SMaterialInstanceGroupPanel::PassesSearchFilter(const FName& ParamName) const
{
	// Empty search = show everything.
	const FString Query = SearchText.ToString().TrimStartAndEnd();
	if (Query.IsEmpty()) return true;
	// Case-insensitive substring match on the param name.
	return ParamName.ToString().Contains(Query);
}

void SMaterialInstanceGroupPanel::OnSearchChanged(const FText& NewText)
{
	SearchText = NewText;
	// While filtering, drop drag state — the filtered layout invalidates row geometry, so a
	// stale DragOverGroup/DragOverInsertIndex would point at the wrong rows.
	DragOverGroup = NAME_None;
	DragOverInsertIndex = INDEX_NONE;
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnToggleGroupCollapsed(FName GroupName)
{
	if (CollapsedGroups.Contains(GroupName))
	{
		CollapsedGroups.Remove(GroupName);
	}
	else
	{
		CollapsedGroups.Add(GroupName);
	}
	// Collapsed groups can't be a drag target (their rows are hidden).
	DragOverGroup = NAME_None;
	DragOverInsertIndex = INDEX_NONE;
	RebuildInstanceContent();
}

bool SMaterialInstanceGroupPanel::IsGroupCollapsed(FName GroupName) const
{
	// A search forces groups to expand so matching rows are visible — collapsing during a
	// search would hide exactly the params the user is looking for.
	if (!SearchText.ToString().TrimStartAndEnd().IsEmpty()) return false;
	return CollapsedGroups.Contains(GroupName);
}

FReply SMaterialInstanceGroupPanel::OnCollapseAllGroupsClicked()
{
	CollapsedGroups.Reset();
	for (const auto& G : Groups)
	{
		if (G.IsValid()) CollapsedGroups.Add(G->Name);
	}
	DragOverGroup = NAME_None;
	DragOverInsertIndex = INDEX_NONE;
	RebuildInstanceContent();
	return FReply::Handled();
}

FReply SMaterialInstanceGroupPanel::OnExpandAllGroupsClicked()
{
	if (CollapsedGroups.Num() == 0) return FReply::Handled();
	CollapsedGroups.Reset();
	RebuildInstanceContent();
	return FReply::Handled();
}

void SMaterialInstanceGroupPanel::BuildGroupSections(TSharedRef<SVerticalBox> ContentBox)
{
	TWeakPtr<SMaterialInstanceGroupPanel, ESPMode::NotThreadSafe> WeakSelf = StaticCastSharedRef<SMaterialInstanceGroupPanel>(AsShared());

	// Builds the 2px blue drop-indicator line shown between rows / at group end during a drag.
	auto MakeDropIndicator = []() -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FMLPTheme::Accent())
			.Padding(FMargin(2.f, 0.f))
			[
				SNew(SBox).HeightOverride(2.f)
			];
	};

	// Reusable editable-text-box style with a dark-theme-friendly background (the default
	// style is opaque white, making the text invisible on the dark group title bar). Mirrors
	// the style used by SMaterialParameterRow so the two panels look consistent.
	static FEditableTextBoxStyle EditableStyle;
	static bool bStyleInit = false;
	if (!bStyleInit)
	{
		EditableStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		EditableStyle.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.04f));
		EditableStyle.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f));
		EditableStyle.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.039f, 0.561f, 0.890f, 0.15f));
		EditableStyle.Padding = FMargin(2.f, 1.f);
		bStyleInit = true;
	}

	if (Groups.Num() == 0)
	{
		ContentBox->AddSlot().AutoHeight().Padding(FMargin(4, 8))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoParams", "未找到参数"))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
		];
		return;
	}

	for (const auto& Group : Groups)
	{
		if (!Group.IsValid()) continue;
		// NOTE: empty groups (just created via "新建分组") are NOT skipped — render the
		// title bar + an empty hint so the user can move params into it.

		const FName GroupName = Group->Name;
		const int32 SortPrio = Group->SortPriority;

		// Search filter: when a search is active, hide groups that have params but none match.
		// Genuinely-empty groups (Parameters.Num()==0, created via "新建分组") stay visible so the
		// user can still drop/move params into them while filtering.
		if (!SearchText.ToString().TrimStartAndEnd().IsEmpty() && Group->Parameters.Num() > 0)
		{
			bool bAnyMatch = false;
			for (const auto& P : Group->Parameters)
			{
				if (P.IsValid() && PassesSearchFilter(P->Name)) { bAnyMatch = true; break; }
			}
			if (!bAnyMatch) continue;  // whole group filtered out
		}
		// Whether the drag is currently targeting this group (drives both the title-bar
		// highlight AND the row-level blue-line indicator).
		const bool bDropTargetHere = (DragOverGroup == GroupName);
		const bool bCollapsed = IsGroupCollapsed(GroupName);
		int32 RowIdx = 0;  // index within this group, used for indicator placement

		// --- Group title bar. Tracked in GroupTitleWidgets so the panel-level OnDrop can
		// hit-test it (the SCompoundWidget drop-target approach wasn't receiving events).
		// Highlight when this is the group being dragged over (DragOverGroup).
		TSharedPtr<SBorder> TitleBar;
		ContentBox->AddSlot().AutoHeight().Padding(FMargin(0, 8, 0, 2))
		[
			SAssignNew(TitleBar, SBorder)
			.BorderBackgroundColor_Lambda([WeakSelf, GroupName]() -> FLinearColor {
				auto Self = WeakSelf.Pin();
				if (Self.IsValid() && Self->DragOverGroup == GroupName) return FMLPTheme::Accent();
				return FLinearColor(FMLPTheme::Accent().R, FMLPTheme::Accent().G, FMLPTheme::Accent().B, 0.18f);
			})
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(FMargin(4, 3, 4, 3))
				[
					SNew(SHorizontalBox)
				// Collapse/expand arrow (▶ collapsed / ▼ expanded). Separate button so it doesn't
				// conflict with the editable group name or the sort-priority numeric box.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 4, 0))
				[
					SNew(SButton)
					.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
					.ContentPadding(FMargin(2, 0))
					.Text_Lambda([WeakSelf, GroupName]() -> FText {
						auto Self = WeakSelf.Pin();
						return (Self.IsValid() && Self->IsGroupCollapsed(GroupName))
							? FText::FromString(TEXT("▶"))
							: FText::FromString(TEXT("▼"));
					})
					.ToolTipText(LOCTEXT("CollapseTT", "折叠/展开此分组"))
					.OnClicked_Lambda([WeakSelf, GroupName]() -> FReply {
						if (auto Self = WeakSelf.Pin()) Self->OnToggleGroupCollapsed(GroupName);
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(36.f)
					[
						SNew(SNumericEntryBox<int32>)
						.Value(TOptional<int32>(SortPrio))
						.Font(FMLPTheme::FontBody())
						.MinDesiredValueWidth(24.f)
						.AllowSpin(false)
						.ToolTipText(LOCTEXT("GroupSortTT", "排序号(小的在前)"))
						.OnValueCommitted_Lambda([WeakSelf, GroupName](int32 NewValue, ETextCommit::Type)
						{
							if (auto Self = WeakSelf.Pin()) Self->OnGroupSortChanged(GroupName, NewValue);
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(FMargin(6, 0, 0, 0))
				[
					// Editable group name — click/double-click to rename. Shows count in tooltip.
					// Rename writes AssetUserData only (never the parent material's expression Group).
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SEditableTextBox)
						.Style(&EditableStyle)
						.Text(FText::FromName(GroupName))
						.Font(FMLPTheme::FontHeading())
						.ForegroundColor(FMLPTheme::Foreground())
						.SelectAllTextWhenFocused(true)
						.IsReadOnly(false)
						.ToolTipText(FText::FromString(FString::Printf(TEXT("%d 个参数"), Group->Parameters.Num())))
						.OnTextCommitted_Lambda([WeakSelf, GroupName](const FText& NewName, ETextCommit::Type CommitType)
						{
							if (auto Self = WeakSelf.Pin()) Self->OnGroupRenamed(GroupName, NewName, CommitType);
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4, 0, 0, 0))
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("(%d)"), Group->Parameters.Num())))
						.Font(FMLPTheme::FontSmall())
						.ColorAndOpacity(FMLPTheme::Muted())
					]
				]
			]
		];

		// Track this group's title bar for panel-level drop hit-testing.
		if (TitleBar.IsValid())
		{
			GroupTitleWidgets.Add(TPair<FName, TWeakPtr<SWidget>>(GroupName, TitleBar));
		}

		// --- Parameter rows for this group ---
		// Collapsed groups render only their title bar (rows + empty hint + drop indicator hidden).
		for (const auto& P : Group->Parameters)
		{
			if (bCollapsed) break;  // collapsed: skip all rows
			// Skip params that don't match the search filter (group-level pre-check above already
			// hides groups with zero matches; this hides non-matching rows within a partial match).
			if (!P.IsValid() || !PassesSearchFilter(P->Name)) continue;

			TWeakPtr<FMLPInstanceParamVM> WeakVM = P;

			// Value editor per type (always editable; editing auto-enables override).
			TSharedRef<SWidget> ValueEditor = [WeakSelf, WeakVM]() -> TSharedRef<SWidget>
			{
				auto V = WeakVM.Pin();
				if (!V.IsValid()) return SNew(STextBlock).Text(FText::GetEmpty());

				switch (V->Type)
				{
				case (int32)EMLPParameterType::Scalar:
				{
					auto WeakV2 = WeakVM;
					return SNew(SNumericEntryBox<float>)
						.Value_Lambda([WeakV2]() -> TOptional<float> {
							auto V2 = WeakV2.Pin();
							return V2.IsValid() ? TOptional<float>(V2->ScalarValue) : TOptional<float>();
						})
						.Font(FMLPTheme::FontSmall())
						.AllowSpin(true)
						.MinValue(TOptional<float>()).MaxValue(TOptional<float>())
						.MinSliderValue(TOptional<float>()).MaxSliderValue(TOptional<float>())
						.MinDesiredValueWidth(80.f)
						.OnValueChanged_Lambda([WeakV2](float NewVal) {
							auto V2 = WeakV2.Pin(); if (V2.IsValid()) V2->ScalarValue = NewVal;
						})
						.OnValueCommitted_Lambda([WeakSelf, WeakV2](float NewVal, ETextCommit::Type) {
							auto Self = WeakSelf.Pin(); auto V2 = WeakV2.Pin();
							if (Self.IsValid() && V2.IsValid()) Self->OnInstanceScalarChanged(V2, NewVal, ETextCommit::Default);
						});
				}
				case (int32)EMLPParameterType::Vector:
				{
					auto WeakV2 = WeakVM;
					return SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 4, 0))
						[
							SNew(SBox).WidthOverride(28.f).HeightOverride(16.f)
							[
								SNew(SBorder)
								.BorderBackgroundColor_Lambda([WeakV2]() -> FLinearColor {
									auto V2 = WeakV2.Pin();
									if (!V2.IsValid()) return FLinearColor::Transparent;
									FLinearColor C = V2->VectorValue;
									if (C.A <= 0.001f) C.A = 1.0f;
									return C;
								})
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.OnMouseButtonDown_Lambda([WeakSelf, WeakV2](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply {
									if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
									auto V2 = WeakV2.Pin(); if (!V2.IsValid()) return FReply::Unhandled();
									FColorPickerArgs Args; Args.bUseAlpha = true;
								#if ENGINE_MAJOR_VERSION >= 5
									Args.InitialColor = V2->VectorValue;
								#else
									Args.InitialColorOverride = V2->VectorValue;
								#endif
									TWeakPtr<FMLPInstanceParamVM, ESPMode::NotThreadSafe> WeakParam = V2;
									Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([WeakSelf, WeakParam](FLinearColor NewColor) {
										auto Self = WeakSelf.Pin(); auto Param = WeakParam.Pin();
										if (Self.IsValid() && Param.IsValid()) Self->OnInstanceVectorChanged(Param, NewColor);
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
								auto V2 = WeakV2.Pin(); if (!V2.IsValid()) return FText::GetEmpty();
								const FLinearColor& C = V2->VectorValue;
								return FText::FromString(FString::Printf(TEXT("R:%.2f G:%.2f B:%.2f A:%.2f"), C.R, C.G, C.B, C.A));
							})
							.Font(FMLPTheme::FontSmall()).ColorAndOpacity(FMLPTheme::Muted())
						];
				}
				case (int32)EMLPParameterType::Texture:
				{
					return SNew(SButton)
						.Text_Lambda([WeakVM]() -> FText {
							auto V = WeakVM.Pin();
							if (V.IsValid() && V->TextureValue.IsValid()) return FText::FromString(V->TextureValue->GetName());
							return FText::FromString(TEXT("(无)"));
						})
						.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
						.ContentPadding(FMargin(2, 0)).HAlign(HAlign_Left)
						.OnClicked_Lambda([WeakSelf, WeakVM]() -> FReply {
							auto Self = WeakSelf.Pin(); if (!Self.IsValid()) return FReply::Handled();
							FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
							FAssetPickerConfig Config;
						#if ENGINE_MAJOR_VERSION >= 5
							Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
						#else
							Config.Filter.ClassNames.Add(TEXT("Texture2D"));
						#endif
							auto WeakV2 = WeakVM;
							TWeakPtr<SMaterialInstanceGroupPanel, ESPMode::NotThreadSafe> WeakSelf2 = WeakSelf;
							Config.OnAssetSelected = FOnAssetSelected::CreateLambda([WeakSelf2, WeakV2](const FAssetData& AssetData) -> void {
								auto Self2 = WeakSelf2.Pin(); auto V2 = WeakV2.Pin();
								if (!Self2.IsValid() || !V2.IsValid() || !AssetData.IsValid()) { FSlateApplication::Get().DismissAllMenus(); return; }
								UObject* Asset = AssetData.GetAsset();
								if (!Asset) { const FString Path = AssetData.PackageName.ToString() / AssetData.AssetName.ToString(); Asset = LoadObject<UObject>(nullptr, *Path); }
								if (Asset) Self2->OnInstanceTextureChanged(V2, Asset);
								FSlateApplication::Get().DismissAllMenus();
							});
							Config.bAllowNullSelection = false;
							Config.InitialAssetViewType = EAssetViewType::List;
							TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
							FSlateApplication::Get().PushMenu(Self->AsShared(), FWidgetPath(), Picker, FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
							return FReply::Handled();
						});
				}
				case (int32)EMLPParameterType::StaticBool:
				case (int32)EMLPParameterType::StaticSwitch:
				{
					auto WeakV2 = WeakVM;
					return SNew(SCheckBox)
						.IsChecked_Lambda([WeakV2]() -> ECheckBoxState {
							auto V2 = WeakV2.Pin();
							return (V2.IsValid() && V2->BoolValue) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
						.OnCheckStateChanged_Lambda([WeakSelf, WeakV2](ECheckBoxState State) {
							auto Self = WeakSelf.Pin(); auto V2 = WeakV2.Pin();
							if (Self.IsValid() && V2.IsValid()) Self->OnInstanceBoolChanged(V2, State == ECheckBoxState::Checked);
						});
				}
				default:
					return SNew(STextBlock).Text(FText::FromString(TEXT("(不支持)"))).Font(FMLPTheme::FontSmall()).ColorAndOpacity(FMLPTheme::Muted());
				}
			}();

			// Group dropdown — lets user move this param to another group (AssetUserData only).
			// SComboBox holds a raw ptr to its options source, so it must outlive the combo box;
			// use the panel member CachedGroupNames (refreshed each rebuild in RebuildInstanceContent).

			// Blue-line drop indicator BEFORE this row (insert at this index).
			if (bDropTargetHere && DragOverInsertIndex == RowIdx)
			{
				ContentBox->AddSlot().AutoHeight().Padding(FMargin(2, 0))[ MakeDropIndicator() ];
			}

			TSharedPtr<SHorizontalBox> RowBox;
			ContentBox->AddSlot().AutoHeight().Padding(FMargin(2, 1))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor::Transparent)
				.Padding(FMargin(0.f))
				// Right-click → context menu (copy name / toggle override). Left clicks pass through
				// to the row's children (checkboxes, value editors, etc.) unimpeded.
				.OnMouseButtonDown_Lambda([WeakSelf, P](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
				{
					if (MouseEvent.GetEffectingButton() != EKeys::RightMouseButton) return FReply::Unhandled();
					auto Self = WeakSelf.Pin();
					if (!Self.IsValid() || !P.IsValid()) return FReply::Unhandled();
					TSharedRef<SWidget> Menu = Self->BuildRowContextMenu(P);
					FSlateApplication::Get().PushMenu(
						Self->AsShared(),
						FWidgetPath(),
						Menu,
						FSlateApplication::Get().GetCursorPos(),
						FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
					return FReply::Handled();
				})
				[
					SAssignNew(RowBox, SHorizontalBox)
				// Drag handle (:: icon — drag to insert between any rows or move across groups)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 4, 0))
				[
					SNew(SInstanceParamDragSource)
					.Param(P)
					[
						SNew(SBox)
						.WidthOverride(20.f).HeightOverride(20.f)
						.HAlign(HAlign_Center).VAlign(VAlign_Center)
							.ToolTipText(LOCTEXT("DragHandleTT", "拖拽到任意位置插入(可跨组)"))
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("::")))
							.Font(FMLPTheme::FontSmall())
							.ColorAndOpacity(FMLPTheme::Muted())
						]
					]
				]
				// Override checkbox
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([WeakVM]() -> ECheckBoxState {
						auto V = WeakVM.Pin();
						return (V.IsValid() && V->bOverridden) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([WeakSelf, WeakVM](ECheckBoxState) {
						auto Self = WeakSelf.Pin(); auto V = WeakVM.Pin();
						if (Self.IsValid() && V.IsValid()) Self->OnToggleOverride(V);
					})
					.ToolTipText(LOCTEXT("OverrideTT", "勾选=覆盖实例值"))
				]
				// Duplicate-name warning badge (⚠) — shown only when another param shares this name.
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(2, 0, 0, 0))
				[
					SNew(SBox)
					.Visibility_Lambda([WeakVM]() -> EVisibility {
						auto V = WeakVM.Pin();
						return (V.IsValid() && V->bHasDuplicateName) ? EVisibility::Visible : EVisibility::Hidden;
					})
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("⚠")))
						.Font(FMLPTheme::FontBody())
						.ColorAndOpacity(FMLPTheme::Warning())
						.ToolTipText(LOCTEXT("DupNameTTInst", "重复参数名:父材质里有其他参数同名,运行时只有一个生效。"))
					]
				]
				// Parameter name
				+ SHorizontalBox::Slot().FillWidth(0.30f).VAlign(VAlign_Center).Padding(FMargin(4, 0))
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
				// Value editor
				+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center)
				[
					ValueEditor
				]
				// Group selector (move param to another group). Wrap the combo content in a
				// semi-opaque SBorder so the text (light) is readable regardless of theme — the
				// default combo button background is unpredictable across themes.
				+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(FMargin(4, 0))
				[
					SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&CachedGroupNames)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item) -> TSharedRef<SWidget> {
						// Combo popup background follows the editor theme (dark on UE4.26), so the
						// item text must be LIGHT — black text was invisible against the dark popup.
						return SNew(STextBlock)
							.Text(Item.IsValid() ? FText::FromName(*Item) : FText::GetEmpty())
							.Font(FMLPTheme::FontSmall())
							.ColorAndOpacity(FMLPTheme::Foreground());
					})
					.OnSelectionChanged_Lambda([WeakSelf, WeakVM](TSharedPtr<FName> NewItem, ESelectInfo::Type) {
						auto Self = WeakSelf.Pin(); auto V = WeakVM.Pin();
						if (!Self.IsValid() || !V.IsValid() || !NewItem.IsValid()) return;
						FName NewGroup = *NewItem;
						if (!NewGroup.IsNone() && NewGroup != V->EffectiveGroup)
						{
							Self->OnParamMovedToGroup(V, NewGroup);
						}
					})
					.ComboBoxStyle(MLP_STYLE::Get(), "ComboBox")
					.ForegroundColor(FLinearColor::White)
					.ContentPadding(FMargin(4, 2))
					[
						SNew(SBorder)
						.BorderBackgroundColor(FLinearColor(0.15f, 0.15f, 0.15f, 1.0f))
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.Padding(FMargin(4, 1))
						[
							SNew(STextBlock)
							.Text_Lambda([WeakVM]() -> FText {
								auto V = WeakVM.Pin();
								return V.IsValid() ? FText::FromName(V->EffectiveGroup) : FText::GetEmpty();
							})
							.Font(FMLPTheme::FontSmall())
							.ColorAndOpacity(FLinearColor::White)
						]
					]
				]
				// Override indicator
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
			]  // end SBorder (row wrapper)
			];  // end AddSlot

			// Register this row's outer widget for row-level drop hit-testing + indicator.
			if (RowBox.IsValid())
			{
				ParamRowWidgets.Add(TPair<FName, TWeakPtr<SWidget>>(GroupName, RowBox.ToSharedRef()));
			}
			++RowIdx;
		}

		// Blue-line drop indicator at the GROUP END (insert at index == ParamCount: append).
		if (!bCollapsed && bDropTargetHere && DragOverInsertIndex == RowIdx)
		{
			ContentBox->AddSlot().AutoHeight().Padding(FMargin(2, 0))[ MakeDropIndicator() ];
		}

		// Empty group hint — show when a group has no params (e.g. just created).
		if (!bCollapsed && Group->Parameters.Num() == 0)
		{
			ContentBox->AddSlot().AutoHeight().Padding(FMargin(8, 2))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyGroupHint", "该分组没有参数(用参数行的组下拉把参数移过来)"))
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			];
		}
	}
}

// ============================================================================
// Handlers
// ============================================================================

FReply SMaterialInstanceGroupPanel::OnRefreshClicked()
{
	ResolveTarget();
	PullFromInstance();
	RebuildInstanceContent();
	return FReply::Handled();
}

void SMaterialInstanceGroupPanel::OnParamMovedToGroup(TSharedPtr<FMLPInstanceParamVM> Param, FName NewGroup)
{
	if (!Param.IsValid() || !TargetInstance.IsValid() || !GroupData.IsValid()) return;

	UMaterialInstanceGroupData* GD = GroupData.Get();
	GD->SetParamGroup(Param->Name, NewGroup);
	GD->Save(TargetInstance.Get());

	Param->EffectiveGroup = NewGroup;
	PullFromInstance();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnParamInsertedAt(TSharedPtr<FMLPInstanceParamVM> Param, FName TargetGroup, int32 InsertIdx)
{
	if (!Param.IsValid() || !TargetInstance.IsValid() || !GroupData.IsValid()) return;
	if (TargetGroup.IsNone()) return;

	const FName DragName = Param->Name;
	UMaterialInstanceGroupData* GD = GroupData.Get();

	// The target group's parameters IN CURRENT DISPLAY ORDER (Groups was built by PullFromInstance
	// sorted by ParamSort). This is the ordering the user sees, so InsertIdx is relative to it.
	TArray<FName> OrderInGroup;
	for (const auto& G : Groups)
	{
		if (!G.IsValid() || G->Name != TargetGroup) continue;
		for (const auto& P : G->Parameters)
		{
			if (P.IsValid()) OrderInGroup.Add(P->Name);
		}
		break;
	}

	// If the dragged param is ALREADY in this group, remove it first and adjust InsertIdx so the
	// index stays semantically correct (an index after the old position shifts down by one).
	const int32 ExistingIdx = OrderInGroup.RemoveSingle(DragName);
	const bool bWasInGroup = (ExistingIdx != INDEX_NONE);
	if (bWasInGroup && InsertIdx > ExistingIdx)
	{
		--InsertIdx;
	}

	// Clamp + insert.
	InsertIdx = FMath::Clamp(InsertIdx, 0, OrderInGroup.Num());
	OrderInGroup.Insert(DragName, InsertIdx);

	// If moving across groups, update the param's group mapping.
	if (Param->EffectiveGroup != TargetGroup)
	{
		GD->SetParamGroup(DragName, TargetGroup);
		Param->EffectiveGroup = TargetGroup;
	}

	// Renumber ParamSort for this group so the new order is contiguous (0,1,2,...). This is what
	// PullFromInstance's sort reads next rebuild.
	for (int32 i = 0; i < OrderInGroup.Num(); ++i)
	{
		GD->SetParamSort(OrderInGroup[i], i);
	}

	GD->Save(TargetInstance.Get());
	PullFromInstance();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnGroupSortChanged(FName GroupName, int32 NewPriority)
{
	if (!GroupData.IsValid()) return;

	// Update GroupOrder: rebuild with new priority.
	TArray<FName> Order;
	for (const auto& G : Groups) Order.Add(G->Name);
	// Move the changed group to its new priority slot.
	Order.Remove(GroupName);
	Order.Insert(GroupName, FMath::Clamp(NewPriority, 0, Order.Num()));

	GroupData.Get()->SetGroupOrder(Order);
	GroupData.Get()->Save(TargetInstance.Get());

	PullFromInstance();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnGroupRenamed(FName OldName, const FText& NewName, ETextCommit::Type CommitType)
{
	// Only commit on Enter / focus loss (not on every keystroke).
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
	if (!GroupData.IsValid()) return;

	const FString NewStr = NewName.ToString().TrimStartAndEnd();
	if (NewStr.IsEmpty() || NewStr == OldName.ToString()) return;

	FName NewNameF(*NewStr);
	if (NewNameF == OldName) return;

	GroupData.Get()->RenameGroup(OldName, NewNameF);
	GroupData.Get()->Save(TargetInstance.Get());

	PullFromInstance();
	RebuildInstanceContent();
}

FReply SMaterialInstanceGroupPanel::OnAddGroupClicked()
{
	if (!GroupData.IsValid() || !TargetInstance.IsValid()) return FReply::Handled();

	// Generate a unique new group name ("Group1", "Group2", ...).
	FName NewName;
	int32 Suffix = 1;
	TArray<FName> Existing;
	for (const auto& G : Groups) Existing.Add(G->Name);
	do
	{
		NewName = FName(*FString::Printf(TEXT("Group%d"), Suffix++));
	} while (Existing.Contains(NewName) || GroupData->GetGroupOrder().Contains(NewName));

	// Append to the custom group order so the new group is registered (even though it has no
	// params yet, it'll appear as an empty group the user can move params into).
	TArray<FName> Order = GroupData->GetGroupOrder();
	Order.Add(NewName);
	GroupData.Get()->SetGroupOrder(Order);
	GroupData.Get()->Save(TargetInstance.Get());

	PullFromInstance();
	RebuildInstanceContent();
	return FReply::Handled();
}

FReply SMaterialInstanceGroupPanel::OnLocateAssetClicked()
{
	// Select + focus the material instance in the Content Browser so the user can find it
	// (e.g. to drag into another material, check the path, or open its parent material).
	if (TargetInstance.IsValid())
	{
		FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<UObject*> Assets;
		Assets.Add(TargetInstance.Get());
		CB.Get().SyncBrowserToAssets(Assets);
	}
	return FReply::Handled();
}

FReply SMaterialInstanceGroupPanel::OnResetAllOverridesClicked()
{
	if (!TargetInstance.IsValid()) return FReply::Handled();

	// Count how many overrides exist, for the confirmation prompt + early-out.
	int32 OverrideCount = 0;
	for (const auto& P : AllParams)
	{
		if (P.IsValid() && P->bOverridden) ++OverrideCount;
	}
	if (OverrideCount == 0) return FReply::Handled();  // nothing to do

	// Confirm — this is destructive (reverts all instance overrides to parent defaults).
	const FText ConfirmMsg = FText::Format(
		LOCTEXT("ResetAllConf", "确定要清除全部 {0} 个参数覆盖吗?\n所有覆盖值将回退到父材质默认值,此操作可撤销。"),
		FText::AsNumber(OverrideCount));
	if (FMessageDialog::Open(EAppMsgType::YesNo, ConfirmMsg) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("重置全部覆盖")));
	// Ensure the instance is transactional so Modify() snapshots it into the undo buffer.
	// Loaded MIs don't always carry RF_Transactional, in which case edits wouldn't be undoable.
	MI->SetFlags(RF_Transactional);
	MI->Modify();

	// Clear every typed override array.
	MI->ScalarParameterValues.Reset();
	MI->VectorParameterValues.Reset();
	MI->TextureParameterValues.Reset();

	// Static switches live in a parameter set — rebuild it without this instance's overrides.
	FStaticParameterSet ParamSet;
	MI->GetStaticParameterValues(ParamSet);
	ParamSet.StaticSwitchParameters.Reset();
	MI->UpdateStaticPermutation(ParamSet);

	// Refresh the VMs so the UI reflects the cleared state.
	for (const auto& P : AllParams)
	{
		if (P.IsValid()) P->bOverridden = false;
	}

	MI->PostEditChange();
	MI->MarkPackageDirty();
	PullFromInstance();
	RebuildInstanceContent();
	return FReply::Handled();
}

FReply SMaterialInstanceGroupPanel::OnEnableAllOverridesClicked()
{
	if (!TargetInstance.IsValid()) return FReply::Handled();

	// Count params that aren't yet overridden (only those need enabling).
	int32 ToEnable = 0;
	for (const auto& P : AllParams)
	{
		if (P.IsValid() && !P->bOverridden) ++ToEnable;
	}
	if (ToEnable == 0) return FReply::Handled();  // nothing to do

	const FText ConfirmMsg = FText::Format(
		LOCTEXT("EnableAllConf", "确定要用当前值为 {0} 个参数启用覆盖吗?\n此操作可撤销。"),
		FText::AsNumber(ToEnable));
	if (FMessageDialog::Open(EAppMsgType::YesNo, ConfirmMsg) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("全部启用覆盖")));
	MI->SetFlags(RF_Transactional);
	MI->Modify();

	// Build a combined static-switch set once (avoid per-param UpdateStaticPermutation).
	FStaticParameterSet ParamSet;
	MI->GetStaticParameterValues(ParamSet);

	for (const auto& P : AllParams)
	{
		if (!P.IsValid() || P->bOverridden) continue;

		switch (P->Type)
		{
		case (int32)EMLPParameterType::Scalar:
		{
			FScalarParameterValue SV;
			SV.ParameterInfo = FMaterialParameterInfo(P->Name);
			SV.ParameterValue = P->ScalarValue;
			SV.ExpressionGUID = P->ExpressionGUID;
			MI->ScalarParameterValues.Add(SV);
			break;
		}
		case (int32)EMLPParameterType::Vector:
		{
			FVectorParameterValue VV;
			VV.ParameterInfo = FMaterialParameterInfo(P->Name);
			VV.ParameterValue = P->VectorValue;
			VV.ExpressionGUID = P->ExpressionGUID;
			MI->VectorParameterValues.Add(VV);
			break;
		}
		case (int32)EMLPParameterType::Texture:
		{
			FTextureParameterValue TV;
			TV.ParameterInfo = FMaterialParameterInfo(P->Name);
			TV.ParameterValue = P->TextureValue.Get();
			TV.ExpressionGUID = P->ExpressionGUID;
			MI->TextureParameterValues.Add(TV);
			break;
		}
		case (int32)EMLPParameterType::StaticBool:
		case (int32)EMLPParameterType::StaticSwitch:
		{
			// Add/update the override entry in the combined set.
			bool bFound = false;
			for (FStaticSwitchParameter& SP : ParamSet.StaticSwitchParameters)
			{
				if (SP.ParameterInfo.Name == P->Name)
				{
					SP.bOverride = true; SP.Value = P->BoolValue; bFound = true; break;
				}
			}
			if (!bFound)
			{
				ParamSet.StaticSwitchParameters.Add(FStaticSwitchParameter(
					FMaterialParameterInfo(P->Name), P->BoolValue, true, P->ExpressionGUID));
			}
			break;
		}
		default: break;
		}
		P->bOverridden = true;
	}

	MI->UpdateStaticPermutation(ParamSet);
	MI->PostEditChange();
	MI->MarkPackageDirty();
	PullFromInstance();
	RebuildInstanceContent();
	return FReply::Handled();
}

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildRowContextMenu(TSharedPtr<FMLPInstanceParamVM> Param)
{
	// Capture the param weakly so the menu actions don't keep it alive past the panel.
	TWeakPtr<FMLPInstanceParamVM> WeakParam = Param;
	TWeakPtr<SMaterialInstanceGroupPanel, ESPMode::NotThreadSafe> WeakSelf = StaticCastSharedRef<SMaterialInstanceGroupPanel>(AsShared());

	FMenuBuilder MenuBuilder(true, nullptr);

	// Copy parameter name to the OS clipboard.
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CtxCopyName", "复制参数名"),
		LOCTEXT("CtxCopyNameTT", "将此参数名复制到剪贴板"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([WeakParam]()
		{
			auto P = WeakParam.Pin();
			if (P.IsValid())
			{
				FPlatformApplicationMisc::ClipboardCopy(*P->Name.ToString());
			}
		}))
	);

	// "Move to group" submenu — lists every group except the param's current one; clicking one
	// moves the param (writes AssetUserData only). Snapshot the group names now so the submenu
	// is stable even if the list is rebuilt while the menu is open.
	TArray<FName> AllGroupNames;
	for (const auto& G : Groups)
	{
		if (G.IsValid()) AllGroupNames.Add(G->Name);
	}
	const FName CurrentGroup = Param.IsValid() ? Param->EffectiveGroup : NAME_None;
	MenuBuilder.AddSubMenu(
		LOCTEXT("CtxMoveToGroup", "移动到分组"),
		LOCTEXT("CtxMoveToGroupTT", "将此参数移到另一个分组"),
		FNewMenuDelegate::CreateLambda([WeakSelf, WeakParam, AllGroupNames, CurrentGroup](FMenuBuilder& SubMenu)
		{
			for (const FName& GroupName : AllGroupNames)
			{
				if (GroupName == CurrentGroup) continue;  // hide the current group
				const FText GroupLabel = FText::FromName(GroupName);
				SubMenu.AddMenuEntry(
					GroupLabel,
					FText::GetEmpty(),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([WeakSelf, WeakParam, GroupName]()
					{
						auto Self = WeakSelf.Pin(); auto P = WeakParam.Pin();
						if (Self.IsValid() && P.IsValid())
						{
							Self->OnParamMovedToGroup(P, GroupName);
						}
					}))
				);
			}
		}),
		false,
		FSlateIcon(),
		true  // close window after selection (so the menu dismisses once a group is picked)
	);

	MenuBuilder.AddMenuSeparator();

	// Toggle / reset override. Label adapts to the current state.
	const bool bOverridden = Param.IsValid() && Param->bOverridden;
	MenuBuilder.AddMenuEntry(
		bOverridden ? LOCTEXT("CtxResetOverride", "重置覆盖(恢复父材质值)") : LOCTEXT("CtxEnableOverride", "启用覆盖"),
		bOverridden ? LOCTEXT("CtxResetOverrideTT", "移除此实例对该参数的覆盖,回退到父材质默认值") : LOCTEXT("CtxEnableOverrideTT", "用当前值覆盖父材质默认值"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([WeakSelf, WeakParam]()
		{
			auto Self = WeakSelf.Pin(); auto P = WeakParam.Pin();
			if (Self.IsValid() && P.IsValid()) Self->OnToggleOverride(P);
		}))
	);

	return MenuBuilder.MakeWidget();
}

void SMaterialInstanceGroupPanel::OnToggleOverride(TSharedPtr<FMLPInstanceParamVM> Param)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("切换参数覆盖")));
	// Ensure the instance is transactional so Modify() snapshots it into the undo buffer.
	// Loaded MIs don't always carry RF_Transactional, in which case edits wouldn't be undoable.
	MI->SetFlags(RF_Transactional);
	MI->Modify();

	if (Param->bOverridden)
	{
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
			SetStaticSwitchOverride(Param, false, Param->BoolValue);
			break;
		default: break;
		}
		Param->bOverridden = false;
	}
	else
	{
		switch (Param->Type)
		{
		case (int32)EMLPParameterType::Scalar:
			{ FScalarParameterValue SV; SV.ParameterInfo = FMaterialParameterInfo(Param->Name); SV.ParameterValue = Param->ScalarValue; SV.ExpressionGUID = Param->ExpressionGUID; MI->ScalarParameterValues.Add(SV); }
			break;
		case (int32)EMLPParameterType::Vector:
			{ FVectorParameterValue VV; VV.ParameterInfo = FMaterialParameterInfo(Param->Name); VV.ParameterValue = Param->VectorValue; VV.ExpressionGUID = Param->ExpressionGUID; MI->VectorParameterValues.Add(VV); }
			break;
		case (int32)EMLPParameterType::Texture:
			{ FTextureParameterValue TV; TV.ParameterInfo = FMaterialParameterInfo(Param->Name); TV.ParameterValue = Param->TextureValue.Get(); TV.ExpressionGUID = Param->ExpressionGUID; MI->TextureParameterValues.Add(TV); }
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
	PullFromInstance();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceScalarChanged(TSharedPtr<FMLPInstanceParamVM> Param, float NewValue, ETextCommit::Type)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->ScalarValue = NewValue;
	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("修改标量参数")));
	// Ensure the instance is transactional so Modify() snapshots it into the undo buffer.
	// Loaded MIs don't always carry RF_Transactional, in which case edits wouldn't be undoable.
	MI->SetFlags(RF_Transactional);
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->ScalarParameterValues)
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = NewValue; bFound = true; break; }
	if (!bFound)
	{
		FScalarParameterValue SV; SV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		SV.ParameterValue = NewValue; SV.ExpressionGUID = Param->ExpressionGUID;
		MI->ScalarParameterValues.Add(SV); Param->bOverridden = true;
	}
	MI->PostEditChange(); MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceVectorChanged(TSharedPtr<FMLPInstanceParamVM> Param, FLinearColor NewColor)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->VectorValue = NewColor;
	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("修改向量参数")));
	// Ensure the instance is transactional so Modify() snapshots it into the undo buffer.
	// Loaded MIs don't always carry RF_Transactional, in which case edits wouldn't be undoable.
	MI->SetFlags(RF_Transactional);
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->VectorParameterValues)
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = NewColor; bFound = true; break; }
	if (!bFound)
	{
		FVectorParameterValue VV; VV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		VV.ParameterValue = NewColor; VV.ExpressionGUID = Param->ExpressionGUID;
		MI->VectorParameterValues.Add(VV); Param->bOverridden = true;
	}
	MI->PostEditChange(); MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceTextureChanged(TSharedPtr<FMLPInstanceParamVM> Param, UObject* NewTexture)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->TextureValue = Cast<UTexture>(NewTexture);
	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("修改纹理参数")));
	// Ensure the instance is transactional so Modify() snapshots it into the undo buffer.
	// Loaded MIs don't always carry RF_Transactional, in which case edits wouldn't be undoable.
	MI->SetFlags(RF_Transactional);
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->TextureParameterValues)
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = Cast<UTexture>(NewTexture); bFound = true; break; }
	if (!bFound)
	{
		FTextureParameterValue TV; TV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		TV.ParameterValue = Cast<UTexture>(NewTexture); TV.ExpressionGUID = Param->ExpressionGUID;
		MI->TextureParameterValues.Add(TV); Param->bOverridden = true;
	}
	MI->PostEditChange(); MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceBoolChanged(TSharedPtr<FMLPInstanceParamVM> Param, bool bNewValue)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->BoolValue = bNewValue;
	SetStaticSwitchOverride(Param, true, bNewValue);
}

void SMaterialInstanceGroupPanel::SetStaticSwitchOverride(TSharedPtr<FMLPInstanceParamVM> Param, bool bOverride, bool bNewValue)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	UMaterialInstance* MI = TargetInstance.Get();

	const FScopedTransaction T(FText::FromString(TEXT("修改静态开关参数")));
	// Ensure the instance is transactional so Modify() snapshots it into the undo buffer.
	// Loaded MIs don't always carry RF_Transactional, in which case edits wouldn't be undoable.
	MI->SetFlags(RF_Transactional);
	MI->Modify();

	FStaticParameterSet ParamSet;
	MI->GetStaticParameterValues(ParamSet);

	bool bChanged = false;
	if (bOverride)
	{
		// Add or update the override entry.
		for (FStaticSwitchParameter& SP : ParamSet.StaticSwitchParameters)
		{
			if (SP.ParameterInfo.Name == Param->Name)
			{
				SP.bOverride = true; SP.Value = bNewValue; bChanged = true; break;
			}
		}
		if (!bChanged)
		{
			FStaticSwitchParameter SP(FMaterialParameterInfo(Param->Name), bNewValue, true, Param->ExpressionGUID);
			ParamSet.StaticSwitchParameters.Add(SP); bChanged = true;
		}
	}
	else
	{
		// Remove the override entry entirely (matches the RemoveAll behavior used for
		// scalar/vector/texture overrides — "un-override" = revert to parent default).
		const int32 Before = ParamSet.StaticSwitchParameters.Num();
		ParamSet.StaticSwitchParameters.RemoveAll([&](const FStaticSwitchParameter& SP)
		{
			return SP.ParameterInfo.Name == Param->Name;
		});
		bChanged = (ParamSet.StaticSwitchParameters.Num() != Before);
	}
	if (bChanged)
	{
		MI->UpdateStaticPermutation(ParamSet);
		Param->bOverridden = bOverride;
	}
	MI->PostEditChange(); MI->MarkPackageDirty();
	RebuildInstanceContent();
}

// ============================================================================
// Status
// ============================================================================

FText SMaterialInstanceGroupPanel::GetInstanceName() const
{
	UMaterialInstance* MI = TargetInstance.Get();
	return MI ? FText::FromString(FString::Printf(TEXT("实例: %s"), *MI->GetName())) : FText::GetEmpty();
}

FText SMaterialInstanceGroupPanel::GetStatusText() const
{
	if (!TargetInstance.IsValid()) return LOCTEXT("NS", "未选择材质实例");

	// When a search is active, show how many params match out of the total.
	const FString Query = SearchText.ToString().TrimStartAndEnd();
	if (!Query.IsEmpty())
	{
		int32 Matched = 0;
		for (const auto& P : AllParams)
		{
			if (P.IsValid() && P->Name.ToString().Contains(Query)) ++Matched;
		}
		return FText::FromString(FString::Printf(TEXT("匹配 %d / %d 参数"), Matched, AllParams.Num()));
	}
	return FText::FromString(FString::Printf(TEXT("%d 参数 | %d 分组"), AllParams.Num(), Groups.Num()));
}

#undef LOCTEXT_NAMESPACE
