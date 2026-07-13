#include "Widgets/SMaterialCSVImportDialog.h"
#include "MaterialLayoutProTheme.h"
#include "Styling/CoreStyle.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Engine/Texture.h"
#include "UObject/UObjectGlobals.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_STYLE FAppStyle
#else
#include "EditorStyleSet.h"
#define MLP_STYLE FEditorStyle
#endif

#define LOCTEXT_NAMESPACE "SMaterialCSVImportDialog"

void SMaterialCSVImportDialog::ParseCSVLine(const FString& Line, TArray<FString>& OutFields)
{
	// Minimal RFC-4180-style splitter: a field is either wrapped in double quotes (commas inside
	// are literal, "" is an escaped quote) or runs up to the next comma. Handles group names that
	// contain commas — the naive ParseIntoArray(..., ",") would corrupt those.
	OutFields.Reset();
	FString Current;
	bool bInQuotes = false;
	for (int32 i = 0; i < Line.Len(); ++i)
	{
		const TCHAR Ch = Line[i];
		if (bInQuotes)
		{
			if (Ch == TEXT('"'))
			{
				if (i + 1 < Line.Len() && Line[i + 1] == TEXT('"'))
				{
					// Escaped quote literal.
					Current.AppendChar(TEXT('"'));
					++i;
				}
				else
				{
					bInQuotes = false;
				}
			}
			else
			{
				Current.AppendChar(Ch);
			}
		}
		else
		{
			if (Ch == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Ch == TEXT(','))
			{
				OutFields.Add(Current);
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Ch);
			}
		}
	}
	OutFields.Add(Current);  // final field
}

