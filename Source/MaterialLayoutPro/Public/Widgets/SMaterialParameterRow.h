#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialParameterInfo.h"

DECLARE_DELEGATE_TwoParams(FOnParameterRowClicked, TSharedPtr<FMLPParameterInfo>, const FPointerEvent&);
DECLARE_DELEGATE_TwoParams(FOnParameterGroupChanged, TSharedPtr<FMLPParameterInfo>, FName);
DECLARE_DELEGATE_TwoParams(FOnParameterPriorityChanged, TSharedPtr<FMLPParameterInfo>, int32);

/**
 * A single parameter row in the Material Layout Pro panel.
 * Shows name, type badge, group, priority, status and supports inline editing.
 */
class MATERIALLAYOUTPRO_API SMaterialParameterRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialParameterRow)
		: _bSelected(false)
	{}
		SLATE_ARGUMENT(TSharedPtr<FMLPParameterInfo>, Item)
		SLATE_ARGUMENT(bool, bSelected)
		SLATE_EVENT(FOnParameterRowClicked, OnClicked)
		SLATE_EVENT(FOnParameterGroupChanged, OnGroupChanged)
		SLATE_EVENT(FOnParameterPriorityChanged, OnPriorityChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnRowClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	void OnGroupTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnPriorityValueCommitted(int32 NewValue, ETextCommit::Type CommitType);

	TSharedPtr<FMLPParameterInfo> Item;
	bool bSelected;

	FOnParameterRowClicked OnClicked;
	FOnParameterGroupChanged OnGroupChanged;
	FOnParameterPriorityChanged OnPriorityChanged;
};