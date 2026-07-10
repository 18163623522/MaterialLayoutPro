#include "Widgets/SMaterialParameterRow.h"
#include "MaterialLayoutProTheme.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionParameter.h"
#include "ScopedTransaction.h"
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
	OnClickedDelegate = InArgs._OnClicked;

	if (!VM.IsValid())
	{
		ChildSlot [ SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效")) ];
		return;
	}

	// Type color — shown as a small dot to the left of the name.
	const FLinearColor& TypeColor = FMLPTheme::GetTypeColorForType(VM->Type);

	// Usage status color — small dot after priority (green=used, red=unused, amber=half).
	const FLinearColor UsageColor = VM->GetUsageColor();

	// Name color — unused params shown in red (original-style diagnostic).
	const FLinearColor NameColor = (VM->Usage == EMLPParameterUsage::Unused)
		? FMLPTheme::Destructive()
		: (VM->bHasDuplicateName ? FMLPTheme::Warning() : FMLPTheme::Foreground());

	// Editable-text-box style: nearly transparent normally, light gray on hover/focus
	// so users can tell these fields are editable (not plain labels).
	static FEditableTextBoxStyle EditableStyle;
	static bool bStyleInit = false;
	if (!bStyleInit)
	{
		EditableStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
		// Normal: very subtle tint so the field is barely visible but hints it's interactive.
		EditableStyle.BackgroundImageNormal.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.04f));
		// Hover: clearly visible light background — "you can click me".
		EditableStyle.BackgroundImageHovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f));
		// Focused: slightly stronger so the active field is obvious.
		EditableStyle.BackgroundImageFocused.TintColor = FSlateColor(FLinearColor(0.039f, 0.561f, 0.890f, 0.15f));
		EditableStyle.Padding = FMargin(2.f, 1.f);
		bStyleInit = true;
	}

	auto GetBgColor = [this]() -> FLinearColor
	{
		if (bSelected) return FMLPTheme::SelectionBg();
		return FLinearColor::Transparent;
	};

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor_Lambda(GetBgColor)
		.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
		.Padding(FMargin(6.f, 2.f, 4.f, 2.f))
		[
			SNew(SHorizontalBox)

			// Type color dot
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SBox).WidthOverride(8.f).HeightOverride(8.f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(TypeColor)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.Padding(0.f)
				]
			]

			// Name (editable) — hover/focus highlights to show it's editable.
			+ SHorizontalBox::Slot().FillWidth(0.30f).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				SNew(SEditableTextBox)
				.Style(&EditableStyle)
				.Text(FText::FromName(VM->Name))
				.Font(FMLPTheme::FontBody())
				.ForegroundColor(NameColor)
				.SelectAllTextWhenFocused(true)
				.OnTextCommitted(this, &SMaterialParameterRow::OnNameCommitted)
				.ToolTipText(FText::FromString(TEXT("点击编辑参数名")))
			]

			// Value editor
			+ SHorizontalBox::Slot().FillWidth(0.34f).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				BuildValueEditor()
			]

			// Group (editable, detail mode) — hover/focus highlights to show it's editable.
			+ SHorizontalBox::Slot().FillWidth(0.20f).VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				bDetailMode
					? StaticCastSharedRef<SWidget>(
						SNew(SEditableTextBox)
						.Style(&EditableStyle)
						.Text(FText::FromName(VM->Group))
						.Font(FMLPTheme::FontSmall())
						.ForegroundColor(FMLPTheme::Muted())
						.OnTextCommitted(this, &SMaterialParameterRow::OnGroupCommitted)
						.ToolTipText(FText::FromString(TEXT("点击编辑分组"))))
					: StaticCastSharedRef<SWidget>(SNew(SBox))
			]

			// Sort priority (detail mode only)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				bDetailMode
					? StaticCastSharedRef<SWidget>(
						SNew(SBox).WidthOverride(48.f)
						[
							SNew(SNumericEntryBox<int32>)
							.Value(TOptional<int32>(VM->SortPriority))
							.Font(FMLPTheme::FontSmall())
							.OnValueCommitted(this, &SMaterialParameterRow::OnPriorityCommitted)
						])
					: StaticCastSharedRef<SWidget>(SNew(SBox))
			]

			// Usage status dot — green=used, red=unused, amber=half-used.
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
			[
				SNew(SBox).WidthOverride(7.f).HeightOverride(7.f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(UsageColor)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.Padding(0.f)
				]
			]
		]
	];
}