bool SMaterialCSVImportDialog::ApplyValueToExpression(UMaterialExpressionParameter* Expr, const FString& ValueString)
{
	if (!Expr) return false;
	FString V = ValueString;
	V.TrimStartAndEndInline();
	if (V.IsEmpty()) return false;  // no value to apply

	// Scalar: a plain float ("%.4f" on export).
	if (auto* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr))
	{
		float F = 0.f;
		if (V.IsNumeric())
		{
			F = FCString::Atof(*V);
			if (F != Scalar->DefaultValue) { Scalar->DefaultValue = F; return true; }
		}
		return false;
	}

	// Vector: "R:1.00 G:0.50 B:0.25 A:1.00" (export format). Parse each channel tolerantly.
	if (auto* Vector = Cast<UMaterialExpressionVectorParameter>(Expr))
	{
		FLinearColor Parsed = Vector->DefaultValue;  // start from current so partial parse is safe
		auto Extract = [&V](const TCHAR Channel) -> TOptional<float>
		{
			// Find "<Channel>:" then read the following number.
			FString Token = FString::Printf(TEXT("%c:"), Channel);
			int32 At = V.Find(Token, ESearchCase::IgnoreCase);
			if (At == INDEX_NONE) return TOptional<float>();
			// Gather the numeric substring starting at the token (sign/digits/dot).
			FString Num;
			for (int32 i = At + Token.Len(); i < V.Len(); ++i)
			{
				const TCHAR C = V[i];
				if ((C >= '0' && C <= '9') || C == '.' || C == '-' || C == '+') Num.AppendChar(C);
				else break;
			}
			Num.TrimStartAndEndInline();
			if (Num.IsEmpty() || !Num.IsNumeric()) return TOptional<float>();
			return FCString::Atof(*Num);
		};
		bool bAny = false;
		if (auto R = Extract('R')) { Parsed.R = *R; bAny = true; }
		if (auto G = Extract('G')) { Parsed.G = *G; bAny = true; }
		if (auto B = Extract('B')) { Parsed.B = *B; bAny = true; }
		if (auto A = Extract('A')) { Parsed.A = *A; bAny = true; }
		// Fallback: if no channel tokens, try 4 plain numbers "1 0.5 0.25 1".
		if (!bAny)
		{
			TArray<FString> Parts;
			V.ParseIntoArray(Parts, TEXT(" "), true);
			if (Parts.Num() >= 3
				&& Parts[0].IsNumeric() && Parts[1].IsNumeric() && Parts[2].IsNumeric())
			{
				Parsed.R = FCString::Atof(*Parts[0]);
				Parsed.G = FCString::Atof(*Parts[1]);
				Parsed.B = FCString::Atof(*Parts[2]);
				Parsed.A = (Parts.Num() >= 4 && Parts[3].IsNumeric()) ? FCString::Atof(*Parts[3]) : 1.f;
				bAny = true;
			}
		}
		if (bAny && Parsed != Vector->DefaultValue) { Vector->DefaultValue = Parsed; return true; }
		return false;
	}

	// Texture: a name or object path. Try to load by full path, else by name match against the
	// existing texture's package (best-effort — a bare name may be ambiguous).
	if (auto* TexSample = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
	{
		if (V.Contains(TEXT(".")))
		{
			if (UTexture* T = LoadObject<UTexture>(nullptr, *V))
			{
				if (T != TexSample->Texture) { TexSample->Texture = T; return true; }
			}
		}
		return false;
	}
	if (auto* TexObj = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
	{
		if (V.Contains(TEXT(".")))
		{
			if (UTexture* T = LoadObject<UTexture>(nullptr, *V))
			{
				if (T != TexObj->Texture) { TexObj->Texture = T; return true; }
			}
		}
		return false;
	}

	// Static bool / switch: "True"/"False" (case-insensitive).
	if (auto* BoolParam = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
	{
		const bool bNew = (V == TEXT("True") || V == TEXT("1"));
		if (bNew != BoolParam->DefaultValue) { BoolParam->DefaultValue = bNew; return true; }
		return false;
	}
	if (auto* SwitchParam = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
	{
		const bool bNew = (V == TEXT("True") || V == TEXT("1"));
		if (bNew != SwitchParam->DefaultValue) { SwitchParam->DefaultValue = bNew; return true; }
		return false;
	}

	return false;
}

bool SMaterialCSVImportDialog::WouldValueChange(UMaterialExpressionParameter* Expr, const FString& ValueString)
{
	if (!Expr) return false;
	FString V = ValueString;
	V.TrimStartAndEndInline();
	if (V.IsEmpty()) return false;

	// Compare against the current value WITHOUT writing. Mirrors ApplyValueToExpression's
	// per-type parsing but only reads. Kept in sync with that function.
	if (auto* Scalar = Cast<UMaterialExpressionScalarParameter>(Expr))
	{
		if (!V.IsNumeric()) return false;
		return FCString::Atof(*V) != Scalar->DefaultValue;
	}
	if (auto* Vector = Cast<UMaterialExpressionVectorParameter>(Expr))
	{
		auto Extract = [&V](const TCHAR Channel) -> TOptional<float>
		{
			FString Token = FString::Printf(TEXT("%c:"), Channel);
			int32 At = V.Find(Token, ESearchCase::IgnoreCase);
			if (At == INDEX_NONE) return TOptional<float>();
			FString Num;
			for (int32 i = At + Token.Len(); i < V.Len(); ++i)
			{
				const TCHAR C = V[i];
				if ((C >= '0' && C <= '9') || C == '.' || C == '-' || C == '+') Num.AppendChar(C);
				else break;
			}
			Num.TrimStartAndEndInline();
			if (Num.IsEmpty() || !Num.IsNumeric()) return TOptional<float>();
			return FCString::Atof(*Num);
		};
		bool bAny = Extract('R').IsSet() || Extract('G').IsSet() || Extract('B').IsSet() || Extract('A').IsSet();
		if (!bAny)
		{
			TArray<FString> Parts;
			V.ParseIntoArray(Parts, TEXT(" "), true);
			bAny = Parts.Num() >= 3 && Parts[0].IsNumeric() && Parts[1].IsNumeric() && Parts[2].IsNumeric();
		}
		return bAny;  // any parseable channel counts as a (potentially) different value
	}
	if (Cast<UMaterialExpressionTextureSampleParameter>(Expr) || Cast<UMaterialExpressionTextureObjectParameter>(Expr))
	{
		return V.Contains(TEXT("."));  // only full object paths are loadable → potential change
	}
	if (Cast<UMaterialExpressionStaticBoolParameter>(Expr) || Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
	{
		return V == TEXT("True") || V == TEXT("False") || V == TEXT("0") || V == TEXT("1");
	}
	return false;
}

void SMaterialCSVImportDialog::BuildPreview()
{
	Rows.Reset();

	if (!TargetMaterial.IsValid()) return;

	// Map parameter name -> expression for resolution + current values.
	TMap<FName, UMaterialExpressionParameter*> NameToExpr;
#if ENGINE_MAJOR_VERSION >= 5
	for (UMaterialExpression* E : TargetMaterial->GetExpressions())
#else
	for (UMaterialExpression* E : TargetMaterial->Expressions)
#endif
	{
		if (auto* P = Cast<UMaterialExpressionParameter>(E))
		{
			NameToExpr.Add(P->ParameterName, P);
		}
	}

	TArray<FString> Lines;
	CSVText.ParseIntoArrayLines(Lines);
	if (Lines.Num() < 2) return;  // need at least a header + one row

	// Find column indices from the header row (tolerant of column reordering / extra columns).
	TArray<FString> Header;
	ParseCSVLine(Lines[0], Header);
	int32 NameIdx = 0, GroupIdx = 2, PriorityIdx = 3, ValueIdx = INDEX_NONE;
	for (int32 i = 0; i < Header.Num(); ++i)
	{
		const FString& Col = Header[i];
		if (Col == TEXT("Name")) NameIdx = i;
		else if (Col == TEXT("Group")) GroupIdx = i;
		else if (Col == TEXT("SortPriority")) PriorityIdx = i;
		else if (Col == TEXT("Value")) ValueIdx = i;
	}

	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		if (Lines[i].IsEmpty()) continue;
		TArray<FString> Fields;
		ParseCSVLine(Lines[i], Fields);

		FImportRow Row;
		if (Fields.IsValidIndex(NameIdx)) Row.Name = Fields[NameIdx];
		if (Fields.IsValidIndex(GroupIdx)) Row.NewGroup = Fields[GroupIdx];
		if (Fields.IsValidIndex(PriorityIdx)) Row.NewSortPriority = FCString::Atoi(*Fields[PriorityIdx]);
		if (ValueIdx != INDEX_NONE && Fields.IsValidIndex(ValueIdx)) Row.NewValue = Fields[ValueIdx];

		Row.Name.TrimStartAndEndInline();
		if (Row.Name.IsEmpty()) continue;

		if (UMaterialExpressionParameter* const* Found = NameToExpr.Find(FName(*Row.Name)))
		{
			Row.bMatched = true;
			Row.OldGroup = (*Found)->Group.ToString();
			Row.OldSortPriority = (*Found)->SortPriority;
			Row.bWillChange = (Row.OldGroup != Row.NewGroup) || (Row.OldSortPriority != Row.NewSortPriority);
			// Non-mutating check so the preview doesn't change the material before confirm.
			Row.bWillChangeValue = WouldValueChange(*Found, Row.NewValue);
			Row.bWillChange = Row.bWillChange || Row.bWillChangeValue;
		}
		Rows.Add(MoveTemp(Row));
	}
}

void SMaterialCSVImportDialog::Construct(const FArguments& InArgs)
{
	TargetMaterial = InArgs._TargetMaterial;
	CSVText = InArgs._CSVText;
	OnApplied = InArgs._OnApplied;

	BuildPreview();

	// Tally counts for the summary.
	int32 Matched = 0, WillChange = 0, Unknown = 0;
	for (const FImportRow& R : Rows)
	{
		if (!R.bMatched) ++Unknown;
		else { ++Matched; if (R.bWillChange) ++WillChange; }
	}

	// Build a scrollable preview of every row (color-coded: green=will-change, muted=no-change,
	// red=unknown). Plain SVerticalBox of STextBlocks keeps it dependency-light (no SListView row
	// boilerplate) and is fine for the typical few-dozen-row CSV.
	TSharedRef<SVerticalBox> RowsBox = SNew(SVerticalBox);
	const FLinearColor ChangeColor = FMLPTheme::StatusUsed();    // blue/green-ish
	const FLinearColor NoChangeColor = FMLPTheme::Muted();
	const FLinearColor UnknownColor = FMLPTheme::Destructive();

	for (const FImportRow& R : Rows)
	{
		FLinearColor Color;
		FString Desc;
		if (!R.bMatched)
		{
			Color = UnknownColor;
			Desc = FString::Printf(TEXT("%s  —  ⚠ 未知参数(跳过)"), *R.Name);
		}
		else if (R.bWillChange)
		{
			Color = ChangeColor;
			// Show group/priority diff + a marker if the value will also change.
			Desc = FString::Printf(TEXT("%s   分组: %s → %s   优先级: %d → %d%s"),
				*R.Name, *R.OldGroup, *R.NewGroup, R.OldSortPriority, R.NewSortPriority,
				R.bWillChangeValue ? TEXT("   值: 是") : TEXT(""));
		}
		else
		{
			Color = NoChangeColor;
			Desc = FString::Printf(TEXT("%s   (无变化)"), *R.Name);
		}

		RowsBox->AddSlot().AutoHeight().Padding(FMargin(0.f, 1.f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Desc))
			.Font(FMLPTheme::FontSmall())
			.ColorAndOpacity(Color)
		];
	}

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("Title", "导入 CSV — 预览"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(560.f, 460.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FMLPTheme::Background())
			.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
			.Padding(FMLPTheme::PadMD())
			[
				SNew(SVerticalBox)
				// Summary line.
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 0.f, 0.f, 6.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("匹配 %d 个参数(其中 %d 个将变化),%d 个未知将跳过。"),
						Matched, WillChange, Unknown)))
					.Font(FMLPTheme::FontBody())
					.ColorAndOpacity(FMLPTheme::Foreground())
				]
				// Preview rows (scrollable).
				+ SVerticalBox::Slot().FillHeight(1.f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FMLPTheme::Surface())
					.BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
					.Padding(FMLPTheme::PadSM())
					[
					SNew(SScrollBox)
					+ SScrollBox::Slot()[ RowsBox ]
					]
				]
				// Buttons.
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 8.f, 0.f, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Apply", "应用导入"))
						.ToolTipText(LOCTEXT("ApplyTT", "把匹配行的 Group/SortPriority/Value 写回材质表达式"))
						.HAlign(HAlign_Center)
						.IsEnabled(WillChange > 0)
						.OnClicked(this, &SMaterialCSVImportDialog::OnApplyClicked)
					]
					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Cancel", "取消"))
						.HAlign(HAlign_Center)
						.OnClicked(this, &SMaterialCSVImportDialog::OnCancelClicked)
					]
				]
			]
		]
	);
}

