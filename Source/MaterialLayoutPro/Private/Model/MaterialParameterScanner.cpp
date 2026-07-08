#include "Model/MaterialParameterScanner.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"

TArray<TSharedPtr<FMLPParameterInfo>> FMaterialParameterScanner::ScanMaterial(UMaterial* Material)
{
	TArray<TSharedPtr<FMLPParameterInfo>> Result;
	if (!Material)
	{
		return Result;
	}

#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* Expression : Material->GetExpressions())
#else
	for (UMaterialExpression* Expression : Material->Expressions)
#endif
	{
		if (!Expression)
		{
			continue;
		}

		TSharedPtr<FMLPParameterInfo> Info = CreateInfo(Expression);
		if (Info.IsValid())
		{
			Result.Add(Info);
		}
	}

	return Result;
}

TArray<TSharedPtr<FMLPParameterInfo>> FMaterialParameterScanner::ScanMaterialInstance(UMaterialInstance* MaterialInstance)
{
	if (!MaterialInstance)
	{
		return TArray<TSharedPtr<FMLPParameterInfo>>();
	}

	UMaterial* BaseMaterial = MaterialInstance->GetBaseMaterial();
	return ScanMaterial(BaseMaterial);
}

TSharedPtr<FMLPParameterInfo> FMaterialParameterScanner::CreateInfo(UMaterialExpression* Expression)
{
	EMLPParameterType Type = DetermineType(Expression);
	if (Type == EMLPParameterType::Other)
	{
		return nullptr;
	}

	TSharedPtr<FMLPParameterInfo> Info = MakeShared<FMLPParameterInfo>();
	Info->Type = Type;
	Info->Expression = Expression;
	Info->Group = GetGroup(Expression);
	Info->SortPriority = GetSortPriority(Expression);
	Info->Guid = GetGuid(Expression);
	Info->bIsInstanceEditable = GetIsInstanceEditable(Expression);
	Info->ValueString = GetValueString(Expression);

	if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		Info->Name = Scalar->ParameterName;
	}
	else if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		Info->Name = Vector->ParameterName;
	}
	else if (UMaterialExpressionTextureSampleParameter* TexSample = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
	{
		Info->Name = TexSample->ParameterName;
	}
	else if (UMaterialExpressionTextureObjectParameter* TexObj = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
	{
		Info->Name = TexObj->ParameterName;
	}
	else if (UMaterialExpressionStaticBoolParameter* BoolParam = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
	{
		Info->Name = BoolParam->ParameterName;
	}
	else if (UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
	{
		Info->Name = SwitchParam->ParameterName;
	}

	return Info;
}

EMLPParameterType FMaterialParameterScanner::DetermineType(UMaterialExpression* Expression)
{
	if (Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		return EMLPParameterType::Scalar;
	}
	if (Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		return EMLPParameterType::Vector;
	}
	if (Cast<UMaterialExpressionTextureSampleParameter>(Expression) ||
	    Cast<UMaterialExpressionTextureObjectParameter>(Expression))
	{
		return EMLPParameterType::Texture;
	}
	if (Cast<UMaterialExpressionStaticBoolParameter>(Expression))
	{
		return EMLPParameterType::StaticBool;
	}
	if (Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
	{
		return EMLPParameterType::StaticSwitch;
	}
	return EMLPParameterType::Other;
}

FName FMaterialParameterScanner::GetGroup(UMaterialExpression* Expression)
{
	if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expression))
	{
		return Param->Group;
	}
	return NAME_None;
}

int32 FMaterialParameterScanner::GetSortPriority(UMaterialExpression* Expression)
{
	if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expression))
	{
		return Param->SortPriority;
	}
	return 0;
}

FGuid FMaterialParameterScanner::GetGuid(UMaterialExpression* Expression)
{
	if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expression))
	{
		return Param->ExpressionGUID;
	}
	return FGuid();
}

bool FMaterialParameterScanner::GetIsInstanceEditable(UMaterialExpression* Expression)
{
	// For the MVP, treat all parameters as instance-editable unless they are explicitly
	// custom primitive data parameters. Texture atlas position checks are skipped.
	if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		return !Scalar->bUseCustomPrimitiveData;
	}
	if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		return !Vector->bUseCustomPrimitiveData;
	}
	return true;
}

FString FMaterialParameterScanner::GetValueString(UMaterialExpression* Expression)
{
	if (!Expression)
	{
		return FString();
	}

	if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		return FString::Printf(TEXT("%.4f"), Scalar->DefaultValue);
	}
	if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		return FString::Printf(TEXT("R:%.2f G:%.2f B:%.2f A:%.2f"),
			Vector->DefaultValue.R, Vector->DefaultValue.G,
			Vector->DefaultValue.B, Vector->DefaultValue.A);
	}
	if (UMaterialExpressionTextureSampleParameter* TexSample = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
	{
		return TexSample->Texture ? TexSample->Texture->GetName() : TEXT("None");
	}
	if (UMaterialExpressionTextureObjectParameter* TexObj = Cast<UMaterialExpressionTextureObjectParameter>(Expression))
	{
		return TexObj->Texture ? TexObj->Texture->GetName() : TEXT("None");
	}
	if (UMaterialExpressionStaticBoolParameter* BoolParam = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
	{
		return BoolParam->DefaultValue ? TEXT("True") : TEXT("False");
	}
	if (UMaterialExpressionStaticSwitchParameter* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
	{
		return SwitchParam->DefaultValue ? TEXT("True") : TEXT("False");
	}
	return FString();
}
