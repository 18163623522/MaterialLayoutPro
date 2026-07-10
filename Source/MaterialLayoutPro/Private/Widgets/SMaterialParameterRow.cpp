#include "Widgets/SMaterialParameterRow.h"
#include "MaterialLayoutProTheme.h"
#include "Model/MaterialParameterDragDrop.h"

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
	OnParamDroppedDelegate = InArgs._OnParamDropped;

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
		if (bIsDropTarget) return FMLPTheme::AccentBg();     // drop highlight
		if (bSelected) return FMLPTheme::SelectionBg();
		return FLinearColor::Transparent;
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		// Drop indicator line (top) - shown when bDropBefore
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderBackgroundColor_Lambda([this]() -> FLinearColor {
				return (bIsDropTarget && bDropBefore) ? FMLPTheme::Accent() : FLinearColor::Transparent;
			})
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(0.f)
			[
				SNew(SBox).HeightOverride(2.f)
			]
		]

		// Main row content
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderBackgroundColor_Lambda(GetBgColor)
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMargin(6.f, 2.f, 4.f, 2.f))
			[
				SNew(SHorizontalBox)

				// Drag handle
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					BuildDragHandle()
				]

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

				// Name (editable)
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

				// Group (editable, detail mode)
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

				// Usage status label
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText {
						if (!VM.IsValid()) return FText::GetEmpty();
						switch (VM->Usage)
						{
						case EMLPParameterUsage::Used:     return FText::FromString(TEXT("已用"));
						case EMLPParameterUsage::Unused:   return FText::FromString(TEXT("未用"));
						case EMLPParameterUsage::HalfUsed: return FText::FromString(TEXT("部分"));
						case EMLPParameterUsage::Indirect: return FText::FromString(TEXT("间接"));
						default:                           return FText::FromString(TEXT("?"));
						}
					})
					.Font(FMLPTheme::FontSmall())
					.ColorAndOpacity_Lambda([this]() -> FSlateColor {
						if (!VM.IsValid()) return FMLPTheme::Muted();
						return VM->GetUsageColor();
					})
				]
			]
		]

		// Drop indicator line (bottom) - shown when !bDropBefore
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBorder)
			.BorderBackgroundColor_Lambda([this]() -> FLinearColor {
				return (bIsDropTarget && !bDropBefore) ? FMLPTheme::Accent() : FLinearColor::Transparent;
			})
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(0.f)
			[
				SNew(SBox).HeightOverride(2.f)
			]
		]
	];
}

FReply SMaterialParameterRow::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bCtrl = MouseEvent.IsControlDown();
		const bool bShift = MouseEvent.IsShiftDown();

		if (bCtrl || bShift)
		{
			// Multi-select: intercept and consume so children don't react.
			OnClickedDelegate.ExecuteIfBound(VM, bCtrl, bShift);
			return FReply::Handled();
		}
		// Plain click: fire selection but DON'T consume - let children (text boxes,
		// value editors) also receive the click for editing. This way both selection
		// and editing work on a single click.
		OnClickedDelegate.ExecuteIfBound(VM, false, false);
	}
	return SCompoundWidget::OnPreviewMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SMaterialParameterRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Plain left-click on empty row area (child widgets already consumed clicks on themselves).
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnClickedDelegate.ExecuteIfBound(VM, false, false);
	}
	return FReply::Unhandled();
}

// ============================================================================
// Drag handle
// ============================================================================

TSharedRef<SWidget> SMaterialParameterRow::BuildDragHandle()
{
	// The grip is a small SBorder that captures mouse-down to start a drag.
	// We don't start the drag here directly - instead we call DetectDrag() which
	// triggers OnDragDetected after the user moves the mouse enough.
	return SNew(SBorder)
		.BorderBackgroundColor(FLinearColor::Transparent)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.Padding(0.f)
		.Cursor(EMouseCursor::GrabHand)
		[
			SNew(SBox)
			.WidthOverride(12.f)
			.HeightOverride(16.f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\xE2\xA1\xAE"))) // ≡
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity_Lambda([this]() -> FSlateColor {
					return bSelected ? FMLPTheme::Accent() : FMLPTheme::Muted();
				})
			]
		]
		// Detect drag when user presses left mouse button on the grip.
		.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply {
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && VM.IsValid())
			{
				return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton);
			}
			return FReply::Unhandled();
		});
}

