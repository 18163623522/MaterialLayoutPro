#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Model/MaterialParameterInfo.h"

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
 * with inline value editing (Scalar/Vector/Texture/Switch). All edits happen in a
 * working copy — nothing touches the material until the user clicks Apply.
 *
 * Apply writes back to the material (Group/SortPriority/Value) and optionally to a
 * material instance (value overrides) in a single FScopedTransaction.
 */
class MATERIALLAYOUTPRO_API SMaterialParameterEditor : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMaterialParameterEditor) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterial>, TargetMaterial)
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterialInstance>, TargetInstance)
		SLATE_ARGUMENT(TArray<TSharedPtr<FMLPParameterInfo>>, Parameters)
		SLATE_EVENT(FOnParameterEditorApplied, OnApplied)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** A virtual tab (user-defined grouping, independent of material Group). */
	struct FVirtualTab
	{
		FName Name;
		TArray<TSharedPtr<FMLPParameterInfo>> Parameters;
	};

	/** Deep-copy the source parameters into a working set. */
	void InitWorkParameters(const TArray<TSharedPtr<FMLPParameterInfo>>& Source);
	/** Create default tabs from the material's existing Group field. */
	void InitDefaultTabs();

	/** Rebuild the tab bar + active tab content. */
	void RebuildUI();
	/** Build the tab bar row (tabs + "+" button). */
	TSharedRef<SWidget> BuildTabBar();
	/** Build the content for the currently active tab. */
	TSharedRef<SWidget> BuildTabContent();
	/** Build a single parameter row with value editor. */
	TSharedRef<SWidget> BuildParamRow(TSharedPtr<FMLPParameterInfo> Param);

	/** Select a tab by index. */
	FReply SelectTab(int32 TabIndex);
	/** Create a new virtual tab (prompts for name). */
	FReply OnAddTabClicked();
	/** Move a parameter to a different tab. */
	void MoveParameterToTab(TSharedPtr<FMLPParameterInfo> Param, int32 TargetTabIndex);

	/** Value-edit callbacks — update the working copy only (no material write). */
	void OnScalarChanged(TSharedPtr<FMLPParameterInfo> Param, float NewValue);
	void OnVectorChanged(TSharedPtr<FMLPParameterInfo> Param, const FLinearColor& NewColor);
	void OnTextureChanged(TSharedPtr<FMLPParameterInfo> Param, UObject* NewTexture);
	void OnBoolChanged(TSharedPtr<FMLPParameterInfo> Param, bool bNewValue);
	void OnGroupChanged(TSharedPtr<FMLPParameterInfo> Param, const FText& NewText, ETextCommit::Type CommitType);
	void OnPriorityChanged(TSharedPtr<FMLPParameterInfo> Param, int32 NewValue, ETextCommit::Type CommitType);

	/** Write all working-copy changes back to the material. */
	FReply OnApplyToMaterialClicked();
	/** Write parameter value overrides back to the material instance. */
	FReply OnApplyToInstanceClicked();
	FReply OnCancelClicked();

	TWeakObjectPtr<UMaterial> TargetMaterial;
	TWeakObjectPtr<UMaterialInstance> TargetInstance;
	TArray<TSharedPtr<FMLPParameterInfo>> WorkParameters;
	TArray<TSharedPtr<FVirtualTab>> VirtualTabs;
	int32 ActiveTabIndex = 0;

	TSharedPtr<SVerticalBox> ContentContainer;
	TSharedPtr<SVerticalBox> TabBarContainer;
	FOnParameterEditorApplied OnApplied;
};
