#include "MLPEditorData.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Engine/Texture.h"
#include "AssetRegistry/AssetRegistryModule.h"

UMLPEditorParameter::UMLPEditorParameter()
	: bShowScalar(false), bShowVector(false), bShowTexture(false), bShowBool(false)
{
}

void UMLPEditorParameter::PullFromExpression()
{
	auto* Expr = SourceExpression.Get();
	if (!Expr) return;

	Group = Expr->Group;
	SortPriority = Expr->SortPriority;

	if (auto* S = Cast<UMaterialExpressionScalarParameter>(Expr))
	{
		bShowScalar = true;
		ScalarValue = S->DefaultValue;
	}
	else if (auto* V = Cast<UMaterialExpressionVectorParameter>(Expr))
	{
		bShowVector = true;
		VectorValue = V->DefaultValue;
	}
	else if (auto* T = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
	{
		bShowTexture = true;
		TextureValue = T->Texture;
	}
	else if (auto* T = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
	{
		bShowTexture = true;
		TextureValue = T->Texture;
	}
	else if (auto* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
	{
		bShowBool = true;
		BoolValue = B->DefaultValue;
	}
	else if (auto* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
	{
		bShowBool = true;
		BoolValue = Sw->DefaultValue;
	}
}

void UMLPEditorParameter::PushToExpression()
{
	auto* Expr = SourceExpression.Get();
	if (!Expr) return;

	Expr->Modify();
	Expr->Group = Group;
	Expr->SortPriority = SortPriority;

	if (auto* S = Cast<UMaterialExpressionScalarParameter>(Expr))
	{
		S->DefaultValue = ScalarValue;
	}
	else if (auto* V = Cast<UMaterialExpressionVectorParameter>(Expr))
	{
		V->DefaultValue = VectorValue;
	}
	else if (auto* T = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
	{
		T->Texture = TextureValue.LoadSynchronous();
	}
	else if (auto* T = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
	{
		T->Texture = TextureValue.LoadSynchronous();
	}
	else if (auto* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
	{
		B->DefaultValue = BoolValue;
	}
	else if (auto* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
	{
		Sw->DefaultValue = BoolValue;
	}
}

void UMLPEditorData::BuildFromMaterial(UMaterial* InMaterial)
{
	TargetMaterial = InMaterial;
	ParameterGroups.Reset();

	if (!InMaterial) return;

	// Collect all parameter expressions.
	TMap<FName, int32> GroupIndexMap;

	auto AddParam = [&](UMaterialExpressionParameter* ParamExpr, const FName& TypeName)
	{
		FName GroupName = ParamExpr->Group.IsNone() ? FName(TEXT("(None)")) : ParamExpr->Group;

		int32* Found = GroupIndexMap.Find(GroupName);
		if (!Found)
		{
			FMLPEditorParameterGroup NewGroup;
			NewGroup.GroupName = GroupName;
			ParameterGroups.Add(NewGroup);
			Found = &GroupIndexMap.Add(GroupName, ParameterGroups.Num() - 1);
		}

		UMLPEditorParameter* Wrapper = NewObject<UMLPEditorParameter>(this);
		Wrapper->DisplayName = FText::FromName(ParamExpr->ParameterName);
		Wrapper->ParameterType = TypeName;
		Wrapper->SourceExpression = ParamExpr;
		Wrapper->PullFromExpression();
		ParameterGroups[*Found].Parameters.Add(Wrapper);
	};

#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* Expr : InMaterial->GetExpressions())
#else
	for (UMaterialExpression* Expr : InMaterial->Expressions)
#endif
	{
		if (!Expr) continue;

		if (Cast<UMaterialExpressionScalarParameter>(Expr))
			AddParam(Cast<UMaterialExpressionScalarParameter>(Expr), TEXT("标量"));
		else if (Cast<UMaterialExpressionVectorParameter>(Expr))
			AddParam(Cast<UMaterialExpressionVectorParameter>(Expr), TEXT("向量"));
		else if (Cast<UMaterialExpressionTextureSampleParameter>(Expr) || Cast<UMaterialExpressionTextureObjectParameter>(Expr))
			AddParam(Cast<UMaterialExpressionParameter>(Expr), TEXT("纹理"));
		else if (Cast<UMaterialExpressionStaticBoolParameter>(Expr))
			AddParam(Cast<UMaterialExpressionStaticBoolParameter>(Expr), TEXT("静态布尔"));
		else if (Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
			AddParam(Cast<UMaterialExpressionStaticSwitchParameter>(Expr), TEXT("静态开关"));
	}

	// Sort groups by name.
	ParameterGroups.Sort([](const FMLPEditorParameterGroup& A, const FMLPEditorParameterGroup& B)
	{
		return A.GroupName.ToString() < B.GroupName.ToString();
	});
}

void UMLPEditorData::PushAllToMaterial()
{
	if (!TargetMaterial.IsValid()) return;

	for (FMLPEditorParameterGroup& Group : ParameterGroups)
	{
		for (UMLPEditorParameter* Param : Group.Parameters)
		{
			if (Param) Param->PushToExpression();
		}
	}

	if (UMaterial* Mat = TargetMaterial.Get())
	{
		Mat->PostEditChange();
		Mat->MarkPackageDirty();
	}
}