FReply SMaterialParameterRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Left-click anywhere on the row selects this parameter.
	// Child widgets (editable text, value editors) capture their own clicks first,
	// so this only fires for clicks on empty row areas — no conflict.
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnClickedDelegate.ExecuteIfBound(VM);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

TSharedRef<SWidget> SMaterialParameterRow::BuildValueEditor()
{
	if (!VM.IsValid()) return SNew(STextBlock).Text(FText::GetEmpty());

	TWeakPtr<FMLPParamVM> WeakVM = VM;

	switch (VM->Type)
	{
	case EMLPParameterType::Scalar:
	{
		return SNew(SNumericEntryBox<float>)
			.Value_Lambda([WeakVM]() -> TOptional<float> {
				if (auto V = WeakVM.Pin()) return TOptional<float>(V->ScalarValue);
				return TOptional<float>();
			})
			.Font(FMLPTheme::FontBody())
			.AllowSpin(true)
			.MinValue(TOptional<float>()).MaxValue(TOptional<float>())
			.MinSliderValue(TOptional<float>()).MaxSliderValue(TOptional<float>())
			.OnValueChanged(this, &SMaterialParameterRow::OnScalarDragged)
			.OnValueCommitted(this, &SMaterialParameterRow::OnScalarCommitted);
	}
	case EMLPParameterType::Vector:
	{
		return SNew(SColorBlock)
			.Color_Lambda([WeakVM]() -> FLinearColor {
				if (auto V = WeakVM.Pin()) return V->VectorValue;
				return FLinearColor::White;
			})
			.OnMouseButtonDown_Lambda([this, WeakVM](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply {
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
				Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, V](FLinearColor NewColor) { OnVectorChanged(NewColor); });
				OpenColorPicker(Args);
				return FReply::Handled();
			});
	}
	case EMLPParameterType::Texture:
	{
		auto GetTexName = [WeakVM]() -> FText {
			if (auto V = WeakVM.Pin())
				if (UTexture* T = V->TextureValue.Get()) return FText::FromString(T->GetName());
			return FText::FromString(TEXT("（无）"));
		};
		return SNew(SButton)
			.Text_Lambda(GetTexName)
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ContentPadding(FMargin(2.f, 0.f))
			.HAlign(HAlign_Left)
			.OnClicked_Lambda([this]() -> FReply {
				if (!VM.IsValid()) return FReply::Handled();
				FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				FAssetPickerConfig Config;
#if ENGINE_MAJOR_VERSION >= 5
				Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
#else
				Config.Filter.ClassNames.Add(TEXT("Texture2D"));
#endif
				Config.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData) -> void {
					if (VM.IsValid() && AssetData.IsValid()) {
						UObject* Asset = AssetData.GetAsset();
						if (!Asset) { const FString Path = AssetData.PackageName.ToString() / AssetData.AssetName.ToString(); Asset = LoadObject<UObject>(nullptr, *Path); }
						if (Asset) OnTextureChanged(Asset);
					}
					FSlateApplication::Get().DismissAllMenus();
				});
				Config.bAllowNullSelection = false;
				Config.InitialAssetViewType = EAssetViewType::List;
				TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
				FSlateApplication::Get().PushMenu(AsShared(), FWidgetPath(), Picker, FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
				return FReply::Handled();
			});
	}
	case EMLPParameterType::StaticBool:
	case EMLPParameterType::StaticSwitch:
	{
		return SNew(SCheckBox)
			.IsChecked_Lambda([WeakVM]() -> ECheckBoxState {
				if (auto V = WeakVM.Pin()) return V->BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				return ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { OnBoolChanged(State == ECheckBoxState::Checked); });
	}
	default:
		return SNew(STextBlock).Text(FText::FromString(TEXT("(不支持)"))).Font(FMLPTheme::FontSmall()).ColorAndOpacity(FMLPTheme::Muted());
	}
}

