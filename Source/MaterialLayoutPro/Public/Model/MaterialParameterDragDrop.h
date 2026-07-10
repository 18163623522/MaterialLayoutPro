#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Model/MaterialLayoutViewModel.h"

/**
 * Drag-and-drop payload carrying one or more material parameters (VMs).
 * Used by SMaterialParameterRow to support dragging a parameter onto another
 * row to reorder within a group or move across groups.
 */
class FMLPParameterDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMLPParameterDragDrop, FDragDropOperation)

	FMLPParameterDragDrop() = default;
	explicit FMLPParameterDragDrop(TArray<TSharedPtr<FMLPParamVM>> InParameters)
		: Parameters(MoveTemp(InParameters))
	{
	}

	/** Parameters being dragged (all selected rows travel together). */
	TArray<TSharedPtr<FMLPParamVM>> Parameters;

	/** True when at least one dragged parameter exists. */
	bool IsValid() const { return Parameters.Num() > 0; }

	/** The first dragged parameter - convenience accessor. */
	TSharedPtr<FMLPParamVM> GetFirstParam() const
	{
		return Parameters.Num() > 0 ? Parameters[0] : nullptr;
	}

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		// Simple drag preview: a semi-transparent border showing the param name.
		FString Label;
		for (const auto& P : Parameters)
		{
			if (P.IsValid())
			{
				if (!Label.IsEmpty()) Label += TEXT(", ");
				Label += P->Name.ToString();
			}
		}
		if (Label.IsEmpty()) Label = TEXT("?");
		if (Parameters.Num() > 1) Label = FString::Printf(TEXT("%s (+%d)"), *Label, Parameters.Num() - 1);

		return SNew(SBorder)
			.BorderBackgroundColor(FLinearColor(0.1f, 0.3f, 0.6f, 0.85f))
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(FMargin(6.f, 2.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Normal", 9))
				.ColorAndOpacity(FLinearColor::White)
			];
	}
};
