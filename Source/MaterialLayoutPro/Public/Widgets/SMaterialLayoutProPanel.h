#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class UMaterialExpressionParameter;
class IMaterialEditor;
class FMLPSession;
struct FMLPParamVM;
class SEditableTextBox;
class SVerticalBox;

class MATERIALLAYOUTPRO_API SMaterialLayoutProPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayoutProPanel) {}
		SLATE_ARGUMENT(TWeakPtr<IMaterialEditor>, OwningMaterialEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMaterialLayoutProPanel() override;

	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	void BindToMaterialEditor(TWeakPtr<IMaterialEditor> InEditor);
	void OnMaterialChangedBySession();
	void OnSelectionChanged(UObject* Selection);
	void ResolveTargetMaterial();

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();
	void RefreshParameters();
	void RebuildTree();

	// --- Selection (multi-select) ---
	void SelectParam(TSharedPtr<FMLPParamVM> Param, bool bCtrl, bool bShift);
	void JumpToParam(TSharedPtr<FMLPParamVM> Param);
	void ClearSelection();
	bool IsSelected(TSharedPtr<FMLPParamVM> Param) const;

	// --- Drag-drop reorder ---
	void OnParamDropped(TSharedPtr<FMLPParamVM> DraggedParam, TSharedPtr<FMLPParamVM> TargetParam, bool bInsertBefore);

	// --- Search ---
	void OnSearchChanged(const FText& NewText);
	bool PassesFilter(const TSharedPtr<FMLPParamVM>& Param) const;

	// --- Status ---
	FText GetTargetMaterialName() const;
	FText GetStatusText() const;

	// --- Toolbar handlers ---
	FReply OnRefreshClicked();
	FReply OnArchiveUnusedClicked();
	FReply OnDeleteUnusedClicked();
	FReply OnAutoGroupClicked();
	FReply OnExportClicked();
	FReply OnImportClicked();
	FReply OnSortWorkbenchClicked();
	FReply OnParameterEditorClicked();
	FReply OnGroupByCommentClicked();
	FReply OnApplyChangesClicked();
	FReply OnSetGroupForSelectionClicked();

	// --- Data ---
	TSharedPtr<FMLPSession> Session;
	TWeakObjectPtr<UMaterial> TargetMaterial;
	TWeakObjectPtr<UMaterialInstance> TargetMaterialInstance;
	TWeakPtr<IMaterialEditor> OwningMaterialEditor;
	FDelegateHandle MaterialChangedHandle;

	// --- Multi-selection ---
	TArray<TSharedPtr<FMLPParamVM>> SelectedParams;
	TSharedPtr<FMLPParamVM> LastSelectedParam;

	// --- UI containers ---
	TSharedPtr<SVerticalBox> TreeContainer;
	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SEditableTextBox> SetGroupInput;

	// --- Search filter ---
	FString SearchText;

	// --- Polling ---
	TOptional<double> LastPollTime;
	double SyncCooldownUntil = 0.0;
};