void SMaterialParameterRow::OnScalarDragged(float NewValue)
{
	if (VM.IsValid()) { VM->ScalarValue = NewValue; VM->bDirty = true; }
}

void SMaterialParameterRow::OnScalarCommitted(float NewValue, ETextCommit::Type CommitType)
{
	if (VM.IsValid()) { VM->ScalarValue = NewValue; VM->bDirty = true; if (Session.IsValid()) Session->PushParamNow(VM); }
}

void SMaterialParameterRow::OnVectorChanged(FLinearColor NewColor)
{
	if (VM.IsValid()) { VM->VectorValue = NewColor; VM->bDirty = true; if (Session.IsValid()) Session->PushParamNow(VM); }
}

void SMaterialParameterRow::OnTextureChanged(UObject* NewTexture)
{
	if (VM.IsValid()) { VM->TextureValue = Cast<UTexture>(NewTexture); VM->bDirty = true; if (Session.IsValid()) Session->PushParamNow(VM); }
}

void SMaterialParameterRow::OnBoolChanged(bool bNewValue)
{
	if (VM.IsValid()) { VM->BoolValue = bNewValue; VM->bDirty = true; if (Session.IsValid()) Session->PushParamNow(VM); }
}

void SMaterialParameterRow::OnGroupCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
	if (VM.IsValid()) {
		FName NewGroup(*NewText.ToString());
		if (NewGroup != VM->Group) { VM->Group = NewGroup; VM->bDirty = true; if (Session.IsValid()) Session->PushParamNow(VM); }
	}
}

void SMaterialParameterRow::OnPriorityCommitted(int32 NewValue, ETextCommit::Type CommitType)
{
	if (VM.IsValid() && NewValue != VM->SortPriority) { VM->SortPriority = NewValue; VM->bDirty = true; if (Session.IsValid()) Session->PushParamNow(VM); }
}

void SMaterialParameterRow::OnNameCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter && CommitType != ETextCommit::OnUserMovedFocus) return;
	if (!VM.IsValid() || !VM->SourceExpression.IsValid() || !Session.IsValid()) return;
	const FString NewNameStr = NewText.ToString();
	if (NewNameStr.IsEmpty() || NewNameStr == VM->Name.ToString()) return;
	if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(VM->SourceExpression.Get())) {
		const FName NewName(*NewNameStr);
		if (ParamExpr->ParameterName != NewName) {
			const FScopedTransaction Transaction(FText::FromString(TEXT("重命名材质参数")));
			ParamExpr->Modify(); ParamExpr->ParameterName = NewName; VM->Name = NewName;
			if (UMaterial* Mat = Session->TargetMaterial.Get()) { Mat->PostEditChange(); Mat->MarkPackageDirty(); }
		}
	}
}

FText SMaterialParameterRow::MakeDiagnosticTooltip() const
{
	if (!VM.IsValid()) return FText::GetEmpty();
	FString Tip = FString::Printf(TEXT("Name: %s\nType: %d\nGroup: %s\nPriority: %d\nUsage: %s"),
		*VM->Name.ToString(), static_cast<int32>(VM->Type), *VM->Group.ToString(), VM->SortPriority, *VM->GetUsageLabel().ToString());
	if (VM->Usage == EMLPParameterUsage::Unused) Tip += TEXT("\n\n未连接到任何输出");
	if (VM->bHasDuplicateName) Tip += TEXT("\n\n存在同名参数冲突");
	return FText::FromString(Tip);
}

#undef LOCTEXT_NAMESPACE
