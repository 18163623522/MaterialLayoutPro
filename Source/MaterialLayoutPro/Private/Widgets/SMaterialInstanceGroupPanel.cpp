#include "Widgets/SMaterialInstanceGroupPanel.h"
#include "MaterialLayoutProTheme.h"
#include "Model/MaterialParameterScanner.h"
#include "Model/MaterialParameterInfo.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "StaticParameterSet.h"
#include "Engine/Texture.h"

#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Styling/CoreStyle.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_STYLE FAppStyle
#else
#include "EditorStyleSet.h"
#define MLP_STYLE FEditorStyle
#endif

#define LOCTEXT_NAMESPACE "SMaterialInstanceGroupPanel"

void SMaterialInstanceGroupPanel::Construct(const FArguments& InArgs)
{
	TargetInstance = InArgs._TargetInstance;
	if (TargetInstance.IsValid())
	{
		TargetMaterial = TargetInstance->GetBaseMaterial();
	}

	ChildSlot
	[
		BuildInitialContent()
	];
}

// ============================================================================
// Data load
// ============================================================================

void SMaterialInstanceGroupPanel::PullFromInstance()
{
	InstanceParams.Reset();
	InstanceTabNames.Reset();

	UMaterialInstance* MI = TargetInstance.Get();
	UMaterial* BaseMat = TargetMaterial.Get();
	if (!MI || !BaseMat) return;

	// Scan parent material for parameter list + groups.
	auto ScannedParams = FMaterialParameterScanner::ScanMaterial(BaseMat);

	for (const auto& P : ScannedParams)
	{
		if (!P.IsValid() || !P->Expression.IsValid()) continue;

		TSharedPtr<FMLPInstanceParamVM> VM = MakeShared<FMLPInstanceParamVM>();
		VM->Name = P->Name;
		VM->Group = P->Group.IsNone() ? FName(TEXT("(None)")) : P->Group;
		VM->ExpressionGUID = P->Guid;
		VM->Type = (int32)P->Type;

		// Check if this parameter has an override on the instance.
		FHashedMaterialParameterInfo ParamInfo(VM->Name);
		if (P->Type == EMLPParameterType::Scalar)
		{
			float OutVal;
			VM->bOverridden = MI->GetScalarParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->ScalarValue = OutVal;
			else
			{
				if (auto* ScalarExpr = Cast<UMaterialExpressionScalarParameter>(P->Expression.Get()))
					VM->ScalarValue = ScalarExpr->DefaultValue;
			}
		}
		else if (P->Type == EMLPParameterType::Vector)
		{
			FLinearColor OutVal;
			VM->bOverridden = MI->GetVectorParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->VectorValue = OutVal;
			else
			{
				if (auto* VecExpr = Cast<UMaterialExpressionVectorParameter>(P->Expression.Get()))
					VM->VectorValue = VecExpr->DefaultValue;
			}
		}
		else if (P->Type == EMLPParameterType::Texture)
		{
			UTexture* OutVal;
			VM->bOverridden = MI->GetTextureParameterValue(ParamInfo, OutVal, true);
			if (VM->bOverridden) VM->TextureValue = OutVal;
			else
			{
				if (auto* TexExpr = Cast<UMaterialExpressionTextureSampleParameter>(P->Expression.Get()))
					VM->TextureValue = TexExpr->Texture;
				else if (auto* TexObjExpr = Cast<UMaterialExpressionTextureObjectParameter>(P->Expression.Get()))
					VM->TextureValue = TexObjExpr->Texture;
			}
		}
		else if (P->Type == EMLPParameterType::StaticBool || P->Type == EMLPParameterType::StaticSwitch)
		{
			bool bOutVal;
			FGuid OutGuid;
			VM->bOverridden = MI->GetStaticSwitchParameterValue(ParamInfo, bOutVal, OutGuid, true);
			if (VM->bOverridden) VM->BoolValue = bOutVal;
			else
			{
				if (auto* BoolExpr = Cast<UMaterialExpressionStaticBoolParameter>(P->Expression.Get()))
					VM->BoolValue = BoolExpr->DefaultValue;
			}
		}

		InstanceParams.Add(VM);

		// Collect unique group names.
		if (!InstanceTabNames.Contains(VM->Group))
			InstanceTabNames.Add(VM->Group);
	}

	// Sort tab names alphabetically (None group last).
	InstanceTabNames.Sort([](const FName& A, const FName& B)
	{
		if (A == TEXT("(None)")) return false;
		if (B == TEXT("(None)")) return true;
		return A.ToString() < B.ToString();
	});

	// Default to first tab.
	if (!InstanceTabNames.Contains(CurrentTab))
	{
		CurrentTab = InstanceTabNames.Num() > 0 ? InstanceTabNames[0] : NAME_None;
	}
}

