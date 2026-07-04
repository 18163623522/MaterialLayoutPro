#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialParameterInfo.h"

class UMaterial;
class UMaterialInstance;
class UMaterialExpressionParameter;

/** A group of parameters sharing the same Group name. */
struct FMLPParameterGroup
{
	FName Name;
	int32 SortPriority;
	TArray<TSharedPtr<FMLPParameterInfo>> Parameters;
	bool bExpanded = true;
};

/**
 * Main dockable panel for Material Layout Pro.
 * Shows material parameters grouped by Group, with type/status badges and bulk editing.
 */
class MATERIALLAYOUTPRO_API SMaterialLayoutProPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayoutProPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMaterialLayoutProPanel() override;

private:
	void RefreshParameters();
	void RebuildGroups();
	void RebuildGroupList();
	void OnSelectionChanged(UObject* Selection);

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();
	TSharedRef<SWidget> BuildGroupList();
	TSharedRef<SWidget> BuildGroupHeader(TSharedPtr<FMLPParameterGroup> Group);
	TSharedRef<SWidget> BuildEmptyState();

	FReply OnRefreshClicked();
	FReply OnSelectMaterialClicked();
	FReply OnArchiveUnusedClicked();
	FReply OnDeleteUnusedClicked();
	FReply OnSetGroupClicked();
	FReply OnAutoGroupClicked();
	FReply OnBulkRenameClicked();
	FReply OnExportClicked();
	FReply OnGroupByCommentClicked();

	void OnRowClicked(TSharedPtr<FMLPParameterInfo> Item, const FPointerEvent& MouseEvent);
	void OnGroupHeaderClicked(TSharedPtr<FMLPParameterGroup> Group);
	void OnParameterGroupChanged(TSharedPtr<FMLPParameterInfo> Item, FName NewGroup);
	void OnParameterPriorityChanged(TSharedPtr<FMLPParameterInfo> Item, int32 NewValue);
	void ApplyGroupChange(TSharedPtr<FMLPParameterInfo> Item, FName NewGroup);
	void ApplyPriorityChange(TSharedPtr<FMLPParameterInfo> Item, int32 NewValue);
	void ApplyGroupToSelected(FName NewGroup);

	void ApplyGroupChangeInternal(TSharedPtr<FMLPParameterInfo> Item, FName NewGroup);
	void ApplyPriorityChangeInternal(TSharedPtr<FMLPParameterInfo> Item, int32 NewValue);

	void SetSelected(TSharedPtr<FMLPParameterInfo> Item, bool bSelected);
	void ClearSelection();
	void ToggleGroupExpansion(TSharedPtr<FMLPParameterGroup> Group);
	void ArchiveUnused();
	void DeleteUnused();
	void AutoGroup();
	void BulkRename(const FString& Find, const FString& Replace, bool bRegex);
	void GroupByComment();
	void ExportParameters(const FString& FilePath);

	FText GetTargetMaterialName() const;
	FText GetStatusText() const;
	int32 GetSelectedCount() const;
	int32 GetUnusedCount() const;
	bool HasUnusedParameters() const;

	/** Currently displayed material. */
	TWeakObjectPtr<UMaterial> TargetMaterial;
	TWeakObjectPtr<UMaterialInstance> TargetMaterialInstance;

	/** Flat parameter items. */
	TArray<TSharedPtr<FMLPParameterInfo>> Parameters;

	/** Grouped parameters. */
	TArray<TSharedPtr<FMLPParameterGroup>> Groups;

	/** Scroll box containing groups. */
	TSharedPtr<SVerticalBox> GroupContainer;

	/** Text box for bulk group assignment. */
	TSharedPtr<SEditableTextBox> SetGroupTextBox;

	/** Last clicked item for Shift+click range selection. */
	TWeakPtr<FMLPParameterInfo> LastClickedItem;
};