// ============================================================================
// Drag-drop overrides
// ============================================================================

FReply SMaterialParameterRow::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && VM.IsValid())
	{
		// Build the drag payload - just this row's VM for now.
		// (Multi-select drag could be added later by checking SelectedParams on the panel.)
		TArray<TSharedPtr<FMLPParamVM>> DraggedParams;
		DraggedParams.Add(VM);

		TSharedPtr<FMLPParameterDragDrop> DragOp = MakeShared<FMLPParameterDragDrop>(MoveTemp(DraggedParams));
		return FReply::Handled().BeginDragDrop(DragOp.ToSharedRef());
	}
	return FReply::Unhandled();
}

void SMaterialParameterRow::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FMLPParameterDragDrop> DragOp = DragDropEvent.GetOperationAs<FMLPParameterDragDrop>();
	if (DragOp.IsValid() && DragOp->IsValid() && VM.IsValid())
	{
		// Don't allow dropping onto yourself.
		TSharedPtr<FMLPParamVM> FirstDragged = DragOp->GetFirstParam();
		if (FirstDragged == VM) return;

		bIsDropTarget = true;
		// Top half = insert before, bottom half = insert after.
		const FVector2D LocalMouse = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
		bDropBefore = (LocalMouse.Y < MyGeometry.GetLocalSize().Y * 0.5f);
	}
}

void SMaterialParameterRow::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bIsDropTarget = false;
}

FReply SMaterialParameterRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FMLPParameterDragDrop> DragOp = DragDropEvent.GetOperationAs<FMLPParameterDragDrop>();
	if (DragOp.IsValid() && DragOp->IsValid() && VM.IsValid())
	{
		TSharedPtr<FMLPParamVM> FirstDragged = DragOp->GetFirstParam();
		if (FirstDragged == VM)
		{
			bIsDropTarget = false;
			return FReply::Unhandled();
		}

		// Determine insert position from mouse Y.
		const FVector2D LocalMouse = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
		const bool bInsertBefore = (LocalMouse.Y < MyGeometry.GetLocalSize().Y * 0.5f);

		// Notify the panel - it will handle the actual array reorder + SortPriority reassignment.
		OnParamDroppedDelegate.ExecuteIfBound(FirstDragged, VM, bInsertBefore);

		bIsDropTarget = false;
		return FReply::Handled();
	}
	bIsDropTarget = false;
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
		// Color swatch + RGBA text so the value is visible at a glance.
		auto GetRGBAText = [WeakVM]() -> FText {
			auto V = WeakVM.Pin();
			if (!V.IsValid()) return FText::GetEmpty();
			const FLinearColor& C = V->VectorValue;
			return FText::FromString(FString::Printf(TEXT("R:%.2f G:%.2f B:%.2f A:%.2f"), C.R, C.G, C.B, C.A));
		};
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
			[
				// SImage renders color directly via ColorAndOpacity — no border/background interference.
				SNew(SBox).WidthOverride(28.f).HeightOverride(16.f)
				[
					SNew(SImage)
					.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.ColorAndOpacity_Lambda([WeakVM]() -> FLinearColor {
						if (auto V = WeakVM.Pin())
						{
							// Force opaque — 4.26 VectorParameter DefaultValue often has A=0,
							// which makes the swatch invisible (transparent).
							FLinearColor C = V->VectorValue;
							if (C.A <= 0.001f) C.A = 1.0f;
							return C;
						}
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
					})
				]
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda(GetRGBAText)
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			];
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
		// Checkbox + true/false text so the value is visible.
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([WeakVM]() -> ECheckBoxState {
					if (auto V = WeakVM.Pin()) return V->BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					return ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { OnBoolChanged(State == ECheckBoxState::Checked); })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakVM]() -> FText {
					auto V = WeakVM.Pin();
					return (V.IsValid() && V->BoolValue) ? FText::FromString(TEXT("True")) : FText::FromString(TEXT("False"));
				})
				.Font(FMLPTheme::FontSmall())
				.ColorAndOpacity(FMLPTheme::Muted())
			];
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
