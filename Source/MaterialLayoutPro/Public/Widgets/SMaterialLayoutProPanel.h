#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class UMaterialExpressionParameter;
class FMLPSession;
struct FMLPParamVM;
class SEditableTextBox;
class SWidgetSwitcher;
class SVerticalBox;
class SScrollBox;

/**
 * Main dockable panel — dual-pane (wide) / accordion (narrow) with adaptive layout.
 *
 * Data flow: UMaterial → FMLPSession (snapshot) → hand-written Slate. Value edits
 * land on the VM (not the engine object); "应用更改" flushes via Session::PushDirty().
 */
class MATERIALLAYOUTPRO_API SMaterialLayoutProPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayoutProPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMaterialLayoutProPanel() override;

	// SCompoundWidget — detect size changes for adaptive layout.
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	void OnSelectionChanged(UObject* Selection);

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();

	void RefreshParameters();

	// --- Selection ---
	void SelectParam(TSharedPtr<FMLPParamVM> Param);

	// --- Layout builders ---
	TSharedRef<SWidget> BuildWideMode();
	TSharedRef<SWidget> BuildNarrowMode();
	void RebuildLeftTree();
	void RebuildNarrowList();
	void RebuildRightDetail();

	// --- Adaptive ---
	void UpdateLayoutMode(float Width);

	// --- Status ---
	FText GetTargetMaterialName() const;
	FText GetStatusText() const;

	// --- Toolbar handlers ---
	FReply OnRefreshClicked();
	FReply OnSelectMaterialClicked();
	FReply OnOpenMaterialEditorClicked();
	FReply OnArchiveUnusedClicked();
	FReply OnDeleteUnusedClicked();
	FReply OnAutoGroupClicked();
	FReply OnBulkRenameClicked();
	FReply OnExportClicked();
	FReply OnImportClicked();
	FReply OnSortWorkbenchClicked();
	FReply OnParameterEditorClicked();
	FReply OnGroupByCommentClicked();
	FReply OnApplyChangesClicked();

	// --- Data ---
	TSharedPtr<FMLPSession> Session;
	TWeakObjectPtr<UMaterial> TargetMaterial;
	TWeakObjectPtr<UMaterialInstance> TargetMaterialInstance;
	TSharedPtr<FMLPParamVM> SelectedParam;

	// --- Layout state ---
	bool bIsWideMode = true;
	float CachedWidth = 0.f;

	// --- UI containers ---
	TSharedPtr<SWidgetSwitcher> ModeSwitcher;
	TSharedPtr<SVerticalBox> LeftTreeContainer;
	TSharedPtr<SVerticalBox> RightDetailContainer;
	TSharedPtr<SVerticalBox> NarrowListContainer;
	TSharedPtr<SEditableTextBox> SearchBox;

	// --- Search filter ---
	FString SearchText;
	void OnSearchChanged(const FText& NewText);
	bool PassesFilter(const TSharedPtr<FMLPParamVM>& Param) const;
};
