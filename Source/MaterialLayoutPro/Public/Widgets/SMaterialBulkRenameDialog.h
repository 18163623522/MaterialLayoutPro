#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Model/MaterialParameterInfo.h"

class UMaterial;

/** Broadcast after parameters have been renamed and the material post-edited. */
DECLARE_DELEGATE(FOnParametersRenamed);

/**
 * Modal dialog for bulk renaming material parameters.
 * Supports simple find/replace and optional regex matching.
 */
class MATERIALLAYOUTPRO_API SMaterialBulkRenameDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMaterialBulkRenameDialog) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterial>, TargetMaterial)
		SLATE_ARGUMENT(TArray<TSharedPtr<FMLPParameterInfo>>, Parameters)
		SLATE_EVENT(FOnParametersRenamed, OnRenamed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	struct FPreviewItem
	{
		TSharedPtr<FMLPParameterInfo> Param;
		FString NewName;
	};

	void UpdatePreview(const FText& NewText = FText::GetEmpty());
	FString ComputeNewName(const FString& OldName) const;

	FReply OnRenameClicked();
	FReply OnCancelClicked();

	TWeakObjectPtr<UMaterial> TargetMaterial;
	TArray<TSharedPtr<FMLPParameterInfo>> Parameters;

	TSharedPtr<SEditableTextBox> FindTextBox;
	TSharedPtr<SEditableTextBox> ReplaceTextBox;
	TSharedPtr<SCheckBox> RegexCheckBox;
	TSharedPtr<SListView<TSharedPtr<FPreviewItem>>> PreviewList;

	TArray<TSharedPtr<FPreviewItem>> PreviewData;

	FOnParametersRenamed OnRenamed;

	TSharedRef<ITableRow> OnGeneratePreviewRow(TSharedPtr<FPreviewItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
};