FReply SMaterialCSVImportDialog::OnApplyClicked()
{
	if (TargetMaterial.IsValid())
	{
		const FScopedTransaction T(LOCTEXT("ImportTx", "从 CSV 导入参数"));
		UMaterial* M = TargetMaterial.Get();
		M->SetFlags(RF_Transactional);
		M->Modify();

		int32 Applied = 0;
		// Re-resolve names (in case the material changed since BuildPreview, though unlikely here).
		TMap<FName, UMaterialExpressionParameter*> NameToExpr;
#if ENGINE_MAJOR_VERSION >= 5
		for (UMaterialExpression* E : M->GetExpressions())
#else
		for (UMaterialExpression* E : M->Expressions)
#endif
		{
			if (auto* P = Cast<UMaterialExpressionParameter>(E)) NameToExpr.Add(P->ParameterName, P);
		}

		for (const FImportRow& R : Rows)
		{
			if (!R.bMatched || !R.bWillChange) continue;
			if (UMaterialExpressionParameter* const* Found = NameToExpr.Find(FName(*R.Name)))
			{
				(*Found)->SetFlags(RF_Transactional);
				(*Found)->Modify();
				(*Found)->Group = FName(*R.NewGroup);
				(*Found)->SortPriority = R.NewSortPriority;
				// Apply the Value column too (scalar/vector/texture/bool). Returns true if it wrote.
				ApplyValueToExpression(*Found, R.NewValue);
				++Applied;
			}
		}

		M->PostEditChange();
		M->MarkPackageDirty();
		OnApplied.ExecuteIfBound();
	}

	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SMaterialCSVImportDialog::OnCancelClicked()
{
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
