#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"

UENUM(BlueprintType)
enum class EMLPParameterUsage : uint8
{
	Used        UMETA(DisplayName = "Used"),
	Unused      UMETA(DisplayName = "Unused"),
	HalfUsed    UMETA(DisplayName = "Half Used"),
	Indirect    UMETA(DisplayName = "Indirect"),
	Unknown     UMETA(DisplayName = "Unknown"),
};

UENUM(BlueprintType)
enum class EMLPParameterType : uint8
{
	Scalar        UMETA(DisplayName = "Scalar"),
	Vector        UMETA(DisplayName = "Vector"),
	Texture       UMETA(DisplayName = "Texture"),
	StaticBool    UMETA(DisplayName = "Static Bool"),
	StaticSwitch  UMETA(DisplayName = "Static Switch"),
	Other         UMETA(DisplayName = "Other"),
};

/** Runtime metadata for a single material parameter. Plain struct to avoid UHT/UE name collisions. */
struct FMLPParameterInfo
{
	/** Parameter name. */
	FName Name;

	/** Parameter category (e.g. Scalar, Vector, Texture). */
	EMLPParameterType Type = EMLPParameterType::Other;

	/** Display group in the material editor. */
	FName Group;

	/** Sort priority within the group. */
	int32 SortPriority = 0;

	/** Stable GUID used for parameter identification. */
	FGuid Guid;

	/** The expression node this parameter is represented by. */
	TWeakObjectPtr<UMaterialExpression> Expression;

	/** Usage classification. */
	EMLPParameterUsage Usage = EMLPParameterUsage::Unknown;

	/** True if this parameter is editable from a material instance. */
	bool bIsInstanceEditable = false;

	/** Default / current value display string. */
	FString ValueString;

	/** Runtime selection state (not serialized). */
	bool bSelected = false;

	/** True if this parameter has been detected as HalfUsed (e.g. MaterialAttributes switch). */
	bool bHalfUsed = false;

	FMLPParameterInfo() = default;

	FText GetDisplayTypeName() const;
	FLinearColor GetTypeColor() const;
	FText GetUsageLabel() const;
	FLinearColor GetUsageColor() const;
	FLinearColor GetUsageBgColor() const;
};
