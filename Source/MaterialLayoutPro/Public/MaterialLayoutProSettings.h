#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MaterialLayoutProSettings.generated.h"

/** A single auto-group rule mapping a parameter-name prefix to a target group. */
USTRUCT()
struct FMaterialLayoutProAutoGroupRule
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "材质布局 Pro")
	FString Prefix;

	UPROPERTY(Config, EditAnywhere, Category = "材质布局 Pro")
	FString Group;
};

/**
 * Project-wide configuration for the Material Layout Pro plugin.
 * Exposed under Project Settings → Plugins → Material Layout Pro.
 */
UCLASS(Config = MaterialLayoutPro, defaultconfig, meta = (DisplayName = "材质布局 Pro"))
class UMaterialLayoutProSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMaterialLayoutProSettings();

	//~ Begin UDeveloperSettings
	virtual FName GetSectionName() const override;
	virtual FName GetCategoryName() const override;
	//~ End UDeveloperSettings

	/** Prefix-to-group rules used by the "自动分组" action. */
	UPROPERTY(Config, EditAnywhere, Category = "Auto Grouping", meta = (TitleProperty = "{Prefix} → {Group}"))
	TArray<FMaterialLayoutProAutoGroupRule> AutoGroupRules;

	/** Group name used by "归档未使用". */
	UPROPERTY(Config, EditAnywhere, Category = "Cleanup")
	FString DeprecatedGroupName;

	/** Whether the CSV export should include the Value column. */
	UPROPERTY(Config, EditAnywhere, Category = "导出")
	bool bExportValueColumn;
};
