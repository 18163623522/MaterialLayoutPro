#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MLPEditorData.generated.h"

class UMaterial;
class UMaterialExpressionParameter;
class UMaterialExpressionScalarParameter;
class UMaterialExpressionVectorParameter;
class UMaterialExpressionTextureSampleParameter;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionStaticBoolParameter;
class UMaterialExpressionStaticSwitchParameter;
class UTexture;

/** Wrapper for a single material parameter value. One UObject per parameter.
 *  IDetailsView edits these UPROPERTY fields natively — no custom Slate needed. */
UCLASS(BlueprintType)
class UMLPEditorParameter : public UObject
{
	GENERATED_BODY()

public:
	/** Parameter display name (read-only). */
	UPROPERTY(VisibleAnywhere, Category = "Parameter", meta = (DisplayName = ""))
	FText DisplayName;

	/** Parameter type (read-only). */
	UPROPERTY(VisibleAnywhere, Category = "Parameter")
	FName ParameterType;

	/** Group (editable). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (DisplayName = "Group"))
	FName Group;

	/** Sort priority (editable). */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (DisplayName = "Sort Priority"))
	int32 SortPriority = 0;

	/** Scalar value. */
	UPROPERTY(EditAnywhere, Category = "Value", meta = (DisplayName = "Scalar", EditCondition = "bShowScalar", EditConditionHides))
	float ScalarValue = 0.0f;

	/** Vector/Color value. */
	UPROPERTY(EditAnywhere, Category = "Value", meta = (DisplayName = "Vector", EditCondition = "bShowVector", EditConditionHides))
	FLinearColor VectorValue = FLinearColor::White;

	/** Texture value. */
	UPROPERTY(EditAnywhere, Category = "Value", meta = (DisplayName = "Texture", EditCondition = "bShowTexture", EditConditionHides, AllowedClasses = "/Script/Engine.Texture"))
	TSoftObjectPtr<UTexture> TextureValue;

	/** Bool/Switch value. */
	UPROPERTY(EditAnywhere, Category = "Value", meta = (DisplayName = "Default Value", EditCondition = "bShowBool", EditConditionHides))
	bool BoolValue = false;

	/** Usage status (read-only display). */
	UPROPERTY(VisibleAnywhere, Category = "Parameter")
	FName UsageStatus;

	// Internal flags controlling which value field is visible.
	UPROPERTY() uint8 bShowScalar : 1;
	UPROPERTY() uint8 bShowVector : 1;
	UPROPERTY() uint8 bShowTexture : 1;
	UPROPERTY() uint8 bShowBool : 1;

	/** Weak pointer to the actual expression in the material. */
	UPROPERTY() TWeakObjectPtr<UMaterialExpressionParameter> SourceExpression;

	/** Read the current value from the source expression. */
	void PullFromExpression();

	/** Push the current value back to the source expression. */
	void PushToExpression();

	UMLPEditorParameter();
};

/** A group of parameters. Shown as a collapsible array in IDetailsView. */
USTRUCT(BlueprintType)
struct FMLPEditorParameterGroup
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Group")
	FName GroupName;

	UPROPERTY(Instanced, EditAnywhere, Category = "Group", meta = (TitleProperty = "DisplayName"))
	TArray<UMLPEditorParameter*> Parameters;
};

/** Top-level wrapper object that IDetailsView displays. */
UCLASS(BlueprintType)
class UMLPEditorData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY() TWeakObjectPtr<UMaterial> TargetMaterial;

	UPROPERTY(Instanced, EditAnywhere, Category = "Material Parameters", meta = (TitleProperty = "GroupName"))
	TArray<FMLPEditorParameterGroup> ParameterGroups;

	/** Build from the material's parameter expressions. */
	void BuildFromMaterial(UMaterial* InMaterial);

	/** Push all parameter values back to the material. */
	void PushAllToMaterial();
};