// ============================================================================
// UI build / refresh
// ============================================================================

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildInitialContent()
{
	// If the instance is invalid, show an error message instead of the panel.
	if (!TargetInstance.IsValid())
	{
		return SNew(SBorder)
			.Padding(FMargin(16))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoInstance", "未找到材质实例"))
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity(FMLPTheme::Muted())
			];
	}

	PullFromInstance();

	// Top-level container. RebuildInstanceContent() clears + refills THIS box on every
	// state change (tab switch / override toggle / value edit).
	TSharedPtr<SVerticalBox> Root = SNew(SVerticalBox);
	ContentContainer = Root;

	// Title row with the instance name.
	Root->AddSlot().AutoHeight().Padding(FMargin(2, 2, 2, 2))
	[
		SNew(STextBlock)
		.Text_Lambda([this]() -> FText {
			UMaterialInstance* MI = TargetInstance.Get();
			return MI ? FText::FromString(FString::Printf(TEXT("实例: %s"), *MI->GetName())) : FText::GetEmpty();
		})
		.Font(FMLPTheme::FontHeading())
		.ColorAndOpacity(FMLPTheme::Foreground())
	];

	RebuildInstanceContent();

	return Root.ToSharedRef();
}

void SMaterialInstanceGroupPanel::RebuildInstanceContent()
{
	if (!ContentContainer.IsValid()) return;

	// Remove everything after the title row (slot index 0). The title row is added once in
	// BuildInitialContent and must persist across rebuilds.
	while (ContentContainer->NumSlots() > 1)
	{
		ContentContainer->RemoveSlot(ContentContainer->GetChildren()->GetChildAt(ContentContainer->NumSlots() - 1));
	}

	// --- Tab bar row ---
	ContentContainer->AddSlot().AutoHeight().Padding(FMargin(0, 2, 0, 4))
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(FMLPTheme::Border().R, FMLPTheme::Border().G, FMLPTheme::Border().B, 0.5f))
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.Padding(FMargin(2, 1))
		[
			BuildTabBar()
		]
	];

	// --- Parameter rows for the current tab (inside a scroll box) ---
	TSharedRef<SScrollBox> Scroller = SNew(SScrollBox);
	BuildRows(Scroller);

	ContentContainer->AddSlot().FillHeight(1.0f)
	[
		Scroller
	];
}

