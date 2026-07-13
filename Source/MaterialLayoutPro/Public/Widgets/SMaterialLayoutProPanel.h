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
class SWidget;

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

	/** Notify the bound material editor that the material's expressions changed externally
	  * (group/sort/value edits made by this panel directly on Material->Expressions). This is
	  * REQUIRED for changes to persist: the material editor edits a transient preview copy
	  * (FMaterialEditor::Material) and only copies it back to the on-disk OriginalMaterial when
	  * its internal bMaterialDirty flag is set. Calling UpdateMaterialAfterGraphChange() sets
	  * that flag (via SetMaterialDirty), so a subsequent Ctrl+S / close actually saves the
	  * edits. Without this, edits appear to apply but are silently lost on save/reopen. */
	void NotifyMaterialEditorChanged();

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();
	/** Build the "更多" dropdown menu content (grouping / cleanup / import-export / templates). */
	TSharedRef<SWidget> BuildMoreMenu();
	void RefreshParameters();
	void RebuildTree();

	// --- Selection (multi-select) ---
	void SelectParam(TSharedPtr<FMLPParamVM> Param, bool bCtrl, bool bShift);
	void JumpToParam(TSharedPtr<FMLPParamVM> Param);
	void ClearSelection();
	bool IsSelected(TSharedPtr<FMLPParamVM> Param) const;

	// --- Drag-drop reorder ---
	void OnParamDropped(TSharedPtr<FMLPParamVM> DraggedParam, TSharedPtr<FMLPParamVM> TargetParam, bool bInsertBefore);

	/** Build the right-click context menu for a parameter row (copy name / jump / move to group). */
	TSharedRef<SWidget> BuildRowContextMenu(TSharedPtr<FMLPParamVM> Param);
	/** Move a param to a different group (writes the expression Group + pushes back to material). */
	void MoveParamToGroup(TSharedPtr<FMLPParamVM> Param, FName NewGroup);
	/** Show a dialog listing the downstream consumers + upstream inputs of a parameter. */
	void ShowUsageLinks(TSharedPtr<FMLPParamVM> Param);

	// --- Search ---
	void OnSearchChanged(const FText& NewText);
	bool PassesFilter(const TSharedPtr<FMLPParamVM>& Param) const;

	// --- Collapse/expand ---
	void OnToggleGroupCollapsed(FName GroupName);
	bool IsGroupCollapsed(FName GroupName) const;
	FReply OnCollapseAllGroupsClicked();
	FReply OnExpandAllGroupsClicked();

	// --- Status ---
	FText GetTargetMaterialName() const;
	FText GetStatusText() const;

	// --- Toolbar handlers ---
	FReply OnRefreshClicked();
	/** Locate + select the target material in the Content Browser. */
	FReply OnLocateAssetClicked();
	FReply OnArchiveUnusedClicked();
	FReply OnDeleteUnusedClicked();
	FReply OnAutoGroupClicked();
	/** Open Project Settings → Material Layout Pro to edit the auto-group prefix rules. */
	FReply OnAutoGroupRulesClicked();
	/** Renumber every parameter's SortPriority to its position within its group (0,1,2,...),
	 *  so priorities are contiguous after manual edits leave them fragmented. */
	FReply OnResetAllPrioritiesClicked();
	FReply OnExportClicked();
	FReply OnImportClicked();
	/** Save the current material's {ParamName -> Group} mapping to a .json template for reuse. */
	FReply OnSaveGroupTemplateClicked();
	/** Load a group template .json and apply its {Name -> Group} to this material's params
	 *  (preview + confirm, like CSV import). */
	FReply OnApplyGroupTemplateClicked();
	FReply OnSortWorkbenchClicked();
	FReply OnParameterEditorClicked();
	FReply OnGroupByCommentClicked();
	FReply OnApplyChangesClicked();
	FReply OnSetGroupForSelectionClicked();

	// NOTE: instance-mode panel logic lives in SMaterialInstanceGroupPanel, now registered as
	// a dockable tab in the material instance editor (FMaterialLayoutProModule::RegisterInstanceSidebar).
	// This material-editor sidebar no longer hosts any instance UI.

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
	/** Names of groups the user has collapsed (session-only view state, not persisted). */
	TSet<FName> CollapsedGroups;

	// --- Polling ---
	TOptional<double> LastPollTime;
	double SyncCooldownUntil = 0.0;
};
