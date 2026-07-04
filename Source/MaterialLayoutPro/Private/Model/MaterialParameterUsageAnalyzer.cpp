#include "Model/MaterialParameterUsageAnalyzer.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionIO.h"
#include "UObject/Field.h"

void FMaterialParameterUsageAnalyzer::Analyze(UMaterial* Material, TArray<TSharedPtr<FMLPParameterInfo>>& Parameters)
{
	if (!Material)
	{
		for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
		{
			Param->Usage = EMLPParameterUsage::Unknown;
		}
		return;
	}

	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (Param.IsValid())
		{
			Param->Usage = AnalyzeExpression(Param->Expression.Get(), Material);
		}
	}
}

EMLPParameterUsage FMaterialParameterUsageAnalyzer::AnalyzeExpression(UMaterialExpression* ParameterExpression, UMaterial* Material)
{
	if (!ParameterExpression || !Material)
	{
		return EMLPParameterUsage::Unknown;
	}

	if (HasAnyConnection(ParameterExpression, Material))
	{
		return EMLPParameterUsage::Used;
	}

	if (HasIndirectConnection(ParameterExpression, Material))
	{
		return EMLPParameterUsage::Indirect;
	}

	return EMLPParameterUsage::Unused;
}

bool FMaterialParameterUsageAnalyzer::IsExpressionInputProperty(FStructProperty* StructProp)
{
	if (!StructProp || !StructProp->Struct)
	{
		return false;
	}

	// FExpressionInput is a noexport USTRUCT, so it has no C++ StaticStruct().
	// Compare by the reflected UScriptStruct name instead.
	return StructProp->Struct->GetFName() == FName(TEXT("ExpressionInput"));
}

bool FMaterialParameterUsageAnalyzer::HasAnyConnection(UMaterialExpression* ParameterExpression, UMaterial* Material)
{
	if (!ParameterExpression || !Material)
	{
		return false;
	}

	for (UMaterialExpression* OtherExpression : Material->Expressions)
	{
		if (!OtherExpression || OtherExpression == ParameterExpression)
		{
			continue;
		}

		for (TFieldIterator<FProperty> PropIt(OtherExpression->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}

			if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				if (IsExpressionInputProperty(StructProp))
				{
					FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(OtherExpression);
					if (Input && Input->Expression == ParameterExpression)
					{
						return true;
					}
				}
			}
			else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				if (FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					if (IsExpressionInputProperty(InnerStructProp))
					{
						FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(OtherExpression));
						for (int32 Idx = 0; Idx < ArrayHelper.Num(); ++Idx)
						{
							FExpressionInput* Input = reinterpret_cast<FExpressionInput*>(ArrayHelper.GetRawPtr(Idx));
							if (Input && Input->Expression == ParameterExpression)
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool FMaterialParameterUsageAnalyzer::HasIndirectConnection(UMaterialExpression* ParameterExpression, UMaterial* Material)
{
	if (!ParameterExpression || !Material)
	{
		return false;
	}

	for (UMaterialExpression* OtherExpression : Material->Expressions)
	{
		if (!OtherExpression || OtherExpression == ParameterExpression)
		{
			continue;
		}

		const FString ClassName = OtherExpression->GetClass()->GetName();
		if (ClassName.Contains(TEXT("NamedReroute")) || ClassName.Contains(TEXT("CustomOutput")))
		{
			for (TFieldIterator<FProperty> PropIt(OtherExpression->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (!Property)
				{
					continue;
				}

				if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
				{
					if (IsExpressionInputProperty(StructProp))
					{
						FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(OtherExpression);
						if (Input && Input->Expression == ParameterExpression)
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}
