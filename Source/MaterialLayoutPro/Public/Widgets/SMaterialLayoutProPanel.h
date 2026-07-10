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

/**
 * Parameter management panel.
 *
 * Two binding modes:
 *  - Embedded: bound to an IMaterialEditor; reads the material from GetMaterialInterface().
 *    No content-browser selection needed — it tracks the editor that owns this tab.
 *  - Standalone (Nomad tab): tracks GEditor selection (content browser).
 *
 * Single-column compact tree layout: group headers + inline parameter rows
 * (type pill + name + value editor + group + priority + status badge).
 *
 * Data flow: material → FMLPSession (snapshot) → hand-written Slate. Value edits
 * land on the VM; "应用更改" flushes via Session::PushDirty().
 */
class MATERIALLAYOUTPRO_API SMaterialLayoutProPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayoutProPanel) {}
		/** Embedded mode: the material editor this sidebar belongs to. */
		SLATE_ARGUMENT(TWeakPtr<IMaterialEditor>, OwningMaterialEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMaterialLayoutProPanel() override;

	// Poll for open material editors (standalone mode) — needed because 4.26 doesn't
	// broadcast OnMaterialEditorOpened reliably, and content-browser clicks don't fire
	// GEditor object-selection for assets.
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	// --- Selection source (embedded vs standalone) ---
	void BindToMaterialEditor(TWeakPtr<IMaterialEditor> InEditor);
	/** Called when the session writes back to the material — notifies the bound editor. */
	void OnMaterialChangedBySession();
	void OnSelectionChanged(UObject* Selection);
	/** Resolve the current target material from the bound editor or GEditor selection. */
	void ResolveTargetMaterial();

	// --- UI builders ---
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();
	void RefreshParameters();
	void RebuildTree();

	// --- Selection (within the parameter list) ---
	void SelectParam(TSharedPtr<FMLPParamVM> Param);

	// --- Search ---
	void OnSearchChanged(const FText& NewText);
	bool PassesFilter(const TSharedPtr<FMLPParamVM>& Param) const;

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
	/** Embedded mode: the editor this panel is bound to (null in standalone mode). */
	TWeakPtr<IMaterialEditor> OwningMaterialEditor;

	// --- Parameter-list selection ---
	TSharedPtr<FMLPParamVM> SelectedParam;

	// --- UI containers ---
	TSharedPtr<SVerticalBox> TreeContainer;
	TSharedPtr<SEditableTextBox> SearchBox;

	// --- Search filter ---
	FString SearchText;

	// --- Session change handle (to notify the material editor) ---
	FDelegateHandle MaterialChangedHandle;

	// --- Polling (standalone mode) ---
	TOptional<double> LastPollTime;
};