TSharedRef<SWidget> SMaterialInstanceGroupPanel::BuildTabBar()
{
	// The window holds a strong ref to this widget, so its lifetime == window lifetime.
	// Row lambdas can therefore safely capture a weak ptr (kept weak as defensive measure).
	TWeakPtr<SMaterialInstanceGroupPanel, ESPMode::NotThreadSafe> WeakSelf = StaticCastSharedRef<SMaterialInstanceGroupPanel>(AsShared());

	TSharedRef<SHorizontalBox> TabBar = SNew(SHorizontalBox);

	for (const FName& TabName : InstanceTabNames)
	{
		const bool bActive = (TabName == CurrentTab);
		TabBar->AddSlot().AutoWidth().Padding(FMargin(1, 0))
		[
			SNew(SButton)
			.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
			.ButtonColorAndOpacity(bActive ? FMLPTheme::AccentBg() : FLinearColor::Transparent)
			.ForegroundColor(bActive ? FMLPTheme::Accent() : FMLPTheme::Muted())
			.ContentPadding(FMargin(8, 2))
			.OnClicked_Lambda([WeakSelf, TabName]() -> FReply {
				if (auto Self = WeakSelf.Pin()) return Self->OnTabClicked(TabName);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(FText::FromName(TabName))
				.Font(FMLPTheme::FontBody())
			]
		];
	}

	// [+] button to add a new group.
	TabBar->AddSlot().AutoWidth().Padding(FMargin(2, 0))
	[
		SNew(SButton)
		.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
		.ContentPadding(FMargin(6, 2))
		.ToolTipText(LOCTEXT("AddTabTT", "新建分组"))
		.OnClicked_Lambda([WeakSelf]() -> FReply {
			if (auto Self = WeakSelf.Pin()) return Self->OnAddTabClicked();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("+")))
			.Font(FMLPTheme::FontBody())
		]
	];

	return TabBar;
}

