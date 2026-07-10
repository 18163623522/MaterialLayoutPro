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

	// Build a connection index in a SINGLE pass over all expressions:
	//   ConnectedSet     — expressions directly referenced by some FExpressionInput
	//   IndirectSet      — expressions referenced via NamedReroute / CustomOutput nodes
	// This turns the per-parameter O(N*M) scan into O(M) build + O(N) lookup.
	TSet<UMaterialExpression*> ConnectedSet;
	TSet<UMaterialExpression*> IndirectSet;
	TArray<UMaterialExpression*> IndirectNodes;

#if ENGINE_MAJOR_VERSION >= 5
	const auto& Expressions = Material->GetExpressions();
#else
	const auto& Expressions = Material->Expressions;
#endif

	for (UMaterialExpression* OtherExpression : Expressions)
	{
		if (!OtherExpression)
		{
			continue;
		}

		const FString ClassName = OtherExpression->GetClass()->GetName();
		const bool bIsIndirect = ClassName.Contains(TEXT("NamedReroute")) || ClassName.Contains(TEXT("CustomOutput"));
		if (bIsIndirect)
		{
			IndirectNodes.Add(OtherExpression);
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
					if (Input && Input->Expression)
					{
						ConnectedSet.Add(Input->Expression);
						if (bIsIndirect)
						{
							IndirectSet.Add(Input->Expression);
						}
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
							if (Input && Input->Expression)
							{
								ConnectedSet.Add(Input->Expression);
								if (bIsIndirect)
								{
									IndirectSet.Add(Input->Expression);
								}
							}
						}
					}
				}
			}
		}
	}

	// Also scan the UMaterial's own root input properties (BaseColor, Roughness, Normal,
	// EmissiveColor, etc.). These use FMaterialInput (derives from FExpressionInput) — they
	// connect parameters directly to material outputs, but are NOT inside the Expressions array,
	// so the loop above misses them. Without this, used parameters appear "Unused".
	for (TFieldIterator<FProperty> PropIt(Material->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			// FMaterialInput / FColorMaterialInput / FScalarMaterialInput / FVectorMaterialInput
			// all derive from FExpressionInput and carry the same Expression field.
			const FName StructName = StructProp->Struct->GetFName();
			if (StructName == FName(TEXT("MaterialInput")) ||
				StructName == FName(TEXT("ColorMaterialInput")) ||
				StructName == FName(TEXT("ScalarMaterialInput")) ||
				StructName == FName(TEXT("VectorMaterialInput")))
			{
				FExpressionInput* Input = StructProp->ContainerPtrToValuePtr<FExpressionInput>(Material);
				if (Input && Input->Expression)
				{
					ConnectedSet.Add(Input->Expression);
				}
			}
		}
	}

	// Now classify each parameter with O(1) set lookups instead of re-scanning.
	for (TSharedPtr<FMLPParameterInfo>& Param : Parameters)
	{
		if (!Param.IsValid())
		{
			continue;
		}
		UMaterialExpression* Expr = Param->Expression.Get();
		if (!Expr)
		{
			Param->Usage = EMLPParameterUsage::Unknown;
			continue;
		}

		if (ConnectedSet.Contains(Expr))
		{
			Param->Usage = EMLPParameterUsage::Used;
		}
		else if (IndirectSet.Contains(Expr))
		{
			Param->Usage = EMLPParameterUsage::Indirect;
		}
		else
		{
			Param->Usage = EMLPParameterUsage::Unused;
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

#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* OtherExpression : Material->GetExpressions())
#else
	for (UMaterialExpression* OtherExpression : Material->Expressions)
#endif
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

#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* OtherExpression : Material->GetExpressions())
#else
	for (UMaterialExpression* OtherExpression : Material->Expressions)
#endif
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
