#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialParameterInfo.h"

DECLARE_DELEGATE_TwoParams(FOnParameterRowClicked, TSharedPtr<FMLPParameterInfo>, const FPointerEvent&);
DECLARE_DELEGATE_TwoParams(FOnParameterGroupChanged, TSharedPtr<FMLPParameterInfo>, FName);
DECLARE_DELEGATE_TwoParams(FOnParameterPriorityChanged, TSharedPtr<FMLPParameterInfo>, int32);
DECLARE_DELEGATE_TwoParams(FOnParameterRenamed, TSharedPtr<FMLPParameterInfo>, const FString&);
DECLARE_DELEGATE_TwoParams(FOnParameterDoubleClicked, TSharedPtr<FMLPParameterInfo>, const FPointerEvent&);
DECLARE_DELEGATE_RetVal_ThreeParams(FReply, FOnParameterDragDetected, TSharedPtr<FMLPParameterInfo>, const FGeometry&, const FPointerEvent&);
DECLARE_DELEGATE_TwoParams(FOnParameterDropped, const TArray<TSharedPtr<FMLPParameterInfo>>&, TSharedPtr<FMLPParameterInfo>);
DECLARE_DELEGATE_TwoParams(FOnParameterValueChanged, TSharedPtr<FMLPParameterInfo>, float);
DECLARE_DELEGATE_TwoParams(FOnParameterVectorChanged, TSharedPtr<FMLPParameterInfo>, const FLinearColor&);
DECLARE_DELEGATE_TwoParams(FOnParameterTextureChanged, TSharedPtr<FMLPParameterInfo>, class UObject*);
DECLARE_DELEGATE_TwoParams(FOnParameterBoolChanged, TSharedPtr<FMLPParameterInfo>, bool);

/**
 * A single parameter row in the Material Layout Pro panel.
 * Shows name, type badge, group, priority, status and supports inline editing.
 */
class MATERIALLAYOUTPRO_API SMaterialParameterRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialParameterRow)
		: _bSelected(false)
		, _bIsRenaming(false)
		, _bInstanceViewMode(false)
	{}
		SLATE_ARGUMENT(TSharedPtr<FMLPParameterInfo>, Item)
		SLATE_ARGUMENT(bool, bSelected)
		SLATE_ARGUMENT(bool, bIsRenaming)
		SLATE_ARGUMENT(bool, bInstanceViewMode)
		SLATE_EVENT(FOnParameterRowClicked, OnClicked)
		SLATE_EVENT(FOnParameterDoubleClicked, OnDoubleClicked)
		SLATE_EVENT(FOnParameterGroupChanged, OnGroupChanged)
		SLATE_EVENT(FOnParameterPriorityChanged, OnPriorityChanged)
		SLATE_EVENT(FOnParameterRenamed, OnRenamed)
		SLATE_EVENT(FOnParameterDragDetected, OnDragDetected)
		SLATE_EVENT(FOnParameterDropped, OnDropped)
		SLATE_EVENT(FOnParameterValueChanged, OnScalarChanged)
		SLATE_EVENT(FOnParameterVectorChanged, OnVectorChanged)
		SLATE_EVENT(FOnParameterTextureChanged, OnTextureChanged)
		SLATE_EVENT(FOnParameterBoolChanged, OnBoolChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Update the selected state without rebuilding the widget. */
	void SetSelectedState(bool bNewSelected)
	{
		if (bSelected != bNewSelected)
		{
			bSelected = bNewSelected;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
	bool IsSelected() const { return bSelected; }

	// SWidget overrides for drag-and-drop.
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	FReply OnRowClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	void OnGroupTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnPriorityValueCommitted(int32 NewValue, ETextCommit::Type CommitType);
	void OnNameTextCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnScalarValueCommitted(float NewValue, ETextCommit::Type CommitType);
	/** Called during drag (OnValueChanged) — just forwards to the delegate without commit-type. */
	void OnScalarValueDragged(float NewValue);

	/** Build the value editor appropriate for this parameter's type. */
	TSharedRef<SWidget> BuildValueWidget();

	/** Build a multi-line diagnostic tooltip: type, usage, value, duplicate/conflict alerts. */
	FText MakeDiagnosticTooltip() const;

	TSharedPtr<FMLPParameterInfo> Item;
	bool bSelected;
	bool bIsRenaming;
	bool bInstanceViewMode;

	/** Timestamp of the last single click, used to detect a double-click. */
	double LastClickTime = 0.0;

	FOnParameterRowClicked OnClicked;
	FOnParameterDoubleClicked OnDoubleClicked;
	FOnParameterGroupChanged OnGroupChanged;
	FOnParameterPriorityChanged OnPriorityChanged;
	FOnParameterRenamed OnRenamed;
	FOnParameterDragDetected OnDragDetectedDelegate;
	FOnParameterDropped OnDroppedDelegate;
	FOnParameterValueChanged OnScalarChangedDelegate;
	FOnParameterVectorChanged OnVectorChangedDelegate;
	FOnParameterTextureChanged OnTextureChangedDelegate;
	FOnParameterBoolChanged OnBoolChangedDelegate;
};