void SMaterialInstanceGroupPanel::BuildRows(TSharedRef<SScrollBox> ContentBox)
{
	TWeakPtr<SMaterialInstanceGroupPanel, ESPMode::NotThreadSafe> WeakSelf = StaticCastSharedRef<SMaterialInstanceGroupPanel>(AsShared());

	int32 VisibleCount = 0;
	for (const auto& P : InstanceParams)
	{
		if (P->Group != CurrentTab) continue;
		++VisibleCount;

		TWeakPtr<FMLPInstanceParamVM> WeakVM = P;

		// --- Value editor built per type ---
		// Each editor writes back through OnInstanceScalarChanged / OnInstanceVectorChanged /
		// OnInstanceTextureChanged / OnInstanceBoolChanged, which update the instance's
		// override array AND call RebuildInstanceContent() so the row reflects the new value.
		TSharedRef<SWidget> ValueEditor = [WeakSelf, WeakVM]() -> TSharedRef<SWidget>
		{
			auto V = WeakVM.Pin();
			if (!V.IsValid()) return SNew(STextBlock).Text(FText::GetEmpty());

			const bool bEditable = V->bOverridden;

			switch (V->Type)
			{
			case (int32)EMLPParameterType::Scalar:
			{
				// SNumericEntryBox is read-only when not overridden (Value returns unset),
				// editable when overridden. Matches the UE details-panel scalar row.
				auto WeakV2 = WeakVM;
				return SNew(SNumericEntryBox<float>)
					.Value_Lambda([WeakV2]() -> TOptional<float> {
						auto V2 = WeakV2.Pin();
						if (!V2.IsValid() || !V2->bOverridden) return TOptional<float>();
						return TOptional<float>(V2->ScalarValue);
					})
					.Font(FMLPTheme::FontSmall())
					.AllowSpin(bEditable)
					.MinValue(TOptional<float>()).MaxValue(TOptional<float>())
					.MinSliderValue(TOptional<float>()).MaxSliderValue(TOptional<float>())
					.MinDesiredValueWidth(80.f)
					.IsEnabled(bEditable)
					// Live update during spinner drag — just updates the VM value without
					// rebuilding (rebuilding would steal focus from the spinner).
					.OnValueChanged_Lambda([WeakV2](float NewVal) {
						auto V2 = WeakV2.Pin();
						if (V2.IsValid()) V2->ScalarValue = NewVal;
					})
					// Commit (Enter / focus loss) — write through to the instance + rebuild.
					.OnValueCommitted_Lambda([WeakSelf, WeakV2](float NewVal, ETextCommit::Type) {
						auto Self = WeakSelf.Pin();
						auto V2 = WeakV2.Pin();
						if (Self.IsValid() && V2.IsValid())
						{
							Self->OnInstanceScalarChanged(V2, NewVal, ETextCommit::Default);
						}
					});
			}
			case (int32)EMLPParameterType::Vector:
			{
				auto WeakV2 = WeakVM;
				// Color swatch (click opens picker) + RGBA text. Disabled when not overridden.
				return SNew(SHorizontalBox)
					.IsEnabled(bEditable)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
					[
						SNew(SBox).WidthOverride(28.f).HeightOverride(16.f)
						[
							SNew(SBorder)
							.BorderBackgroundColor_Lambda([WeakV2]() -> FLinearColor {
								auto V2 = WeakV2.Pin();
								if (!V2.IsValid()) return FLinearColor::Transparent;
								FLinearColor C = V2->VectorValue;
								if (C.A <= 0.001f) C.A = 1.0f; // UE4.26 VectorParam DefaultValue often A=0
								return C;
							})
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.OnMouseButtonDown_Lambda([WeakSelf, WeakV2](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply {
								if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
								auto V2 = WeakV2.Pin();
								if (!V2.IsValid()) return FReply::Unhandled();
								FColorPickerArgs Args;
								Args.bUseAlpha = true;
							#if ENGINE_MAJOR_VERSION >= 5
								Args.InitialColor = V2->VectorValue;
							#else
								Args.InitialColorOverride = V2->VectorValue;
							#endif
								TWeakPtr<FMLPInstanceParamVM, ESPMode::NotThreadSafe> WeakParam = V2;
								Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([WeakSelf, WeakParam](FLinearColor NewColor) {
									auto Self = WeakSelf.Pin();
									auto Param = WeakParam.Pin();
									if (Self.IsValid() && Param.IsValid())
									{
										Self->OnInstanceVectorChanged(Param, NewColor);
									}
								});
								OpenColorPicker(Args);
								return FReply::Handled();
							})
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text_Lambda([WeakV2]() -> FText {
							auto V2 = WeakV2.Pin();
							if (!V2.IsValid()) return FText::GetEmpty();
							const FLinearColor& C = V2->VectorValue;
							return FText::FromString(FString::Printf(TEXT("R:%.2f G:%.2f B:%.2f A:%.2f"), C.R, C.G, C.B, C.A));
						})
						.Font(FMLPTheme::FontSmall())
						.ColorAndOpacity(FMLPTheme::Muted())
					];
			}
			case (int32)EMLPParameterType::Texture:
			{
				// Click button -> Content Browser asset picker. Disabled when not overridden.
				return SNew(SButton)
					.IsEnabled(bEditable)
					.Text_Lambda([WeakVM]() -> FText {
						auto V = WeakVM.Pin();
						if (V.IsValid() && V->TextureValue.IsValid())
							return FText::FromString(V->TextureValue->GetName());
						return FText::FromString(TEXT("(无)"));
					})
					.ButtonStyle(MLP_STYLE::Get(), "FlatButton")
					.ContentPadding(FMargin(2.f, 0.f))
					.HAlign(HAlign_Left)
					.OnClicked_Lambda([WeakSelf, WeakVM]() -> FReply {
						auto Self = WeakSelf.Pin();
						if (!Self.IsValid()) return FReply::Handled();
						FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						FAssetPickerConfig Config;
					#if ENGINE_MAJOR_VERSION >= 5
						Config.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Texture2D")));
					#else
						Config.Filter.ClassNames.Add(TEXT("Texture2D"));
					#endif
						auto WeakV2 = WeakVM;
						TWeakPtr<SMaterialInstanceGroupPanel, ESPMode::NotThreadSafe> WeakSelf2 = WeakSelf;
						Config.OnAssetSelected = FOnAssetSelected::CreateLambda([WeakSelf2, WeakV2](const FAssetData& AssetData) -> void {
							auto Self2 = WeakSelf2.Pin();
							auto V2 = WeakV2.Pin();
							if (!Self2.IsValid() || !V2.IsValid() || !AssetData.IsValid()) { FSlateApplication::Get().DismissAllMenus(); return; }
							UObject* Asset = AssetData.GetAsset();
							if (!Asset) { const FString Path = AssetData.PackageName.ToString() / AssetData.AssetName.ToString(); Asset = LoadObject<UObject>(nullptr, *Path); }
							if (Asset) Self2->OnInstanceTextureChanged(V2, Asset);
							FSlateApplication::Get().DismissAllMenus();
						});
						Config.bAllowNullSelection = false;
						Config.InitialAssetViewType = EAssetViewType::List;
						TSharedRef<SWidget> Picker = CB.Get().CreateAssetPicker(Config);
						FSlateApplication::Get().PushMenu(Self->AsShared(), FWidgetPath(), Picker, FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
						return FReply::Handled();
					});
			}
			case (int32)EMLPParameterType::StaticBool:
			case (int32)EMLPParameterType::StaticSwitch:
			{
				// Checkbox toggles bool value; disabled when not overridden.
				auto WeakV2 = WeakVM;
				return SNew(SCheckBox)
					.IsEnabled(bEditable)
					.IsChecked_Lambda([WeakV2]() -> ECheckBoxState {
						auto V2 = WeakV2.Pin();
						if (!V2.IsValid()) return ECheckBoxState::Unchecked;
						return V2->BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([WeakSelf, WeakV2](ECheckBoxState State) {
						auto Self = WeakSelf.Pin();
						auto V2 = WeakV2.Pin();
						if (Self.IsValid() && V2.IsValid())
						{
							Self->OnInstanceBoolChanged(V2, State == ECheckBoxState::Checked);
						}
					});
			}
			default:
				return SNew(STextBlock)
					.Text(FText::FromString(TEXT("(不支持)")))
					.Font(FMLPTheme::FontSmall())
					.ColorAndOpacity(FMLPTheme::Muted());
			}
		}();

		ContentBox->AddSlot().Padding(FMargin(2, 1))
		[
			SNew(SHorizontalBox)
			// Override checkbox
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([WeakVM]() -> ECheckBoxState {
					auto V = WeakVM.Pin();
					return (V.IsValid() && V->bOverridden) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([WeakSelf, WeakVM](ECheckBoxState State) {
					auto Self = WeakSelf.Pin();
					auto V = WeakVM.Pin();
					if (Self.IsValid() && V.IsValid()) Self->OnToggleOverride(V);
				})
				.ToolTipText(LOCTEXT("OverrideTT", "勾选=覆盖实例值"))
			]
			// Parameter name
			+ SHorizontalBox::Slot().FillWidth(0.32f).VAlign(VAlign_Center).Padding(FMargin(4, 0))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakVM]() -> FText {
					auto V = WeakVM.Pin();
					return V.IsValid() ? FText::FromName(V->Name) : FText::GetEmpty();
				})
				.Font(FMLPTheme::FontBody())
				.ColorAndOpacity_Lambda([WeakVM]() -> FSlateColor {
					auto V = WeakVM.Pin();
					if (!V.IsValid()) return FMLPTheme::Muted();
					return V->bOverridden ? FMLPTheme::Foreground() : FMLPTheme::Muted();
				})
			]
			// Value editor (type-matched)
			+ SHorizontalBox::Slot().FillWidth(0.58f).VAlign(VAlign_Center)
			[
				ValueEditor
			]
			// Override indicator dot
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4, 0))
			[
				SNew(STextBlock)
				.Text_Lambda([WeakVM]() -> FText {
					auto V = WeakVM.Pin();
					return (V.IsValid() && V->bOverridden) ? FText::FromString(TEXT("●")) : FText::GetEmpty();
				})
				.ColorAndOpacity(FMLPTheme::Accent())
				.Font(FMLPTheme::FontSmall())
			]
		];
	}

	if (VisibleCount == 0)
	{
		ContentBox->AddSlot().Padding(FMargin(4, 8))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoTabParams", "该分组没有参数"))
			.Font(FMLPTheme::FontBody())
			.ColorAndOpacity(FMLPTheme::Muted())
		];
	}
}

