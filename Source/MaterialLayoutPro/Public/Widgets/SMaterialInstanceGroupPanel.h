#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"

class UMaterial;
class UMaterialInstance;
class UMaterialInstanceGroupData;
class IMaterialEditor;
class SVerticalBox;
class SScrollBox;
class SEditableTextBox;
class FMLPInstanceParamDragDrop;

/** A single parameter in instance mode: name + type + group + override state + value. */
struct FMLPInstanceParamVM
{
	FName Name;
	FName BaseGroup;   // parent material's expression Group (fallback)
	FName EffectiveGroup; // resolved group (custom override or base)
	FGuid ExpressionGUID;
	int32 Type = 0;  // EMLPParameterType
	bool bOverridden = false;

	float ScalarValue = 0.f;
	FLinearColor VectorValue = FLinearColor::White;
	TSoftObjectPtr<UTexture> TextureValue;
	bool BoolValue = false;
};

/**
 * Drop area that is an SBorder subclass. Critical: SBorder PAINTS (has a background brush), so
 * it is registered in Slate's HittestGrid and RELIABLY receives OnDragOver/OnDrop — unlike
 * SCompoundWidget wrappers that don't paint and are skipped by the HittestGrid. This is the
 * outermost widget of the panel content; drag-over/drop are handled here and hit-tested against
 * the panel's group-title geometry (the panel owns FindGroupAtPosition).
 */
class MATERIALLAYOUTPRO_API SInstanceDropArea : public SBorder
{
public:
	DECLARE_DELEGATE_TwoParams(FOnParamDropped, const FVector2D& /*AbsolutePos*/, TSharedPtr<FMLPInstanceParamVM> /*Param*/);
	DECLARE_DELEGATE_OneParam(FOnDragOverPos, const FVector2D& /*AbsolutePos*/);

	SLATE_BEGIN_ARGS(SInstanceDropArea) {}
		SLATE_EVENT(FOnParamDropped, OnDropped)
		SLATE_EVENT(FOnDragOverPos, OnDragOverPos)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

private:
	FOnParamDropped OnDroppedDelegate;
	FOnDragOverPos OnDragOverDelegate;
};

/**
 * Drag source wrapping a parameter row handle. Overrides OnPreviewMouseButtonDown (which fires
 * before children) to request drag detection, and OnDragDetected to begin the drag-drop op.
 */
class MATERIALLAYOUTPRO_API SInstanceParamDragSource : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInstanceParamDragSource) {}
		/** The param this row represents (captured for the drag payload). */
		SLATE_ARGUMENT(TSharedPtr<FMLPInstanceParamVM>, Param)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSharedPtr<FMLPInstanceParamVM> Param;
};

/** A resolved group with its parameters, ready for display. */
struct FMLPInstanceGroupVM
{
	FName Name;
	int32 SortPriority = 0;
	TArray<TSharedPtr<FMLPInstanceParamVM>> Parameters;
};

/**
 * Material instance parameter panel — shows parameters grouped (flat, all groups expanded,
 * like the parent-material sidebar), with inline override value editing.
 *
 * Shown as a dockable tab inside the material instance editor (registered via
 * FMaterialLayoutProModule::RegisterInstanceSidebar). Because the tab's SDockTab holds a strong
 * ref to this widget, the widget lifetime == tab lifetime — row lambdas capture AsShared()
 * weak ptrs safely.
 *
 * Grouping is INDEPENDENT per instance: stored as UMaterialInstanceGroupData (AssetUserData on
 * the MI). Default = parent material's expression Group; user can drag params between groups
 * and the custom mapping is persisted to the MI only (never modifies the parent material).
 */
