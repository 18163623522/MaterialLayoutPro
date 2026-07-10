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

    // Sort groups: by user-defined GroupOrder, or by numeric prefix in the group name,
    // then alphabetically. Also strip numeric prefix for display.
    {
        const auto* Settings = GetDefault<UMaterialLayoutProSettings>();
        TMap<FName, int32> OrderMap;
        TMap<FName, FString> DisplayNameMap;
        if (Settings)
        {
            for (const auto& Rule : Settings->GroupOrder)
            {
                OrderMap.Add(FName(*Rule.GroupName), Rule.Order);
                if (!Rule.DisplayName.IsEmpty())
                    DisplayNameMap.Add(FName(*Rule.GroupName), Rule.DisplayName);
            }
        }

        // Helper: extract numeric prefix from group name (e.g. "000_BaseColor" -> 0).
        auto ExtractNumericPrefix = [](const FString& Name) -> int32
        {
            int32 Idx;
            if (Name.FindChar('_', Idx) && Idx > 0)
            {
                FString Prefix = Name.Left(Idx);
                if (!Prefix.IsEmpty() && Prefix[0] >= '0' && Prefix[0] <= '9')
                {
                    int32 Val = 0;
                    for (TCHAR C : Prefix)
                    {
                        if (C >= '0' && C <= '9') Val = Val * 10 + (C - '0');
                        else { Val = -1; break; }
                    }
                    if (Val >= 0) return Val;
                }
            }
            return -1;
        };

        Groups.Sort([&](const TSharedPtr<FMLPGroupVM>& A, const TSharedPtr<FMLPGroupVM>& B)
        {
            const FString& NameA = A->Name.ToString();
            const FString& NameB = B->Name.ToString();

            // 1. User-defined GroupOrder takes highest priority.
            int32 OrderA = OrderMap.Contains(A->Name) ? OrderMap[A->Name] : -1;
            int32 OrderB = OrderMap.Contains(B->Name) ? OrderMap[B->Name] : -1;
            if (OrderA >= 0 && OrderB >= 0 && OrderA != OrderB) return OrderA < OrderB;
            if (OrderA >= 0 && OrderB < 0) return true;
            if (OrderA < 0 && OrderB >= 0) return false;

            // 2. Fallback: numeric prefix in group name (e.g. "000_X" < "010_Y").
            int32 NumA = ExtractNumericPrefix(NameA);
            int32 NumB = ExtractNumericPrefix(NameB);
            if (NumA >= 0 && NumB >= 0 && NumA != NumB) return NumA < NumB;
            if (NumA >= 0 && NumB < 0) return true;
            if (NumA < 0 && NumB >= 0) return false;

            // 3. Final fallback: alphabetical.
            return NameA < NameB;
        });

        // Apply display names: strip "NNN_" prefix or use user-defined DisplayName.
        for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
        {
            FString* CustomName = DisplayNameMap.Find(Group->Name);
            if (CustomName && !CustomName->IsEmpty())
            {
                Group->DisplayName = *CustomName;
            }
            else
            {
                // Auto-strip numeric prefix: "000_Basecolor" -> "Basecolor"
                FString NameStr = Group->Name.ToString();
                int32 Idx;
                if (NameStr.FindChar('_', Idx) && Idx > 0)
                {
                    FString Prefix = NameStr.Left(Idx);
                    bool bAllDigits = !Prefix.IsEmpty();
                    for (TCHAR C : Prefix) { if (C < '0' || C > '9') { bAllDigits = false; break; } }
                    if (bAllDigits) Group->DisplayName = NameStr.RightChop(Idx + 1);
                    else Group->DisplayName = NameStr;
                }
                else
                {
                    Group->DisplayName = NameStr;
                }
            }
        }
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