// ============================================================================
// Handlers
// ============================================================================

FReply SMaterialInstanceGroupPanel::OnTabClicked(FName GroupName)
{
	CurrentTab = GroupName;
	RebuildInstanceContent();
	return FReply::Handled();
}

FReply SMaterialInstanceGroupPanel::OnAddTabClicked()
{
	// Create a new group on the parent material for all currently un-grouped parameters.
	if (!TargetMaterial.IsValid()) return FReply::Handled();

	FString NewName = FString::Printf(TEXT("Group_%d"), InstanceTabNames.Num());

	auto* M = TargetMaterial.Get();
	const FScopedTransaction T(FText::FromString(TEXT("新建分组")));
	M->Modify();
#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* E : M->GetExpressions())
#else
	for (UMaterialExpression* E : M->Expressions)
#endif
	{
		if (auto* P = Cast<UMaterialExpressionParameter>(E))
		{
			if (P->Group.IsNone())
			{
				P->Modify();
				P->Group = FName(*NewName);
			}
		}
	}
	M->PostEditChange();
	M->MarkPackageDirty();
	PullFromInstance();
	CurrentTab = FName(*NewName);
	RebuildInstanceContent();
	return FReply::Handled();
}

void SMaterialInstanceGroupPanel::OnToggleOverride(TSharedPtr<FMLPInstanceParamVM> Param)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	UMaterialInstance* MI = TargetInstance.Get();
	const FScopedTransaction T(FText::FromString(TEXT("切换参数覆盖")));
	MI->Modify();

	if (Param->bOverridden)
	{
		// Remove override.
		switch (Param->Type)
		{
		case (int32)EMLPParameterType::Scalar:
			MI->ScalarParameterValues.RemoveAll([&](const FScalarParameterValue& V) { return V.ParameterInfo.Name == Param->Name; });
			break;
		case (int32)EMLPParameterType::Vector:
			MI->VectorParameterValues.RemoveAll([&](const FVectorParameterValue& V) { return V.ParameterInfo.Name == Param->Name; });
			break;
		case (int32)EMLPParameterType::Texture:
			MI->TextureParameterValues.RemoveAll([&](const FTextureParameterValue& V) { return V.ParameterInfo.Name == Param->Name; });
			break;
		case (int32)EMLPParameterType::StaticBool:
		case (int32)EMLPParameterType::StaticSwitch:
			SetStaticSwitchOverride(Param, false, Param->BoolValue);
			break;
		default: break;
		}
		Param->bOverridden = false;
	}
	else
	{
		// Add override with current value.
		switch (Param->Type)
		{
		case (int32)EMLPParameterType::Scalar:
			{
				FScalarParameterValue SV;
				SV.ParameterInfo = FMaterialParameterInfo(Param->Name);
				SV.ParameterValue = Param->ScalarValue;
				SV.ExpressionGUID = Param->ExpressionGUID;
				MI->ScalarParameterValues.Add(SV);
			}
			break;
		case (int32)EMLPParameterType::Vector:
			{
				FVectorParameterValue VV;
				VV.ParameterInfo = FMaterialParameterInfo(Param->Name);
				VV.ParameterValue = Param->VectorValue;
				VV.ExpressionGUID = Param->ExpressionGUID;
				MI->VectorParameterValues.Add(VV);
			}
			break;
		case (int32)EMLPParameterType::Texture:
			{
				FTextureParameterValue TV;
				TV.ParameterInfo = FMaterialParameterInfo(Param->Name);
				TV.ParameterValue = Param->TextureValue.Get();
				TV.ExpressionGUID = Param->ExpressionGUID;
				MI->TextureParameterValues.Add(TV);
			}
			break;
		case (int32)EMLPParameterType::StaticBool:
		case (int32)EMLPParameterType::StaticSwitch:
			SetStaticSwitchOverride(Param, true, Param->BoolValue);
			break;
		default: break;
		}
		Param->bOverridden = true;
	}

	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceScalarChanged(TSharedPtr<FMLPInstanceParamVM> Param, float NewValue, ETextCommit::Type)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->ScalarValue = NewValue;
	UMaterialInstance* MI = TargetInstance.Get();
	MI->Modify();
	// Update existing override or add new.
	bool bFound = false;
	for (auto& V : MI->ScalarParameterValues)
	{
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = NewValue; bFound = true; break; }
	}
	if (!bFound)
	{
		FScalarParameterValue SV;
		SV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		SV.ParameterValue = NewValue;
		SV.ExpressionGUID = Param->ExpressionGUID;
		MI->ScalarParameterValues.Add(SV);
		Param->bOverridden = true;
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceVectorChanged(TSharedPtr<FMLPInstanceParamVM> Param, FLinearColor NewColor)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->VectorValue = NewColor;
	UMaterialInstance* MI = TargetInstance.Get();
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->VectorParameterValues)
	{
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = NewColor; bFound = true; break; }
	}
	if (!bFound)
	{
		FVectorParameterValue VV;
		VV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		VV.ParameterValue = NewColor;
		VV.ExpressionGUID = Param->ExpressionGUID;
		MI->VectorParameterValues.Add(VV);
		Param->bOverridden = true;
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceTextureChanged(TSharedPtr<FMLPInstanceParamVM> Param, UObject* NewTexture)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->TextureValue = Cast<UTexture>(NewTexture);
	UMaterialInstance* MI = TargetInstance.Get();
	MI->Modify();
	bool bFound = false;
	for (auto& V : MI->TextureParameterValues)
	{
		if (V.ParameterInfo.Name == Param->Name) { V.ParameterValue = Cast<UTexture>(NewTexture); bFound = true; break; }
	}
	if (!bFound)
	{
		FTextureParameterValue TV;
		TV.ParameterInfo = FMaterialParameterInfo(Param->Name);
		TV.ParameterValue = Cast<UTexture>(NewTexture);
		TV.ExpressionGUID = Param->ExpressionGUID;
		MI->TextureParameterValues.Add(TV);
		Param->bOverridden = true;
	}
	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

void SMaterialInstanceGroupPanel::OnInstanceBoolChanged(TSharedPtr<FMLPInstanceParamVM> Param, bool bNewValue)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	Param->BoolValue = bNewValue;
	// Static switch parameters live in StaticParameters.StaticSwitchParameters, not the typed
	// value arrays. Update via UpdateStaticPermutation so the permutation recompiles.
	SetStaticSwitchOverride(Param, Param->bOverridden, bNewValue);
}

