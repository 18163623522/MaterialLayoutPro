#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class UMaterialExpressionParameter;
class IDetailsView;
class UMLPEditorData;
class SEditableTextBox;

class MATERIALLAYOUTPRO_API SMaterialLayoutProPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayoutProPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMaterialLayoutProPanel() override;

private:
	void OnSelectionChanged(UObject* Selection);

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildStatusBar();

	void RefreshParameters();

	FReply OnRefreshClicked();
	FReply OnSelectMaterialClicked();
	FReply OnOpenMaterialEditorClicked();
	FReply OnArchiveUnusedClicked();
	FReply OnDeleteUnusedClicked();
	FReply OnSetGroupClicked();
	FReply OnAutoGroupClicked();
	FReply OnBulkRenameClicked();
	FReply OnExportClicked();
	FReply OnImportClicked();
	FReply OnSortWorkbenchClicked();
	FReply OnParameterEditorClicked();
	FReply OnGroupByCommentClicked();

	FText GetTargetMaterialName() const;
	FText GetStatusText() const;

	TWeakObjectPtr<UMaterial> TargetMaterial;
	TWeakObjectPtr<UMaterialInstance> TargetMaterialInstance;

	/** Engine-native details view — handles all parameter editing. */
	TSharedPtr<IDetailsView> DetailsView;

	/** Wrapper object holding editable parameter data for IDetailsView. */
	UPROPERTY() UMLPEditorData* EditorData;

	TSharedPtr<SEditableTextBox> SetGroupTextBox;
};
