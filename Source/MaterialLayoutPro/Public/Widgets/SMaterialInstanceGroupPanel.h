#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class SVerticalBox;
class SScrollBox;

/** A single parameter in instance mode: name + type + group + override state + value.
 *  Self-contained per-parameter VM for SMaterialInstanceGroupPanel so the widget can stand alone. */
struct FMLPInstanceParamVM
{
	FName Name;
	FName Group;
	FGuid ExpressionGUID;
	int32 Type = 0;  // EMLPParameterType
	bool bOverridden = false;

	float ScalarValue = 0.f;
	FLinearColor VectorValue = FLinearColor::White;
	TSoftObjectPtr<UTexture> TextureValue;
	bool BoolValue = false;
};

/**
 * Standalone widget for the "material instance parameter group" panel.
 *
 * Shown inside its own SWindow (created by SMaterialLayoutProPanel::OnInstanceGroupClicked
 * / the toolbar "实例分组" button). Because the window holds a strong ref to THIS widget as
 * its content, the widget's lifetime is exactly the window's lifetime — row/tab lambdas can
 * safely capture AsShared() and there is no use-after-free risk.
 *
 * This is a self-contained widget: it owns its own data (InstanceParams / tabs), its own
 * refresh path (RebuildInstanceContent), and writes overrides straight to the bound
 * UMaterialInstance. It deliberately does NOT touch SMaterialLayoutProPanel's material-
 * parameter tree (TreeContainer) — that path is irrelevant here.
 */
class MATERIALLAYOUTPRO_API SMaterialInstanceGroupPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialInstanceGroupPanel) {}
		/** The material instance this panel edits. Required (the panel shows an error if null). */
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterialInstance>, TargetInstance)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// --- Data load ---
	/** Scan the parent material for parameters, fold in instance overrides, build InstanceParams + tabs. */
	void PullFromInstance();

	// --- UI build / refresh ---
	/** Build the top-level container (tab bar slot + scrollable rows slot) once. */
	TSharedRef<SWidget> BuildInitialContent();
	/** Clear + repopulate the top-level container. Called by every state-changing handler. */
	void RebuildInstanceContent();
	/** Build the tab bar row (one button per group + [+] add button). */
	TSharedRef<SWidget> BuildTabBar();
	/** Append parameter rows for CurrentTab into the given scroll box. */
	void BuildRows(TSharedRef<SScrollBox> ContentBox);

	// --- Handlers ---
	FReply OnTabClicked(FName GroupName);
	FReply OnAddTabClicked();
	void OnToggleOverride(TSharedPtr<FMLPInstanceParamVM> Param);
	void OnInstanceScalarChanged(TSharedPtr<FMLPInstanceParamVM> Param, float NewValue, ETextCommit::Type CommitType);
	void OnInstanceVectorChanged(TSharedPtr<FMLPInstanceParamVM> Param, FLinearColor NewColor);
	void OnInstanceTextureChanged(TSharedPtr<FMLPInstanceParamVM> Param, UObject* NewTexture);
	void OnInstanceBoolChanged(TSharedPtr<FMLPInstanceParamVM> Param, bool bNewValue);
	/** Add/update/remove a static switch override entry via StaticParameters + UpdateStaticPermutation. */
	void SetStaticSwitchOverride(TSharedPtr<FMLPInstanceParamVM> Param, bool bOverride, bool bNewValue);

	// --- Data ---
	TWeakObjectPtr<UMaterialInstance> TargetInstance;
	TWeakObjectPtr<UMaterial> TargetMaterial;  // parent material, for group scans
	TArray<TSharedPtr<FMLPInstanceParamVM>> InstanceParams;
	TArray<FName> InstanceTabNames;
	FName CurrentTab;

	// --- UI container (the top-level box RebuildInstanceContent clears + refills) ---
	TSharedPtr<SVerticalBox> ContentContainer;
};
