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
	Used        UMETA(DisplayName = "已使用"),
	Unused      UMETA(DisplayName = "未使用"),
	HalfUsed    UMETA(DisplayName = "部分使用"),
	Indirect    UMETA(DisplayName = "间接"),
	Unknown     UMETA(DisplayName = "未知"),
};

UENUM(BlueprintType)
enum class EMLPParameterType : uint8
{
	Scalar        UMETA(DisplayName = "标量"),
	Vector        UMETA(DisplayName = "向量"),
	Texture       UMETA(DisplayName = "纹理"),
	StaticBool    UMETA(DisplayName = "静态布尔"),
	StaticSwitch  UMETA(DisplayName = "静态开关"),
	Other         UMETA(DisplayName = "其他"),
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

	/** True if another parameter shares the same name (duplicate/conflict alert). */
	bool bHasDuplicateName = false;

	FMLPParameterInfo() = default;

	FText GetDisplayTypeName() const;
	FLinearColor GetTypeColor() const;
	/** Short abbreviation for the pill badge (S/V/T/SS/SB). */
	FText GetTypeAbbreviation() const;
	FText GetUsageLabel() const;
	FLinearColor GetUsageColor() const;
	FLinearColor GetUsageBgColor() const;
};
