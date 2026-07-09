#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class UMaterialExpressionParameter;
class FMLPSession;
class SEditableTextBox;

/**
 * Main dockable panel.
 *
 * NOTE (Phase 1 interim): The old IDetailsView + UMLPEditorData rendering has been
 * removed. This panel currently shows a placeholder until Phase 2 rewrites it with
 * the hand-written Slate + ViewModel layer. The toolbar handlers that operate on
 * the material directly (auto-group, delete unused, export, etc.) are preserved.
 */
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

	TSharedPtr<SEditableTextBox> SetGroupTextBox;
};
