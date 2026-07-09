#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Model/MaterialParameterInfo.h"
#include "Model/MaterialLayoutViewModel.h"

class UMaterial;
class UMaterialInstance;
class SVerticalBox;
class SEditableTextBox;
class SScrollBox;

DECLARE_DELEGATE(FOnParameterEditorApplied);

/**
 * Houdini-style standalone parameter editor window.
 *
 * Shows parameters in user-defined virtual tabs (independent of the material's Group),
 * with inline value editing. Shares the main panel's FMLPSession snapshot, so value
 * edits land on the VM; "应用到材质" flushes via Session::PushDirty(), "应用到实例"
 * reads VM values and writes them as instance overrides.
 */
class MATERIALLAYOUTPRO_API SMaterialParameterEditor : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMaterialParameterEditor) {}
		/** Shared session snapshot (owned by the main panel). */
		SLATE_ARGUMENT(TSharedPtr<FMLPSession>, Session)
		/** Optional material instance for "apply to instance". */
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterialInstance>, TargetInstance)
		SLATE_EVENT(FOnParameterEditorApplied, OnApplied)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** A virtual tab (user-defined grouping, independent of material Group). */
	struct FVirtualTab
	{
		FName Name;
		TArray<TSharedPtr<FMLPParamVM>> Parameters;
	};

	/** Create default tabs from the VM groups (one tab per group). */
	void InitDefaultTabs();
	/** Rebuild the tab bar + active tab content. */
	void RebuildUI();
	/** Build the tab bar row (tabs + "+" button). */
	TSharedRef<SWidget> BuildTabBar();
	/** Build the content for the currently active tab. */
	TSharedRef<SWidget> BuildTabContent();
	/** Build a single parameter row with value editor bound to its VM. */
	TSharedRef<SWidget> BuildParamRow(TSharedPtr<FMLPParamVM> Param);

	/** Select a tab by index. */
	FReply SelectTab(int32 TabIndex);
	/** Create a new virtual tab. */
	FReply OnAddTabClicked();
	/** Move a parameter to a different tab. */
	void MoveParameterToTab(TSharedPtr<FMLPParamVM> Param, int32 TargetTabIndex);

	/** Write all VM changes back to the material (Session::PushDirty). */
	FReply OnApplyToMaterialClicked();
	/** Read VM values and write them as material-instance overrides. */
	FReply OnApplyToInstanceClicked();
	FReply OnCancelClicked();

	TSharedPtr<FMLPSession> Session;
	TWeakObjectPtr<UMaterialInstance> TargetInstance;
	TArray<TSharedPtr<FVirtualTab>> VirtualTabs;
	int32 ActiveTabIndex = 0;

	TSharedPtr<SVerticalBox> ContentContainer;
	TSharedPtr<SVerticalBox> TabBarContainer;
	FOnParameterEditorApplied OnApplied;
};
