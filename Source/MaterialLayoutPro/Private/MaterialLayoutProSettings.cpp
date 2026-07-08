#include "MaterialLayoutProSettings.h"

UMaterialLayoutProSettings::UMaterialLayoutProSettings()
{
	// Seed defaults that mirror the original hardcoded rules.
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("MF_"),  TEXT("Master Faders")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("Tex_"), TEXT("Textures")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("Color_"),TEXT("Colors")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("R_"),   TEXT("Channels")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("G_"),   TEXT("Channels")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("B_"),   TEXT("Channels")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("A_"),   TEXT("Channels")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("N_"),   TEXT("Normals")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("Em_"),  TEXT("Emissive")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("Rgh_"), TEXT("Roughness")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("Met_"), TEXT("Metallic")});
	AutoGroupRules.Add(FMaterialLayoutProAutoGroupRule{TEXT("Sp_"),  TEXT("Specular")});

	DeprecatedGroupName = TEXT("Deprecated");
	bExportValueColumn = true;
}

FName UMaterialLayoutProSettings::GetSectionName() const
{
	return FName(TEXT("材质布局 Pro"));
}

FName UMaterialLayoutProSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
