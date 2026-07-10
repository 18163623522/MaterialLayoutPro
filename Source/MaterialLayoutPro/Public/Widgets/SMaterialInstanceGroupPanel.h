#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class UMaterialInstanceGroupData;
class IMaterialEditor;
class SVerticalBox;
class SScrollBox;
class SEditableTextBox;

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

	// --- Handlers ---
	FReply OnRefreshClicked();
	/** Drag a param to a different group (writes AssetUserData only, not the parent material). */
	void OnParamMovedToGroup(TSharedPtr<FMLPInstanceParamVM> Param, FName NewGroup);
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
	TOptional<double> LastPollTime;
	/** Persisted list of group names shared by all row SComboBoxes (SComboBox holds a raw
	 *  ptr to its options source, so it must outlive the combo box — keep it as a member).
	 *  Stored as shared ptrs because UE4.26 SComboBox requires OptionType to be a pointer type. */
	TArray<TSharedPtr<FName>> CachedGroupNames;
};
