#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialLayoutViewModel.h"

class FMLPSession;

/** Broadcast when the row is clicked. bCtrl=toggle, bShift=range. */
DECLARE_DELEGATE_ThreeParams(FOnRowClicked, TSharedPtr<FMLPParamVM>, bool /*bCtrl*/, bool /*bShift*/);

/** Broadcast when the row is double-clicked (jump to node in graph). */
DECLARE_DELEGATE_OneParam(FOnRowDoubleClicked, TSharedPtr<FMLPParamVM>);

/** Broadcast when a param is dropped onto this row. bInsertBefore=insert above, false=insert below. */
DECLARE_DELEGATE_ThreeParams(FOnParamDropped, TSharedPtr<FMLPParamVM> /*Dragged*/, TSharedPtr<FMLPParamVM> /*Target*/, bool /*bInsertBefore*/);

class MATERIALLAYOUTPRO_API SMaterialParameterRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialParameterRow)
		: _bSelected(false)
		, _bDetailMode(false)
	{}
		SLATE_ARGUMENT(TSharedPtr<FMLPParamVM>, ParamVM)
		SLATE_ARGUMENT(TSharedPtr<FMLPSession>, Session)
		SLATE_ARGUMENT(bool, bSelected)
		SLATE_ARGUMENT(bool, bDetailMode)
		SLATE_EVENT(FOnRowClicked, OnClicked)
		SLATE_EVENT(FOnRowDoubleClicked, OnDoubleClicked)
		SLATE_EVENT(FOnParamDropped, OnParamDropped)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Preview - fires BEFORE child widgets. Intercept Ctrl/Shift+click for multi-select. */
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	// --- Drag-drop overrides ---
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
	TSharedPtr<FMLPParamVM> VM;
	TSharedPtr<FMLPSession> Session;
	bool bSelected = false;
	bool bDetailMode = false;
	FOnRowClicked OnClickedDelegate;
	FOnRowDoubleClicked OnDoubleClickedDelegate;
	FOnParamDropped OnParamDroppedDelegate;

	// Drop target visual state
	bool bIsDropTarget = false;
	bool bDropBefore = false;

	TSharedRef<SWidget> BuildValueEditor();
	TSharedRef<SWidget> BuildDragHandle();

	void OnScalarCommitted(float NewValue, ETextCommit::Type CommitType);
	void OnScalarDragged(float NewValue);
	void OnVectorChanged(FLinearColor NewColor);
	void OnTextureChanged(UObject* NewTexture);
	void OnBoolChanged(bool bNewValue);
	void OnGroupCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnPriorityCommitted(int32 NewValue, ETextCommit::Type CommitType);
	void OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType);

	FText MakeDiagnosticTooltip() const;
};
