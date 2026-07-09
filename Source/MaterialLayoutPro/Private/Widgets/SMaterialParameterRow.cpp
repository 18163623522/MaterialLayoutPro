#include "Widgets/SMaterialParameterRow.h"
#include "MaterialLayoutProTheme.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Styling/CoreStyle.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_STYLE FAppStyle
#else
#include "EditorStyleSet.h"
#define MLP_STYLE FEditorStyle
#endif
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#define LOCTEXT_NAMESPACE "SMaterialParameterRow"

void SMaterialParameterRow::Construct(const FArguments& InArgs)
{
	VM = InArgs._ParamVM;
	Session = InArgs._Session;
	bSelected = InArgs._bSelected;
	bDetailMode = InArgs._bDetailMode;

	if (!VM.IsValid())
	{
		ChildSlot
		[
			SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效"))
		];
		return;
	}

	const FLinearColor TypeColor = FMLPTheme::GetTypeColorForType(VM->Type);
	const FText TypeAbbr = FMLPTheme::GetTypeAbbrForType(VM->Type);
	const FLinearColor NameColor = VM->bHasDuplicateName ? FMLPTheme::Warning() : FMLPTheme::Foreground();

	// Background reads VM selection-like state via bSelected (set at construct time).
	auto GetBgColor = [this]() -> FLinearColor
	{
		return bSelected
			? FLinearColor(FMLPTheme::SelectionBg().R, FMLPTheme::SelectionBg().G, FMLPTheme::SelectionBg().B, 0.35f)
			: FLinearColor(FMLPTheme::Surface().R, FMLPTheme::Surface().G, FMLPTheme::Surface().B, 0.6f);
	};

	// Group editor: editable in detail mode, hidden in compact row mode.
	TSharedRef<SWidget> GroupWidget = bDetailMode
		? StaticCastSharedRef<SWidget>(
			SNew(SEditableTextBox)
			.Text(FText::FromName(VM->Group))
			.Font(FMLPTheme::FontBody())
			.OnTextCommitted(this, &SMaterialParameterRow::OnGroupCommitted))
		: StaticCastSharedRef<SWidget>(SNew(SBox));

	// Priority editor: editable in detail mode only.
	TSharedRef<SWidget> PriorityWidget = bDetailMode
		? StaticCastSharedRef<SWidget>(
			SNew(SBox).WidthOverride(60.f)
			[
				SNew(SNumericEntryBox<int32>)
				.Value(TOptional<int32>(VM->SortPriority))
				.Font(FMLPTheme::FontBody())
				.OnValueCommitted(this, &SMaterialParameterRow::OnPriorityCommitted)
			])
		: StaticCastSharedRef<SWidget>(SNew(SBox));

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor_Lambda(GetBgColor)
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(8.f, 3.f, 6.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				FMLPTheme::MakeTypePill(TypeAbbr, TypeColor)
			]
			+ SHorizontalBox::Slot().FillWidth(bDetailMode ? 0.22f : 0.30f).VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor::Transparent)
				.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
				.Padding(FMargin(0.f))
				[
					SNew(STextBlock)
					.Text(FText::FromName(VM->Name))
					.Font(FMLPTheme::FontBody())
					.ColorAndOpacity(NameColor)
					.ToolTipText(MakeDiagnosticTooltip())
				]
			]
			+ SHorizontalBox::Slot().FillWidth(bDetailMode ? 0.22f : 0.40f).VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				BuildValueEditor()
			]
			+ SHorizontalBox::Slot().FillWidth(0.20f).VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				GroupWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				PriorityWidget
			]
			+ SHorizontalBox::Slot().FillWidth(bDetailMode ? 0.13f : 0.15f).VAlign(VAlign_Center)
			.Padding(FMLPTheme::PadSM())
			[
				SNew(SBorder)
				.BorderBackgroundColor(VM->GetUsageBgColor())
				.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
				.Padding(FMargin(4.f, 1.f))
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(VM->GetUsageLabel())
					.Font(FMLPTheme::FontBadge())
					.ColorAndOpacity(VM->GetUsageColor())
				]
			]
		]
	];
}