class MATERIALLAYOUTPRO_API SMaterialInstanceGroupPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialInstanceGroupPanel) {}
		/** The material instance editor this panel is embedded in. Required. */
		SLATE_ARGUMENT(TWeakPtr<IMaterialEditor>, OwningInstanceEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	/** Find which group title bar (if any) contains the given absolute screen position. */
	FName FindGroupAtPosition(const FVector2D& AbsolutePos) const;
	/**
	 * Row-level hit test: resolve the group the cursor is over (via FindGroupAtPosition) and the
	 * insertion index WITHIN that group — i.e. how many of the group's rows are above the cursor.
	 * OutInsertInGroup ranges [0, group.ParamCount]; OutGroup is NAME_None when over no group.
	 */
	void ComputeDropTarget(const FVector2D& AbsolutePos, FName& OutGroup, int32& OutInsertInGroup) const;
	/** Called by SInstanceDropArea when a param is dropped at an absolute position. */
	void HandleParamDropped(const FVector2D& AbsolutePos, TSharedPtr<FMLPInstanceParamVM> Param);
	/** Called by SInstanceDropArea while a param is dragged — updates DragOverGroup highlight. */
	void HandleDragOverPos(const FVector2D& AbsolutePos);
	// --- Editor binding / resolution ---
	void BindToInstanceEditor(TWeakPtr<IMaterialEditor> InEditor);
	void ResolveTarget();

	// --- Data load ---
	/** Scan parent material + fold in instance overrides + apply custom grouping. */
	void PullFromInstance();

	// --- UI build / refresh ---
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();
	TSharedRef<SWidget> BuildInitialContent();
	/** Clear + repopulate the scrollable group list. Called by every state-changing handler. */
	void RebuildInstanceContent();
	/** Build the group sections (title bar + rows) into the given vertical box. */
	void BuildGroupSections(TSharedRef<SVerticalBox> ContentBox);
	/** Case-insensitive substring match of a param name against the current search text.
	 *  Empty search text matches everything. */
	bool PassesSearchFilter(const FName& ParamName) const;
	/** Called when the search box text changes — rebuilds content to re-apply the filter. */
	void OnSearchChanged(const FText& NewText);
	/** Toggle a group's collapsed state and rebuild. */
	void OnToggleGroupCollapsed(FName GroupName);
	/** Whether a group is currently collapsed. A search forces groups with matches to expand
	 *  so matching rows are visible. */
	bool IsGroupCollapsed(FName GroupName) const;

	// --- Handlers ---
	FReply OnRefreshClicked();
	/** Build the right-click context menu for a parameter row (copy name / toggle override). */
	TSharedRef<SWidget> BuildRowContextMenu(TSharedPtr<FMLPInstanceParamVM> Param);
	/** Drag a param to a different group (writes AssetUserData only, not the parent material). */
	void OnParamMovedToGroup(TSharedPtr<FMLPInstanceParamVM> Param, FName NewGroup);
	/**
	 * Insert a param at an exact position (handles both same-group reorder and cross-group move).
	 * InsertIdx is an index INTO the target group's current param ordering (after removing the
	 * dragged param if it was already in that group). Writes both SetParamGroup (if cross-group)
	 * and renumbers ParamSort for the target group to match the new order.
	 */
	void OnParamInsertedAt(TSharedPtr<FMLPInstanceParamVM> Param, FName TargetGroup, int32 InsertIdx);
	/** Set the group-order sort priority for a group (reorders groups). */
	void OnGroupSortChanged(FName GroupName, int32 NewPriority);
	/** Rename a group (AssetUserData only — updates all params mapped to OldName). */
	void OnGroupRenamed(FName OldName, const FText& NewName, ETextCommit::Type CommitType);
	/** Add a new empty group to the custom group order (AssetUserData only). */
	FReply OnAddGroupClicked();

	// --- Override value handlers (write straight to the MI's typed value arrays) ---
	void OnToggleOverride(TSharedPtr<FMLPInstanceParamVM> Param);
	void OnInstanceScalarChanged(TSharedPtr<FMLPInstanceParamVM> Param, float NewValue, ETextCommit::Type CommitType);
	void OnInstanceVectorChanged(TSharedPtr<FMLPInstanceParamVM> Param, FLinearColor NewColor);
	void OnInstanceTextureChanged(TSharedPtr<FMLPInstanceParamVM> Param, UObject* NewTexture);
	void OnInstanceBoolChanged(TSharedPtr<FMLPInstanceParamVM> Param, bool bNewValue);
	void SetStaticSwitchOverride(TSharedPtr<FMLPInstanceParamVM> Param, bool bOverride, bool bNewValue);

	// --- Status ---
	FText GetInstanceName() const;
	FText GetStatusText() const;

	// --- Data ---
	TWeakPtr<IMaterialEditor> OwningInstanceEditor;
	TWeakObjectPtr<UMaterialInstance> TargetInstance;
	TWeakObjectPtr<UMaterial> TargetMaterial;  // parent material
	TWeakObjectPtr<UMaterialInstanceGroupData> GroupData;

	TArray<TSharedPtr<FMLPInstanceParamVM>> AllParams;
	TArray<TSharedPtr<FMLPInstanceGroupVM>> Groups;

	// --- UI ---
	TSharedPtr<SVerticalBox> ContentContainer;
	TSharedPtr<SScrollBox> ParamScroll;
	/** Current search-box text (case-insensitive substring filter on param names). */
	FText SearchText;
	/** Names of groups the user has collapsed (session-only view state, not persisted). */
	TSet<FName> CollapsedGroups;
	TOptional<double> LastPollTime;
	/** Persisted list of group names shared by all row SComboBoxes (SComboBox holds a raw
	 *  ptr to its options source, so it must outlive the combo box — keep it as a member).
	 *  Stored as shared ptrs because UE4.26 SComboBox requires OptionType to be a pointer type. */
	TArray<TSharedPtr<FName>> CachedGroupNames;

	/** Group title-bar widgets built in the last RebuildInstanceContent, paired with their
	 *  group name. Used by OnDrop to hit-test which group the mouse is over — a reliable
	 *  fallback because SCompoundWidget drop targets aren't always registered in the HittestGrid. */
	TArray<TPair<FName, TWeakPtr<SWidget>>> GroupTitleWidgets;
	/** Per-row outermost widget (SHorizontalBox), one per parameter row, paired with its owning
	 *  group name, IN RENDER ORDER (group order + in-group ParamSort order). Used by
	 *  ComputeDropTarget to find the exact insertion index and to position the blue-line drop
	 *  indicator. Reset each rebuild. */
	TArray<TPair<FName, TWeakPtr<SWidget>>> ParamRowWidgets;
	/** Name of the group the drag is currently hovering over (for highlight), or NAME_None. */
	FName DragOverGroup;
	/** Insertion index WITHIN DragOverGroup (0..ParamCount) the drag is hovering at, or INDEX_NONE. */
	int32 DragOverInsertIndex = INDEX_NONE;
	/** Tick geometry of the panel (for converting pointer coords in OnDrop). */
	FGeometry PanelGeometry;
	};
