#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "MaterialInstanceGroupData.generated.h"

class UMaterialInstance;

/**
 * Per-material-instance custom parameter grouping, stored as AssetUserData on the MI.
 *
 * This lets the "实例分组" (instance group) panel maintain a grouping that is INDEPENDENT
 * of the parent material's expression Group — so the instance user can reorganize how
 * parameters appear without modifying the parent material (which would affect every other
 * instance referencing it).
 *
 * Resolution: a parameter's effective group = its override entry here if present, else falls
 * back to the parent material's expression Group (the default view).
 *
 * Persisted: UAssetUserData is a serialized instanced subobject, so it saves with the MI asset.
 * It shows up in the MI details panel under the (collapsed by default) "AssetUserData" advanced
 * array — non-intrusive.
 */
UCLASS(DefaultToInstanced, editinlinenew)
class MATERIALLAYOUTPRO_API UMaterialInstanceGroupData : public UAssetUserData
{
	GENERATED_BODY()

public:
	/** Get the existing group data on this MI, or create+attach one if none exists. */
	static UMaterialInstanceGroupData* GetOrCreate(UMaterialInstance* MI);

	/** Effective group for a parameter: custom override if present, else Fallback. */
	FName ResolveGroup(const FName& ParamName, const FName& Fallback) const;

	/** Set / replace a parameter's custom group. Marks the MI package dirty. */
	void SetParamGroup(const FName& ParamName, const FName& GroupName);

	/** Remove a parameter's custom override (revert to parent material group). */
	void ClearParamGroup(const FName& ParamName);

	/** Rename a group across all parameters mapped to OldName. */
	void RenameGroup(const FName& OldName, const FName& NewName);

	/** Per-group display order (user can reorder groups). */
	const TArray<FName>& GetGroupOrder() const { return GroupOrder; }
	void SetGroupOrder(TArray<FName> InOrder);

	/** Per-parameter sort index within its group (lower = higher up). 0/absent = first. */
	int32 GetParamSort(const FName& ParamName) const;
	void SetParamSort(const FName& ParamName, int32 Index);

	/** Mark the owning MI package dirty so edits persist on save. */
	void Save(UMaterialInstance* MI);

protected:
	/** Parameter name -> custom group name override. Absent entry = use parent material group. */
	UPROPERTY(EditAnywhere, Category = "MaterialLayoutPro")
	TMap<FName, FName> ParamGroups;

	/** Display order of group names. */
	UPROPERTY(EditAnywhere, Category = "MaterialLayoutPro")
	TArray<FName> GroupOrder;

	/** Parameter name -> sort index within its group. */
	UPROPERTY(EditAnywhere, Category = "MaterialLayoutPro")
	TMap<FName, int32> ParamSort;
};
