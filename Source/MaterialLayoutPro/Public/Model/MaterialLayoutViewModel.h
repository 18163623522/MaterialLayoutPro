#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Model/MaterialParameterInfo.h"
#include "Materials/MaterialExpression.h"

DECLARE_MULTICAST_DELEGATE(FOnMaterialChangedBySession);

class UMaterial;
class UMaterialExpressionParameter;
class UMaterialExpressionScalarParameter;
class UMaterialExpressionVectorParameter;
class UMaterialExpressionTextureSampleParameter;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionStaticBoolParameter;
class UMaterialExpressionStaticSwitchParameter;
class UTexture;

/**
 * Editable snapshot of a single material parameter.
 *
 * UI controls bind their TAttribute to these fields. The control instance stays
 * stable across refreshes (no rebuild), which prevents focus loss.
 *
 * Design: plain struct + TSharedPtr (no UHT dependency, 4.26 friendly).
 * Pull reads from the source expression into the snapshot; Push writes back.
 */
struct FMLPParamVM
{
    /** Identity (copied from FMLPParameterInfo, does not change). */
    FName Name;
    EMLPParameterType Type = EMLPParameterType::Other;
    FGuid Guid;

    /** Source expression — the engine object this VM mirrors. */
    TWeakObjectPtr<UMaterialExpression> SourceExpression;

    // --- Value snapshot (editable, use the one matching Type) ---

    float       ScalarValue  = 0.f;
    FLinearColor VectorValue = FLinearColor::White;
    TSoftObjectPtr<UTexture> TextureValue;
    bool        BoolValue    = false;

    // --- Organization (editable) ---

    FName   Group;
    int32   SortPriority = 0;

    // --- Status (read-only display) ---

    EMLPParameterUsage Usage = EMLPParameterUsage::Unknown;
    bool bIsInstanceEditable = false;
    bool bHasDuplicateName = false;

    // --- Edit state ---

    /** True if this VM has uncommitted changes since the last Push. */
    bool bDirty = false;

    /** Read current values from the source expression into this snapshot. */
    void PullFromExpression();

    /** Write snapshot values back to the source expression (calls Expr->Modify()). */
    void PushToExpression();

    // --- Display helpers (for UI badges) ---

    /** Usage status label text. */
    FText GetUsageLabel() const;
    /** Usage status foreground color. */
    FLinearColor GetUsageColor() const;
    /** Usage status background color. */
    FLinearColor GetUsageBgColor() const;
};

/**
 * A group of parameter VMs. Maps to a material Group.
 */
struct FMLPGroupVM
{
    FName Name;
    int32 SortPriority = 0;
    bool bExpanded = true;
    TArray<TSharedPtr<FMLPParamVM>> Parameters;
};

/**
 * Manages a complete snapshot of one material's parameters + the interaction lock.
 *
 * The interaction lock prevents external refreshes from interrupting user input:
 *   - BeginInteract() when a control gains focus  -> InteractingCount++
 *   - EndInteract()   when a control loses focus   -> InteractingCount--
 *   - PullAll() only proceeds when InteractingCount == 0; otherwise defers.
 */
class FMLPSession
{
public:
    /** The material this session mirrors. */
    TWeakObjectPtr<UMaterial> TargetMaterial;

    /** All groups (each containing parameter VMs). */
    TArray<TSharedPtr<FMLPGroupVM>> Groups;

    // --- Interaction lock ---

    /** Number of controls currently being interacted with. >0 = block refresh. */
    int32 InteractingCount = 0;

    /** A refresh was requested while InteractingCount > 0; run it after count returns to 0. */
    bool bPendingRefresh = false;

    // --- Snapshot lifecycle ---

    /** Rebuild the entire VM tree from the material. No-op if InteractingCount > 0 (sets bPendingRefresh). */
    void PullAll();

    /** Write all bDirty parameters back in a single FScopedTransaction. Clears bDirty. */
    void PushDirty();

    /** Immediately write ONE parameter back to its expression + refresh the material.
     *  Used for live editing: the value control commits, this pushes it straight away
     *  so the change shows up in the viewport without an explicit "Apply" step. */
    void PushParamNow(TSharedPtr<FMLPParamVM> Param);

    /** Check if any parameter has uncommitted changes. */
    bool HasDirty() const;

    // --- Interaction lock API (called by UI controls) ---

    void BeginInteract();
    void EndInteract();

    /** Broadcast after any write-back (PushParamNow/PushDirty) so bound editors can
     *  refresh their node UI / details panel (NotifyExternalMaterialChange). */
    FOnMaterialChangedBySession& OnMaterialChanged() { return MaterialChangedDelegate; }

private:
    FOnMaterialChangedBySession MaterialChangedDelegate;
};
