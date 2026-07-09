#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialLayoutViewModel.h"

class FMLPSession;

/**
 * A single parameter row bound to a FMLPParamVM snapshot.
 *
 * Value controls read/write the VM directly (never the engine expression), so the
 * control instance stays stable across refreshes and focus is never lost. The VM
 * is flushed back to the material in a single transaction via FMLPSession::PushDirty().
 */
class MATERIALLAYOUTPRO_API SMaterialParameterRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialParameterRow)
		: _bSelected(false)
		, _bDetailMode(false)
	{}
		/** The parameter VM this row displays/edits. */
		SLATE_ARGUMENT(TSharedPtr<FMLPParamVM>, ParamVM)
		/** The session (for the interaction lock). */
		SLATE_ARGUMENT(TSharedPtr<FMLPSession>, Session)
		/** Whether this row is selected. */
		SLATE_ARGUMENT(bool, bSelected)
		/** Detail mode: show group + priority editors inline (used by the right pane). */
		SLATE_ARGUMENT(bool, bDetailMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<FMLPParamVM> VM;
	TSharedPtr<FMLPSession> Session;
	bool bSelected = false;
	bool bDetailMode = false;

	/** Build the value editor widget based on VM->Type. */
	TSharedRef<SWidget> BuildValueEditor();

	// --- Value commit callbacks (write to VM, mark dirty) ---

	void OnScalarCommitted(float NewValue, ETextCommit::Type CommitType);
	void OnScalarDragged(float NewValue);
	void OnVectorChanged(FLinearColor NewColor);
	void OnTextureChanged(UObject* NewTexture);
	void OnBoolChanged(bool bNewValue);
	void OnGroupCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnPriorityCommitted(int32 NewValue, ETextCommit::Type CommitType);

	/** Build a multi-line diagnostic tooltip. */
	FText MakeDiagnosticTooltip() const;
};
