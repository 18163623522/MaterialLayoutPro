#include "Model/MaterialLayoutViewModel.h"
#include "MaterialLayoutProTheme.h"
#include "MaterialLayoutProSettings.h"
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
#include "Model/MaterialParameterScanner.h"
#include "Model/MaterialParameterUsageAnalyzer.h"
#include "ScopedTransaction.h"

// ============================================================================
// FMLPParamVM — Pull/Push
// ============================================================================

void FMLPParamVM::PullFromExpression()
{
    UMaterialExpression* Expr = SourceExpression.Get();
    if (!Expr)
    {
        return;
    }

    // Group + SortPriority are on the base UMaterialExpressionParameter.
    if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
    {
        Group = Param->Group;
        SortPriority = Param->SortPriority;
    }

    // Value depends on the concrete expression type.
    if (UMaterialExpressionScalarParameter* S = Cast<UMaterialExpressionScalarParameter>(Expr))
    {
        ScalarValue = S->DefaultValue;
    }
    else if (UMaterialExpressionVectorParameter* V = Cast<UMaterialExpressionVectorParameter>(Expr))
    {
        VectorValue = V->DefaultValue;
    }
    else if (UMaterialExpressionTextureSampleParameter* TS = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
    {
        TextureValue = TS->Texture;
    }
    else if (UMaterialExpressionTextureObjectParameter* TO = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
    {
        TextureValue = TO->Texture;
    }
    else if (UMaterialExpressionStaticBoolParameter* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
    {
        BoolValue = B->DefaultValue;
    }
    else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
    {
        BoolValue = Sw->DefaultValue;
    }
}

void FMLPParamVM::PushToExpression()
{
    UMaterialExpression* Expr = SourceExpression.Get();
    if (!Expr)
    {
        return;
    }

    if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
    {
        Param->Modify();
        Param->Group = Group;
        Param->SortPriority = SortPriority;
    }

    if (UMaterialExpressionScalarParameter* S = Cast<UMaterialExpressionScalarParameter>(Expr))
    {
        S->DefaultValue = ScalarValue;
    }
    else if (UMaterialExpressionVectorParameter* V = Cast<UMaterialExpressionVectorParameter>(Expr))
    {
        V->DefaultValue = VectorValue;
    }
    else if (UMaterialExpressionTextureSampleParameter* TS = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
    {
        TS->Texture = TextureValue.LoadSynchronous();
    }
    else if (UMaterialExpressionTextureObjectParameter* TO = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
    {
        TO->Texture = TextureValue.LoadSynchronous();
    }
    else if (UMaterialExpressionStaticBoolParameter* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
    {
        B->DefaultValue = BoolValue;
    }
    else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
    {
        Sw->DefaultValue = BoolValue;
    }

    bDirty = false;
}

// ============================================================================
// FMLPParamVM — Display helpers
// ============================================================================

FText FMLPParamVM::GetUsageLabel() const
{
    switch (Usage)
    {
    case EMLPParameterUsage::Used:     return FText::FromString(TEXT("已使用"));
    case EMLPParameterUsage::Unused:   return FText::FromString(TEXT("未使用"));
    case EMLPParameterUsage::HalfUsed: return FText::FromString(TEXT("部分"));
    case EMLPParameterUsage::Indirect: return FText::FromString(TEXT("间接"));
    default:                           return FText::FromString(TEXT("未知"));
    }
}

FLinearColor FMLPParamVM::GetUsageColor() const
{
    switch (Usage)
    {
    case EMLPParameterUsage::Used:     return FMLPTheme::StatusUsed();
    case EMLPParameterUsage::Unused:   return FMLPTheme::StatusUnused();
    case EMLPParameterUsage::HalfUsed: return FMLPTheme::StatusHalfUsed();
    case EMLPParameterUsage::Indirect: return FMLPTheme::StatusIndirect();
    default:                           return FMLPTheme::StatusUnknown();
    }
}

FLinearColor FMLPParamVM::GetUsageBgColor() const
{
    // Background = foreground color with low alpha.
    FLinearColor C = GetUsageColor();
    C.A = 0.25f;
    return C;
}

// ============================================================================
// FMLPSession
// ============================================================================

void FMLPSession::PullAll()
{
    // Interaction lock: if a control is being edited, defer the refresh.
    if (InteractingCount > 0)
    {
        bPendingRefresh = true;
        return;
    }

    Groups.Reset();

    UMaterial* Mat = TargetMaterial.Get();
    if (!Mat) return;

    // Scan the material (reuse existing scanner - it's stable and tested).
    TArray<TSharedPtr<FMLPParameterInfo>> Params = FMaterialParameterScanner::ScanMaterial(Mat);
    FMaterialParameterUsageAnalyzer::Analyze(Mat, Params);

    // Group parameters by their Group name.
    TMap<FName, int32> GroupIndexMap;

    for (const TSharedPtr<FMLPParameterInfo>& Info : Params)
    {
        if (!Info.IsValid() || !Info->Expression.IsValid())
        {
            continue;
        }

        FName GroupName = Info->Group.IsNone() ? FName(TEXT("(None)")) : Info->Group;

        // Find or create the group.
        int32* FoundIndex = GroupIndexMap.Find(GroupName);
        if (!FoundIndex)
        {
            TSharedPtr<FMLPGroupVM> NewGroup = MakeShared<FMLPGroupVM>();
            NewGroup->Name = GroupName;
            Groups.Add(NewGroup);
            FoundIndex = &GroupIndexMap.Add(GroupName, Groups.Num() - 1);
        }

        // Create the parameter VM and pull its values.
        TSharedPtr<FMLPParamVM> ParamVM = MakeShared<FMLPParamVM>();
        ParamVM->Name = Info->Name;
        ParamVM->Type = Info->Type;
        ParamVM->Guid = Info->Guid;
        ParamVM->Usage = Info->Usage;
        ParamVM->bIsInstanceEditable = Info->bIsInstanceEditable;
        ParamVM->bHasDuplicateName = Info->bHasDuplicateName;
        ParamVM->SourceExpression = Info->Expression;
        ParamVM->PullFromExpression();

        Groups[*FoundIndex]->Parameters.Add(ParamVM);
    }

    // Sort groups: by SortPriority (editable in the panel), then alphabetically.
    // DisplayName = the group name as-is (no prefix stripping).
    Groups.Sort([](const TSharedPtr<FMLPGroupVM>& A, const TSharedPtr<FMLPGroupVM>& B)
    {
        if (A->SortPriority != B->SortPriority) return A->SortPriority < B->SortPriority;
        return A->Name.ToString() < B->Name.ToString();
    });

    for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        Group->DisplayName = Group->Name.ToString();
    }

    // Sort parameters within each group by SortPriority (then Name as tiebreaker).
    // This makes SortPriority changes from drag-drop / inline editing visible in the panel.
    for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        Group->Parameters.Sort([](const TSharedPtr<FMLPParamVM>& A, const TSharedPtr<FMLPParamVM>& B)
        {
            if (A->SortPriority != B->SortPriority) return A->SortPriority < B->SortPriority;
            return A->Name.ToString() < B->Name.ToString();
        });
    }

    bPendingRefresh = false;
}

void FMLPSession::PushDirty()
{
    UMaterial* Mat = TargetMaterial.Get();
    if (!Mat)
    {
        return;
    }

    // Collect dirty parameters first.
    bool bAnyDirty = false;
    for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
        {
            if (Param.IsValid() && Param->bDirty)
            {
                bAnyDirty = true;
                break;
            }
        }
        if (bAnyDirty) break;
    }

    if (!bAnyDirty)
    {
        return;
    }

    // Single transaction for all changes.
    const FScopedTransaction Transaction(FText::FromString(TEXT("修改材质参数")));

    for (TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        for (TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
        {
            if (Param.IsValid() && Param->bDirty)
            {
                Param->PushToExpression();
            }
        }
    }

    Mat->PostEditChange();
    Mat->MarkPackageDirty();
    MaterialChangedDelegate.Broadcast();
}

void FMLPSession::PushParamNow(TSharedPtr<FMLPParamVM> Param)
{
    UMaterial* Mat = TargetMaterial.Get();
    if (!Mat || !Param.IsValid() || !Param->bDirty)
    {
        return;
    }

    // Single-param transaction + immediate write-back so the viewport updates live.
    const FScopedTransaction Transaction(FText::FromString(TEXT("修改材质参数")));
    Param->PushToExpression();
    Mat->PostEditChange();
    Mat->MarkPackageDirty();
    MaterialChangedDelegate.Broadcast();
}

bool FMLPSession::HasDirty() const
{
    for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
        {
            if (Param.IsValid() && Param->bDirty)
            {
                return true;
            }
        }
    }
    return false;
}

void FMLPSession::BeginInteract()
{
    ++InteractingCount;
}

void FMLPSession::EndInteract()
{
    if (InteractingCount > 0)
    {
        --InteractingCount;
    }

    // When all interactions end, process any deferred refresh.
    if (InteractingCount == 0 && bPendingRefresh)
    {
        PullAll();
    }
}
