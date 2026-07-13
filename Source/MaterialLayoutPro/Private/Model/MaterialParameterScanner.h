#pragma once

#include "CoreMinimal.h"
#include "Model/MaterialParameterInfo.h"

class UMaterial;
class UMaterialInstance;
class UMaterialExpression;

/** Scans a material (or material instance) and extracts all parameter nodes. */
class FMaterialParameterScanner
{
public:
	/** Scan all parameters in the given material. */
	static TArray<TSharedPtr<FMLPParameterInfo>> ScanMaterial(UMaterial* Material);

	/** Scan parameters from a material instance (currently falls back to its base material). */
	static TArray<TSharedPtr<FMLPParameterInfo>> ScanMaterialInstance(UMaterialInstance* MaterialInstance);

private:
	/** Mark bHasDuplicateName=true on every param whose Name appears more than once in the list.
	 *  Called by ScanMaterial so callers always get the flag computed. */
	static void DetectDuplicateNames(TArray<TSharedPtr<FMLPParameterInfo>>& Parameters);

	static TSharedPtr<FMLPParameterInfo> CreateInfo(UMaterialExpression* Expression);
	static EMLPParameterType DetermineType(UMaterialExpression* Expression);
	static FName GetGroup(UMaterialExpression* Expression);
	static int32 GetSortPriority(UMaterialExpression* Expression);
	static FGuid GetGuid(UMaterialExpression* Expression);
	static bool GetIsInstanceEditable(UMaterialExpression* Expression);
	static FString GetValueString(UMaterialExpression* Expression);
};
