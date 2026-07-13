#include "Widgets/SMaterialCSVImportDialog.h"
#include "MaterialLayoutProTheme.h"
#include "Styling/CoreStyle.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"
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
	int32 NameIdx = 0, GroupIdx = 2, PriorityIdx = 3;
	for (int32 i = 0; i < Header.Num(); ++i)
	{
		const FString& Col = Header[i];
		if (Col == TEXT("Name")) NameIdx = i;
		else if (Col == TEXT("Group")) GroupIdx = i;
		else if (Col == TEXT("SortPriority")) PriorityIdx = i;
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

		Row.Name.TrimStartAndEndInline();
		if (Row.Name.IsEmpty()) continue;

		if (UMaterialExpressionParameter* const* Found = NameToExpr.Find(FName(*Row.Name)))
		{
			Row.bMatched = true;
			Row.OldGroup = (*Found)->Group.ToString();
			Row.OldSortPriority = (*Found)->SortPriority;
			Row.bWillChange = (Row.OldGroup != Row.NewGroup) || (Row.OldSortPriority != Row.NewSortPriority);
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
			Desc = FString::Printf(TEXT("%s   分组: %s → %s   优先级: %d → %d"),
				*R.Name, *R.OldGroup, *R.NewGroup, R.OldSortPriority, R.NewSortPriority);
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
						.ToolTipText(LOCTEXT("ApplyTT", "把匹配行的 Group/SortPriority 写回材质表达式"))
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
