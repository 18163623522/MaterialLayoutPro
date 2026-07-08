#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Model/MaterialParameterInfo.h"

/**
 * Confirmation dialog that previews a batch of proposed Group/SortPriority changes
 * before they are applied to the material. Shown by the "DryRun" mode of bulk
 * operations (Archive Unused, Auto Group, Group by Comment).
 */
class MATERIALLAYOUTPRO_API SMaterialLayoutPreviewDialog : public SWindow
{
public:
	struct FPreviewChange
	{
		TSharedPtr<FMLPParameterInfo> Param;
		FName OldGroup;
		FName NewGroup;
	};

	SLATE_BEGIN_ARGS(SMaterialLayoutPreviewDialog) {}
		SLATE_ARGUMENT(TArray<FPreviewChange>, Changes)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Description)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** True if the user confirmed the changes. */
	bool WasConfirmed() const { return bConfirmed; }

private:
	FReply OnConfirmClicked();
	FReply OnCancelClicked();

	TArray<TSharedPtr<FPreviewChange>> PendingChanges;
	bool bConfirmed = false;

	TSharedRef<ITableRow> OnGenerateChangeRow(TSharedPtr<FPreviewChange> Item, const TSharedRef<STableViewBase>& OwnerTable);
};