TSharedRef<SWidget> SMaterialParameterRow::BuildValueEditor()
{
	if (!VM.IsValid())
	{
		return SNew(STextBlock).Text(FText::GetEmpty());
	}

	TWeakPtr<FMLPParamVM> WeakVM = VM;

	switch (VM->Type)
	{
	case EMLPParameterType::Scalar:
	{
		return SNew(SNumericEntryBox<float>)
			.Value_Lambda([WeakVM]() -> TOptional<float>
			{
				if (auto V = WeakVM.Pin()) return TOptional<float>(V->ScalarValue);
				return TOptional<float>();
			})
			.Font(FMLPTheme::FontBody())
			.AllowSpin(true)
			.MinValue(TOptional<float>())
			.MaxValue(TOptional<float>())
			.MinSliderValue(TOptional<float>())
			.MaxSliderValue(TOptional<float>())
			.OnValueChanged(this, &SMaterialParameterRow::OnScalarDragged)
			.OnValueCommitted(this, &SMaterialParameterRow::OnScalarCommitted);
	}
	case EMLPParameterType::Vector:
	{
		return SNew(SColorBlock)
			.Color_Lambda([WeakVM]() -> FLinearColor
			{
				if (auto V = WeakVM.Pin()) return V->VectorValue;
				return FLinearColor::White;
			})
			.OnMouseButtonDown_Lambda([this, WeakVM](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
			{
				if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
				auto V = WeakVM.Pin();
				if (!V.IsValid()) return FReply::Unhandled();
				FColorPickerArgs Args;
				Args.bUseAlpha = true;
#if ENGINE_MAJOR_VERSION >= 5
				Args.InitialColor = V->VectorValue;
#else
				Args.InitialColorOverride = V->VectorValue;
#endif
				Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, V](FLinearColor NewColor)
				{
					OnVectorChanged(NewColor);
				});
				OpenColorPicker(Args);
				return FReply::Handled();
			});
	}
	case EMLPParameterType::Texture:
	{
		auto GetTexName = [WeakVM]() -> FText
		{
			if (auto V = WeakVM.Pin())
				if (UTexture* T = V->TextureValue.Get())
					return FText::FromString(T->GetName());
			return FText::FromString(TEXT("（无）"));
		};
		return SNew(SButton)
			.Text_Lambda(GetTexName)
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ContentPadding(FMargin(2.f, 0.f))
			.HAlign(HAlign_Left)
			.OnClicked_Lambda([this]() -> FReply
			{
				if (!VM.IsValid()) return FReply::Handled();
				FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				FAssetPickerConfig Config;
#if ENGINE_MAJOR_VERSION >= 5
				Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
#else
				Config.Filter.ClassNames.Add(TEXT("Texture2D"));
#endif
				Config.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData) -> void
				{
					if (VM.IsValid() && AssetData.IsValid())
					{
						UObject* Asset = AssetData.GetAsset();
						if (!Asset)
						{
							const FString Path = AssetData.PackageName.ToString() / AssetData.AssetName.ToString();
							Asset = LoadObject<UObject>(nullptr, *Path);
						}
						if (Asset)
						{
							OnTextureChanged(Asset);
						}
					}
					FSlateApplication::Get().DismissAllMenus();
				});
				Config.bAllowNullSelection = false;
				Config.InitialAssetViewType = EAssetViewType::List;

				TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
				FSlateApplication::Get().PushMenu(
					AsShared(),
					FWidgetPath(),
					Picker,
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
				return FReply::Handled();
			});
	}
	case EMLPParameterType::StaticBool:
	case EMLPParameterType::StaticSwitch:
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([WeakVM]() -> ECheckBoxState
			{
				if (auto V = WeakVM.Pin()) return V->BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				return ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				OnBoolChanged(State == ECheckBoxState::Checked);
			});
	}
	default:
		return SNew(STextBlock)
			.Text(FText::FromString(TEXT("(不支持)")))
			.Font(FMLPTheme::FontSmall())
			.ColorAndOpacity(FMLPTheme::Muted());
	}
}

void SMaterialParameterRow::OnScalarDragged(float NewValue)
{
	if (VM.IsValid()) { VM->ScalarValue = NewValue; VM->bDirty = true; }
}

void SMaterialParameterRow::OnScalarCommitted(float NewValue, ETextCommit::Type CommitType)
{
	if (VM.IsValid()) { VM->ScalarValue = NewValue; VM->bDirty = true; }
}

void SMaterialParameterRow::OnVectorChanged(FLinearColor NewColor)
{
	if (VM.IsValid()) { VM->VectorValue = NewColor; VM->bDirty = true; }
}

void SMaterialParameterRow::OnTextureChanged(UObject* NewTexture)
{
	if (VM.IsValid())
	{
		VM->TextureValue = Cast<UTexture>(NewTexture);
		VM->bDirty = true;
	}
}

void SMaterialParameterRow::OnBoolChanged(bool bNewValue)
{
	if (VM.IsValid()) { VM->BoolValue = bNewValue; VM->bDirty = true; }
}

void SMaterialParameterRow::OnGroupCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
	if (VM.IsValid())
	{
		FName NewGroup(*NewText.ToString());
		if (NewGroup != VM->Group) { VM->Group = NewGroup; VM->bDirty = true; }
	}
}

void SMaterialParameterRow::OnPriorityCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
	if (VM.IsValid() && NewValue != VM->SortPriority)
	{
		VM->SortPriority = NewValue;
		VM->bDirty = true;
	}
}

FText SMaterialParameterRow::MakeDiagnosticTooltip() const
{
	if (!VM.IsValid()) return FText::GetEmpty();

	FString Tip = FString::Printf(TEXT("Name: %s"), *VM->Name.ToString());
	Tip += FString::Printf(TEXT("\nType: %d"), static_cast<int32>(VM->Type));
	Tip += FString::Printf(TEXT("\nGroup: %s"), *VM->Group.ToString());
	Tip += FString::Printf(TEXT("\nSort Priority: %d"), VM->SortPriority);
	Tip += FString::Printf(TEXT("\nUsage: %s"), *VM->GetUsageLabel().ToString());

	if (VM->Usage == EMLPParameterUsage::Unused)
		Tip += TEXT("\n\nWarning: this parameter is not connected to any output.");
	if (VM->Usage == EMLPParameterUsage::HalfUsed)
		Tip += TEXT("\n\nNote: half-used — connected via MaterialAttributes but not a direct output.");
	if (VM->bHasDuplicateName)
		Tip += TEXT("\n\nConflict: another parameter shares this name.");

	return FText::FromString(Tip);
}

#undef LOCTEXT_NAMESPACE
