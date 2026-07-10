#include "MaterialInstanceGroupData.h"
#include "Materials/MaterialInstance.h"

UMaterialInstanceGroupData* UMaterialInstanceGroupData::GetOrCreate(UMaterialInstance* MI)
{
	if (!MI) return nullptr;

	// IInterface_AssetUserData: get existing, or create + attach.
	if (UAssetUserData* Existing = MI->GetAssetUserDataOfClass(StaticClass()))
	{
		return Cast<UMaterialInstanceGroupData>(Existing);
	}

	// None yet — create a new one and add it to the MI's AssetUserData array.
	UMaterialInstanceGroupData* NewData = NewObject<UMaterialInstanceGroupData>(MI, StaticClass(), NAME_None, RF_Transactional);
	MI->AddAssetUserData(NewData);
	return NewData;
}

FName UMaterialInstanceGroupData::ResolveGroup(const FName& ParamName, const FName& Fallback) const
{
	if (const FName* Found = ParamGroups.Find(ParamName))
	{
		return *Found;
	}
	return Fallback;
}

void UMaterialInstanceGroupData::SetParamGroup(const FName& ParamName, const FName& GroupName)
{
	ParamGroups.Add(ParamName, GroupName);

	// Ensure the group is registered in GroupOrder (append to end if new).
	if (!GroupOrder.Contains(GroupName))
	{
		GroupOrder.Add(GroupName);
	}
}

void UMaterialInstanceGroupData::ClearParamGroup(const FName& ParamName)
{
	ParamGroups.Remove(ParamName);
}

void UMaterialInstanceGroupData::RenameGroup(const FName& OldName, const FName& NewName)
{
	for (auto& Pair : ParamGroups)
	{
		if (Pair.Value == OldName)
		{
			Pair.Value = NewName;
		}
	}
	// Keep GroupOrder consistent.
	const int32 Idx = GroupOrder.Find(OldName);
	if (Idx != INDEX_NONE)
	{
		GroupOrder[Idx] = NewName;
	}
	else if (!GroupOrder.Contains(NewName))
	{
		GroupOrder.Add(NewName);
	}
}

void UMaterialInstanceGroupData::SetGroupOrder(TArray<FName> InOrder)
{
	GroupOrder = MoveTemp(InOrder);
}

int32 UMaterialInstanceGroupData::GetParamSort(const FName& ParamName) const
{
	if (const int32* Found = ParamSort.Find(ParamName))
	{
		return *Found;
	}
	return 0;
}

void UMaterialInstanceGroupData::SetParamSort(const FName& ParamName, int32 Index)
{
	ParamSort.Add(ParamName, Index);
}

void UMaterialInstanceGroupData::Save(UMaterialInstance* MI)
{
	if (!MI) return;
	MI->Modify();
	this->Modify();
	MI->MarkPackageDirty();
	MI->PostEditChange();
}
