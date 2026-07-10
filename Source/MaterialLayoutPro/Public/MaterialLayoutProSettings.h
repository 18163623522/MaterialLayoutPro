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

/** Defines display order for a group. Groups not listed here sort alphabetically after. */
USTRUCT()
struct FMaterialLayoutProGroupOrder
{
	GENERATED_BODY()

	/** Sort order (lower = appears first). */
	UPROPERTY(Config, EditAnywhere, Category = "材质布局 Pro", meta = (ClampMin = "0"))
	int32 Order = 0;

	/** The group name as stored on the material expression (e.g. "000_Basecolor"). */
	UPROPERTY(Config, EditAnywhere, Category = "材质布局 Pro")
	FString GroupName;

	/** Display name shown in the panel (e.g. "Basecolor"). If empty, uses GroupName with prefix stripped. */
	UPROPERTY(Config, EditAnywhere, Category = "材质布局 Pro")
	FString DisplayName;
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
	UPROPERTY(Config, EditAnywhere, Category = "Auto Grouping", meta = (TitleProperty = "{Prefix} -> {Group}"))
	TArray<FMaterialLayoutProAutoGroupRule> AutoGroupRules;

	/** Group display order. Groups not listed sort alphabetically after listed ones. */
	UPROPERTY(Config, EditAnywhere, Category = "Auto Grouping", meta = (TitleProperty = "{Order}: {GroupName} -> {DisplayName}"))
	TArray<FMaterialLayoutProGroupOrder> GroupOrder;

	/** Group name used by "归档未使用". */
	UPROPERTY(Config, EditAnywhere, Category = "Cleanup")
	FString DeprecatedGroupName;

	/** Whether the CSV export should include the Value column. */
	UPROPERTY(Config, EditAnywhere, Category = "导出")
	bool bExportValueColumn;
};
