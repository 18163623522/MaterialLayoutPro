#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Widgets/SMaterialInstanceGroupPanel.h"  // for FMLPInstanceParamVM

/**
 * Drag-and-drop payload for an instance parameter (FMLPInstanceParamVM).
 * Used by SMaterialInstanceGroupPanel rows to drag a parameter onto a group
 * title bar to move it into that group (writes AssetUserData only).
 */
class FMLPInstanceParamDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMLPInstanceParamDragDrop, FDragDropOperation)

	FMLPInstanceParamDragDrop() = default;
	explicit FMLPInstanceParamDragDrop(TSharedPtr<FMLPInstanceParamVM> InParam)
		: Param(InParam)
	{
	}

	TSharedPtr<FMLPInstanceParamVM> Param;

	bool IsValid() const { return Param.IsValid(); }

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		FString Label = (Param.IsValid() ? Param->Name.ToString() : TEXT("?"));
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
