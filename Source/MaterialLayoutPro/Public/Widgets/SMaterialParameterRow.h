#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialLayoutViewModel.h"

class FMLPSession;

/** Broadcast when the row is clicked. bCtrl=toggle, bShift=range. */
DECLARE_DELEGATE_ThreeParams(FOnRowClicked, TSharedPtr<FMLPParamVM>, bool /*bCtrl*/, bool /*bShift*/);

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
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TSharedPtr<FMLPParamVM> VM;
	TSharedPtr<FMLPSession> Session;
	bool bSelected = false;
	bool bDetailMode = false;
	FOnRowClicked OnClickedDelegate;

	TSharedRef<SWidget> BuildValueEditor();

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
