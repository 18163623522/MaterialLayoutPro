#include "Model/MaterialParameterInfo.h"
#include "MaterialLayoutProTheme.h"

FText FMLPParameterInfo::GetDisplayTypeName() const
{
	static const UEnum* Enum = StaticEnum<EMLPParameterType>();
	return Enum ? Enum->GetDisplayNameTextByValue((int64)Type) : FText::GetEmpty();
}

FLinearColor FMLPParameterInfo::GetTypeColor() const
{
	switch (Type)
	{
	case EMLPParameterType::Scalar:       return FMLPTheme::TypeScalar();
	case EMLPParameterType::Vector:       return FMLPTheme::TypeVector();
	case EMLPParameterType::Texture:      return FMLPTheme::TypeTexture();
	case EMLPParameterType::StaticBool:
	case EMLPParameterType::StaticSwitch: return FMLPTheme::TypeStatic();
	default:                              return FMLPTheme::Muted();
	}
}

FText FMLPParameterInfo::GetUsageLabel() const
{
	static const UEnum* Enum = StaticEnum<EMLPParameterUsage>();
	return Enum ? Enum->GetDisplayNameTextByValue((int64)Usage) : FText::GetEmpty();
}

FLinearColor FMLPParameterInfo::GetUsageColor() const
{
	switch (Usage)
	{
	case EMLPParameterUsage::Used:     return FMLPTheme::StatusUsed();
	case EMLPParameterUsage::Unused:     return FMLPTheme::StatusUnused();
	case EMLPParameterUsage::HalfUsed:   return FMLPTheme::StatusHalfUsed();
	case EMLPParameterUsage::Indirect:   return FMLPTheme::StatusIndirect();
	default:                             return FMLPTheme::StatusUnknown();
	}
}

FLinearColor FMLPParameterInfo::GetUsageBgColor() const
{
	FLinearColor Color = GetUsageColor();
	Color.A = 0.18f;
	return Color;
}
