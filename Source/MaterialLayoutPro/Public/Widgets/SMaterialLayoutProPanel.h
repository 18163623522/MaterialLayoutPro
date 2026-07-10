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

/** A single parameter in instance mode: name + type + group + override state + value. */
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

	// --- Instance mode (tabbed group panel) ---
	void PullFromInstance();
	TSharedRef<SWidget> BuildInstanceContent();
	FReply OnTabClicked(FName GroupName);
	FReply OnAddTabClicked();
	void OnRenameTab(FName OldName, const FText& NewName, ETextCommit::Type);
	void OnDeleteTab(FName GroupName);
	void OnToggleOverride(TSharedPtr<FMLPInstanceParamVM> Param);
	void OnInstanceScalarChanged(TSharedPtr<FMLPInstanceParamVM> Param, float NewValue, ETextCommit::Type);
	void OnInstanceVectorChanged(TSharedPtr<FMLPInstanceParamVM> Param, FLinearColor NewColor);
	void OnInstanceTextureChanged(TSharedPtr<FMLPInstanceParamVM> Param, UObject* NewTexture);
	void OnInstanceBoolChanged(TSharedPtr<FMLPInstanceParamVM> Param, bool bNewValue);

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

	// --- Instance mode state ---
	bool bInstanceMode = false;
	FName CurrentTab;
	TArray<TSharedPtr<FMLPInstanceParamVM>> InstanceParams;
	TArray<FName> InstanceTabNames;
	TSharedPtr<SVerticalBox> InstanceContentContainer;
	TSharedPtr<SEditableTextBox> NewTabInput;
};
