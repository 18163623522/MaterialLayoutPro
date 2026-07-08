#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Model/MaterialParameterInfo.h"

/**
 * Drag-and-drop payload carrying one or more material parameters.
 * Used by SMaterialParameterRow to support dragging a parameter onto another
 * row to move it into the target's group (matches the original plugin's
 * "Drag-and-Drop Sorting" interaction).
 */
class FMLPParameterDragDrop : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FMLPParameterDragDrop, FDragDropOperation)

	FMLPParameterDragDrop() = default;
	explicit FMLPParameterDragDrop(TArray<TSharedPtr<FMLPParameterInfo>> InParameters)
		: Parameters(MoveTemp(InParameters))
	{
	}

	/** Parameters being dragged (all selected rows travel together). */
	TArray<TSharedPtr<FMLPParameterInfo>> Parameters;

	/** True when at least one dragged parameter exists. */
	bool IsValid() const { return Parameters.Num() > 0; }
};
