#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

class UMaterial;

/** Broadcast after the CSV import was applied and the material post-edited. */
DECLARE_DELEGATE(FOnCSVImportApplied);

/**
 * Modal dialog that previews a CSV parameter import BEFORE writing it back to the material.
 *
 * Parses the CSV (Name,Type,Group,SortPriority,Usage,Value) with proper quoted-field handling
 * (so group names containing commas survive), cross-references each row against the material's
 * parameter expressions, and shows two lists: rows that will apply (matched) and rows that will
 * be skipped (unknown param name). The user confirms before anything is written.
 *
 * Only Group and SortPriority are written back (matching OnExportClicked's output columns);
 * Value is intentionally not imported here.
 */
class MATERIALLAYOUTPRO_API SMaterialCSVImportDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMaterialCSVImportDialog) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UMaterial>, TargetMaterial)
		/** The raw CSV text to parse + preview (loaded by the caller from the chosen file). */
		SLATE_ARGUMENT(FString, CSVText)
		SLATE_EVENT(FOnCSVImportApplied, OnApplied)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** One parsed CSV row, resolved against the material. */
	struct FImportRow
	{
		FString Name;
		FString NewGroup;
		int32 NewSortPriority = 0;
		FString NewValue;       // raw CSV Value field (may be empty = don't change value)
		FString OldGroup;       // current value on the expression (empty if unknown param)
		int32 OldSortPriority = 0;
		bool bMatched = false;  // false = param name not found in material (will be skipped)
		bool bWillChange = false; // matched AND (group or priority differs)
		bool bWillChangeValue = false; // matched AND value parses AND differs from current
	};

	/** Robust CSV line splitter: honors double-quoted fields (commas inside quotes are literal). */
	static void ParseCSVLine(const FString& Line, TArray<FString>& OutFields);
	/** Apply a Value string to a typed parameter expression, mutating it. Returns true if changed.
	 *  Handles scalar ("%.4f"), vector ("R:.. G:.. B:.. A:.."), texture (full object path),
	 *  and static bool ("True"/"False"). Empty ValueString => no change (returns false). */
	static bool ApplyValueToExpression(UMaterialExpressionParameter* Expr, const FString& ValueString);
	/** Non-mutating check: would ApplyValueToExpression change this expression? Used by the
	 *  preview so it doesn't mutate the material before the user confirms. */
	static bool WouldValueChange(UMaterialExpressionParameter* Expr, const FString& ValueString);

	/** Parse CSVText + resolve each row against the material's parameters. */
	void BuildPreview();

	FReply OnApplyClicked();
	FReply OnCancelClicked();

	TWeakObjectPtr<UMaterial> TargetMaterial;
	FString CSVText;
	TArray<FImportRow> Rows;

	FOnCSVImportApplied OnApplied;
};
