#pragma once

#include "CoreMinimal.h"
#include "Model/MaterialParameterInfo.h"

class UMaterial;
class UMaterialExpression;

/** Analyzes whether a parameter is actually used by the material graph. */
class FMaterialParameterUsageAnalyzer
{
public:
	/** Analyze all parameters in a material and update their Usage field. */
	static void Analyze(UMaterial* Material, TArray<TSharedPtr<FMLPParameterInfo>>& Parameters);

private:
	/** Determine usage for a single parameter expression. */
	static EMLPParameterUsage AnalyzeExpression(UMaterialExpression* ParameterExpression, UMaterial* Material);

	/** Check if the parameter expression is connected to any other expression's input. */
	static bool HasAnyConnection(UMaterialExpression* ParameterExpression, UMaterial* Material);

	/** Check if the parameter is connected via NamedReroute or CustomOutput-like nodes. */
	static bool HasIndirectConnection(UMaterialExpression* ParameterExpression, UMaterial* Material);

	/** Test whether a property is an FExpressionInput field. */
	static bool IsExpressionInputProperty(FStructProperty* StructProp);
};
