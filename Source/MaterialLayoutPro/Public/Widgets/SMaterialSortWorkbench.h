#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Model/MaterialParameterInfo.h"

class UMaterial;
class SVerticalBox;
class SEditableTextBox;

DECLARE_DELEGATE(FOnWorkbenchApplied);

/**
 * Standalone Sort Workbench window.
 *
 * Opens from the MLP panel, shows a copy of the material's parameters grouped by
 * Group, and lets the user drag rows between groups / reorder within groups / edit
 * Group & SortPriority inline — all in a working copy that does NOT touch the material
 * until the user clicks "应用更改".
 *
 * On Apply: writes Group + SortPriority back in a single FScopedTransaction and
 * broadcasts FOnWorkbenchApplied so the MLP panel can refresh.
 */
class MATERIALLAYOUTPRO_API SMaterialSortWorkbench : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMaterialSortWorkbench) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterial>, TargetMaterial)
		SLATE_ARGUMENT(TArray<TSharedPtr<FMLPParameterInfo>>, Parameters)
		SLATE_EVENT(FOnWorkbenchApplied, OnApplied)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** A working group used by the workbench (independent of the MLP panel's groups). */
	struct FWorkGroup
	{
		FName Name;
		bool bExpanded = true;
		TArray<TSharedPtr<FMLPParameterInfo>> Parameters;
	};

	/** Rebuild the working copy of groups from the working parameter list. */
	void RebuildWorkingGroups();
	/** Rebuild the slate list from the working groups. */
	void RebuildList();

	TSharedRef<SWidget> BuildGroupHeader(TSharedPtr<FWorkGroup> Group);
	TSharedRef<SWidget> BuildRow(TSharedPtr<FMLPParameterInfo> Param, TSharedPtr<FWorkGroup> Group);

	FReply OnApplyClicked();
	FReply OnCancelClicked();
	FReply OnResetClicked();

	void ToggleGroup(TSharedPtr<FWorkGroup> Group);

	TWeakObjectPtr<UMaterial> TargetMaterial;
	TArray<TSharedPtr<FMLPParameterInfo>> WorkParameters;
	TArray<TSharedPtr<FWorkGroup>> WorkGroups;

	TSharedPtr<SVerticalBox> ListContainer;
	FOnWorkbenchApplied OnApplied;
};