void SMaterialInstanceGroupPanel::SetStaticSwitchOverride(TSharedPtr<FMLPInstanceParamVM> Param, bool bOverride, bool bNewValue)
{
	if (!Param.IsValid() || !TargetInstance.IsValid()) return;
	UMaterialInstance* MI = TargetInstance.Get();

	const FScopedTransaction T(FText::FromString(TEXT("修改静态开关参数")));
	MI->Modify();

	// Get the current static parameter set (parent + overrides), mutate the matching entry,
	// and push it back with UpdateStaticPermutation. This is the same path the UE details
	// panel uses for static switch editing.
	FStaticParameterSet ParamSet;
	MI->GetStaticParameterValues(ParamSet);

	bool bChanged = false;
	for (FStaticSwitchParameter& SP : ParamSet.StaticSwitchParameters)
	{
		if (SP.ParameterInfo.Name == Param->Name)
		{
			SP.bOverride = bOverride;
			SP.Value = bNewValue;
			bChanged = true;
			break;
		}
	}
	if (!bChanged && bOverride)
	{
		// Parameter exists in parent but wasn't in the instance set yet — append.
		FStaticSwitchParameter SP(FMaterialParameterInfo(Param->Name), bNewValue, true, Param->ExpressionGUID);
		ParamSet.StaticSwitchParameters.Add(SP);
		bChanged = true;
	}

	if (bChanged)
	{
		// UpdateStaticPermutation recompiles the instance permutation from the modified set.
		MI->UpdateStaticPermutation(ParamSet);
		Param->bOverridden = bOverride;
	}

	MI->PostEditChange();
	MI->MarkPackageDirty();
	RebuildInstanceContent();
}

#undef LOCTEXT_NAMESPACE
