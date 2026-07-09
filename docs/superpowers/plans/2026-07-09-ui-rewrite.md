# MaterialLayoutPro UI 重做实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 放弃 IDetailsView 方案，用纯手写 Slate + ViewModel 快照层重做全部 5 个窗口，统一数据流和视觉风格。

**Architecture:** 三层架构 —— 引擎对象（UMaterial）→ ViewModel 快照层（FMLPSession/FMLPGroupVM/FMLPParamVM）→ UI（手写 Slate 控件，TAttribute 绑定 VM）。VM 用交互锁解决"焦点丢失 vs 外部刷新"矛盾。

**Tech Stack:** UE 4.26 C++ / Slate（SNew/SCompoundWidget/SWindow）/ `#if ENGINE_MAJOR_VERSION >= 5` 兼容 5.x

**验证方式:** 本项目无单元测试基础设施（纯编辑器 Slate 插件，高度依赖 GEditor/FSlateApplication 运行时）。采用**编译通过 + 部署后编辑器内功能验收**作为验证标准。

**编译命令:**
```bash
F:\UE4\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development -Project="E:\Project\UE\WJGZ 4.26\WJGZ.uproject" -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```

**部署:** 编译产物部署到 `E:\Project\UE\WJGZ 4.26\Plugins\MaterialLayoutPro\`

---

## 关键参考（实现前必读）

### 现有 Pull/Push 逻辑参考（来自 MLPEditorData.cpp）

新 VM 的 Pull/Push 直接参考这段已验证逻辑，仅替换字段类型：

```cpp
// Pull 从 Expression 读值（参考 MLPEditorData.cpp:19-57）
void PullFromExpression() {
    auto* Expr = SourceExpression.Get();
    if (!Expr) return;
    Group = Expr->Group;
    SortPriority = Expr->SortPriority;
    if (auto* S = Cast<UMaterialExpressionScalarParameter>(Expr))      { ScalarValue = S->DefaultValue; }
    else if (auto* V = Cast<UMaterialExpressionVectorParameter>(Expr))  { VectorValue = V->DefaultValue; }
    else if (auto* T = Cast<UMaterialExpressionTextureSampleParameter>(Expr))  { TextureValue = T->Texture; }
    else if (auto* T = Cast<UMaterialExpressionTextureObjectParameter>(Expr))  { TextureValue = T->Texture; }
    else if (auto* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))     { BoolValue = B->DefaultValue; }
    else if (auto* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))  { BoolValue = Sw->DefaultValue; }
}

// Push 写回 Expression（参考 MLPEditorData.cpp:59-92）
void PushToExpression() {
    auto* Expr = SourceExpression.Get();
    if (!Expr) return;
    Expr->Modify();
    Expr->Group = Group;
    Expr->SortPriority = SortPriority;
    // ... 同上类型分支写回
}
```

### Scanner 接口（不动，直接复用）

```cpp
// FMaterialParameterScanner::ScanMaterial(UMaterial*) → TArray<TSharedPtr<FMLPParameterInfo>>
// 已按类型提取 Name/Type/Group/SortPriority/Guid/ValueString
```

### UE 版本兼容宏（已建立约定）

```cpp
#if ENGINE_MAJOR_VERSION >= 5
#include "Styling/AppStyle.h"
#define MLP_STYLE FAppStyle
#else
#include "EditorStyleSet.h"
#define MLP_STYLE FEditorStyle
#endif
```

---

## 文件结构

### 新增文件

| 文件 | 职责 | 大小估计 |
|---|---|---|
| `Public/Model/MaterialLayoutViewModel.h` | `FMLPParamVM` / `FMLPGroupVM` / `FMLPSession` 定义 | ~120 行 |
| `Private/Model/MaterialLayoutViewModel.cpp` | Pull/Push/PullAll/PushDirty 实现 | ~150 行 |

### 重写文件（内容全新，文件名不变）

| 文件 | 职责 |
|---|---|
| `Public/Widgets/SMaterialParameterRow.h` | 单参数行控件：类型 pill + 状态徽章 + 值编辑器（接 VM） |
| `Private/Widgets/SMaterialParameterRow.cpp` | 行控件实现 |
| `Public/Widgets/SMaterialLayoutProPanel.h` | 主面板：双栏 + 自适应 |
| `Private/Widgets/SMaterialLayoutProPanel.cpp` | 主面板实现 |
| `Public/Widgets/SMaterialParameterEditor.h` | 参数编辑器：虚拟标签页 |
| `Private/Widgets/SMaterialParameterEditor.cpp` | 参数编辑器实现 |
| `Public/Widgets/SMaterialSortWorkbench.h` | 排序工作台：拖拽树 |
| `Private/Widgets/SMaterialSortWorkbench.cpp` | 排序工作台实现 |
| `Public/Widgets/SMaterialBulkRenameDialog.h` | 批量重命名 |
| `Private/Widgets/SMaterialBulkRenameDialog.cpp` | 批量重命名实现 |
| `Public/Widgets/SMaterialLayoutPreviewDialog.h` | 预览对话框 |
| `Private/Widgets/SMaterialLayoutPreviewDialog.cpp` | 预览对话框实现 |

### 删除文件（被 VM 取代）

| 文件 | 原因 |
|---|---|
| `Public/MLPEditorData.h` | UMLPEditorData/UMLPEditorParameter 被 VM 取代 |
| `Private/MLPEditorData.cpp` | 同上 |

### 不动的文件

- `MaterialLayoutProTheme.h` — 样式令牌系统（已完善）
- `Model/MaterialParameterScanner.*` — 扫描器（职责单一）
- `Model/MaterialParameterUsageAnalyzer.*` — 使用分析器
- `Model/MaterialParameterInfo.*` — 参数信息结构（VM 的数据源）
- `MaterialLayoutProSettings.*` — 插件设置
- `Style/MaterialLayoutProStyle.*` — 样式注册
- `Commands/MaterialLayoutProCommands.*` — 命令注册

---

## 阶段总览

| 阶段 | 内容 | 提交点 |
|---|---|---|
| **1** | ViewModel 层 | ✅ 提交 1 |
| **2** | 主面板（双栏 + 自适应 + 行控件） | ✅ 提交 2 |
| **3** | 参数编辑器（虚拟标签页） | ✅ 提交 3 |
| **4** | 排序工作台（拖拽树） | ✅ 提交 4 |
| **5** | 批量重命名 + 预览对话框 | ✅ 提交 5 |
| **6** | 删除 MLPEditorData + 全量编译验收 | ✅ 提交 6 |

---

## 阶段 1：ViewModel 层

**目标:** 创建 VM 快照层，提供 Pull/Push/交互锁能力。此阶段不改任何 UI，只新增文件。

### Task 1.1: 创建 ViewModel 头文件

**Files:**
- Create: `Source/MaterialLayoutPro/Public/Model/MaterialLayoutViewModel.h`

- [ ] **Step 1: 创建头文件**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "Model/MaterialParameterInfo.h"
#include "Materials/MaterialExpression.h"

class UMaterial;
class UMaterialExpressionParameter;
class UMaterialExpressionScalarParameter;
class UMaterialExpressionVectorParameter;
class UMaterialExpressionTextureSampleParameter;
class UMaterialExpressionTextureObjectParameter;
class UMaterialExpressionStaticBoolParameter;
class UMaterialExpressionStaticSwitchParameter;
class UTexture;

/**
 * Editable snapshot of a single material parameter.
 *
 * UI controls bind their TAttribute to these fields. The control instance stays
 * stable across refreshes (no rebuild), which prevents focus loss.
 *
 * Design: plain struct + TSharedPtr (no UHT dependency, 4.26 friendly).
 * Pull reads from the source expression into the snapshot; Push writes back.
 */
struct FMLPParamVM
{
    /** Identity (copied from FMLPParameterInfo, does not change). */
    FName Name;
    EMLPParameterType Type = EMLPParameterType::Other;
    FGuid Guid;

    /** Source expression — the engine object this VM mirrors. */
    TWeakObjectPtr<UMaterialExpression> SourceExpression;

    // --- Value snapshot (editable, use the one matching Type) ---

    float       ScalarValue  = 0.f;
    FLinearColor VectorValue = FLinearColor::White;
    TSoftObjectPtr<UTexture> TextureValue;
    bool        BoolValue    = false;

    // --- Organization (editable) ---

    FName   Group;
    int32   SortPriority = 0;

    // --- Status (read-only display) ---

    EMLPParameterUsage Usage = EMLPParameterUsage::Unknown;
    bool bIsInstanceEditable = false;
    bool bHasDuplicateName = false;

    // --- Edit state ---

    /** True if this VM has uncommitted changes since the last Push. */
    bool bDirty = false;

    /** Read current values from the source expression into this snapshot. */
    void PullFromExpression();

    /** Write snapshot values back to the source expression (calls Expr->Modify()). */
    void PushToExpression();
};

/**
 * A group of parameter VMs. Maps to a material Group.
 */
struct FMLPGroupVM
{
    FName Name;
    int32 SortPriority = 0;
    bool bExpanded = true;
    TArray<TSharedPtr<FMLPParamVM>> Parameters;
};

/**
 * Manages a complete snapshot of one material's parameters + the interaction lock.
 *
 * The interaction lock prevents external refreshes from interrupting user input:
 *   - BeginInteract() when a control gains focus  → InteractingCount++
 *   - EndInteract()   when a control loses focus   → InteractingCount--
 *   - PullAll() only proceeds when InteractingCount == 0; otherwise defers.
 */
class FMLPSession
{
public:
    /** The material this session mirrors. */
    TWeakObjectPtr<UMaterial> TargetMaterial;

    /** All groups (each containing parameter VMs). */
    TArray<TSharedPtr<FMLPGroupVM>> Groups;

    // --- Interaction lock ---

    /** Number of controls currently being interacted with. >0 = block refresh. */
    int32 InteractingCount = 0;

    /** A refresh was requested while InteractingCount > 0; run it after count returns to 0. */
    bool bPendingRefresh = false;

    // --- Snapshot lifecycle ---

    /** Rebuild the entire VM tree from the material. No-op if InteractingCount > 0 (sets bPendingRefresh). */
    void PullAll();

    /** Write all bDirty parameters back in a single FScopedTransaction. Clears bDirty. */
    void PushDirty();

    /** Check if any parameter has uncommitted changes. */
    bool HasDirty() const;

    // --- Interaction lock API (called by UI controls) ---

    void BeginInteract();
    void EndInteract();
};
```

- [ ] **Step 2: 编译验证（应失败，因为 cpp 还没写）**

```bash
F:\UE4\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development -Project="E:\Project\UE\WJGZ 4.26\WJGZ.uproject" -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```
Expected: 链接错误（unresolved external symbol FMLPParamVM::PullFromExpression 等）—— 这是正常的，下个 Task 补实现。

---

### Task 1.2: 创建 ViewModel 实现文件

**Files:**
- Create: `Source/MaterialLayoutPro/Private/Model/MaterialLayoutViewModel.cpp`

- [ ] **Step 1: 创建实现文件**

```cpp
#include "Model/MaterialLayoutViewModel.h"
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
#include "Model/MaterialParameterScanner.h"
#include "Model/MaterialParameterUsageAnalyzer.h"
#include "ScopedTransaction.h"

// ============================================================================
// FMLPParamVM
// ============================================================================

void FMLPParamVM::PullFromExpression()
{
    UMaterialExpression* Expr = SourceExpression.Get();
    if (!Expr)
    {
        return;
    }

    // Group + SortPriority are on the base UMaterialExpressionParameter.
    if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
    {
        Group = Param->Group;
        SortPriority = Param->SortPriority;
    }

    // Value depends on the concrete expression type.
    if (UMaterialExpressionScalarParameter* S = Cast<UMaterialExpressionScalarParameter>(Expr))
    {
        ScalarValue = S->DefaultValue;
    }
    else if (UMaterialExpressionVectorParameter* V = Cast<UMaterialExpressionVectorParameter>(Expr))
    {
        VectorValue = V->DefaultValue;
    }
    else if (UMaterialExpressionTextureSampleParameter* T = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
    {
        TextureValue = T->Texture;
    }
    else if (UMaterialExpressionTextureObjectParameter* T = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
    {
        TextureValue = T->Texture;
    }
    else if (UMaterialExpressionStaticBoolParameter* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
    {
        BoolValue = B->DefaultValue;
    }
    else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
    {
        BoolValue = Sw->DefaultValue;
    }
}

void FMLPParamVM::PushToExpression()
{
    UMaterialExpression* Expr = SourceExpression.Get();
    if (!Expr)
    {
        return;
    }

    if (UMaterialExpressionParameter* Param = Cast<UMaterialExpressionParameter>(Expr))
    {
        Param->Modify();
        Param->Group = Group;
        Param->SortPriority = SortPriority;
    }

    if (UMaterialExpressionScalarParameter* S = Cast<UMaterialExpressionScalarParameter>(Expr))
    {
        S->DefaultValue = ScalarValue;
    }
    else if (UMaterialExpressionVectorParameter* V = Cast<UMaterialExpressionVectorParameter>(Expr))
    {
        V->DefaultValue = VectorValue;
    }
    else if (UMaterialExpressionTextureSampleParameter* T = Cast<UMaterialExpressionTextureSampleParameter>(Expr))
    {
        T->Texture = TextureValue.LoadSynchronous();
    }
    else if (UMaterialExpressionTextureObjectParameter* T = Cast<UMaterialExpressionTextureObjectParameter>(Expr))
    {
        T->Texture = TextureValue.LoadSynchronous();
    }
    else if (UMaterialExpressionStaticBoolParameter* B = Cast<UMaterialExpressionStaticBoolParameter>(Expr))
    {
        B->DefaultValue = BoolValue;
    }
    else if (UMaterialExpressionStaticSwitchParameter* Sw = Cast<UMaterialExpressionStaticSwitchParameter>(Expr))
    {
        Sw->DefaultValue = BoolValue;
    }

    bDirty = false;
}

// ============================================================================
// FMLPSession
// ============================================================================

void FMLPSession::PullAll()
{
    // Interaction lock: if a control is being edited, defer the refresh.
    if (InteractingCount > 0)
    {
        bPendingRefresh = true;
        return;
    }

    Groups.Reset();

    UMaterial* Mat = TargetMaterial.Get();
    if (!Mat)
    {
        return;
    }

    // Scan the material (reuse existing scanner — it's stable and tested).
    TArray<TSharedPtr<FMLPParameterInfo>> Params = FMaterialParameterScanner::ScanMaterial(Mat);
    FMaterialParameterUsageAnalyzer::Analyze(Mat, Params);

    // Group parameters by their Group name.
    TMap<FName, int32> GroupIndexMap;

    for (const TSharedPtr<FMLPParameterInfo>& Info : Params)
    {
        if (!Info.IsValid() || !Info->Expression.IsValid())
        {
            continue;
        }

        FName GroupName = Info->Group.IsNone() ? FName(TEXT("(None)")) : Info->Group;

        // Find or create the group.
        int32* FoundIndex = GroupIndexMap.Find(GroupName);
        if (!FoundIndex)
        {
            TSharedPtr<FMLPGroupVM> NewGroup = MakeShared<FMLPGroupVM>();
            NewGroup->Name = GroupName;
            Groups.Add(NewGroup);
            FoundIndex = &GroupIndexMap.Add(GroupName, Groups.Num() - 1);
        }

        // Create the parameter VM and pull its values.
        TSharedPtr<FMLPParamVM> ParamVM = MakeShared<FMLPParamVM>();
        ParamVM->Name = Info->Name;
        ParamVM->Type = Info->Type;
        ParamVM->Guid = Info->Guid;
        ParamVM->Usage = Info->Usage;
        ParamVM->bIsInstanceEditable = Info->bIsInstanceEditable;
        ParamVM->bHasDuplicateName = Info->bHasDuplicateName;
        ParamVM->SourceExpression = Info->Expression;
        ParamVM->PullFromExpression();

        Groups[*FoundIndex]->Parameters.Add(ParamVM);
    }

    // Sort groups alphabetically (matches the old BuildFromMaterial behavior).
    Groups.Sort([](const TSharedPtr<FMLPGroupVM>& A, const TSharedPtr<FMLPGroupVM>& B)
    {
        return A->Name.ToString() < B->Name.ToString();
    });

    bPendingRefresh = false;
}

void FMLPSession::PushDirty()
{
    UMaterial* Mat = TargetMaterial.Get();
    if (!Mat)
    {
        return;
    }

    // Collect dirty parameters first.
    bool bAnyDirty = false;
    for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
        {
            if (Param.IsValid() && Param->bDirty)
            {
                bAnyDirty = true;
                break;
            }
        }
        if (bAnyDirty) break;
    }

    if (!bAnyDirty)
    {
        return;
    }

    // Single transaction for all changes.
    const FScopedTransaction Transaction(FText::FromString(TEXT("修改材质参数")));

    for (TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        for (TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
        {
            if (Param.IsValid() && Param->bDirty)
            {
                Param->PushToExpression();
            }
        }
    }

    Mat->PostEditChange();
    Mat->MarkPackageDirty();
}

bool FMLPSession::HasDirty() const
{
    for (const TSharedPtr<FMLPGroupVM>& Group : Groups)
    {
        for (const TSharedPtr<FMLPParamVM>& Param : Group->Parameters)
        {
            if (Param.IsValid() && Param->bDirty)
            {
                return true;
            }
        }
    }
    return false;
}

void FMLPSession::BeginInteract()
{
    ++InteractingCount;
}

void FMLPSession::EndInteract()
{
    if (InteractingCount > 0)
    {
        --InteractingCount;
    }

    // When all interactions end, process any deferred refresh.
    if (InteractingCount == 0 && bPendingRefresh)
    {
        PullAll();
    }
}
```

- [ ] **Step 2: 编译验证**

```bash
F:\UE4\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development -Project="E:\Project\UE\WJGZ 4.26\WJGZ.uproject" -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```
Expected: 编译成功（此时 VM 尚未被任何 UI 引用，但符号已全部定义）。

- [ ] **Step 3: 提交**

```bash
cd F:\Cache\AI\MaterialLayoutPro
git add Source/MaterialLayoutPro/Public/Model/MaterialLayoutViewModel.h Source/MaterialLayoutPro/Private/Model/MaterialLayoutViewModel.cpp
git commit -m "feat(vm): 添加 ViewModel 快照层（FMLPParamVM/FMLPGroupVM/FMLPSession）

- FMLPParamVM: 单参数快照 + Pull/Push
- FMLPGroupVM: 分组容器
- FMLPSession: 会话管理 + 交互锁（InteractingCount）
- 解决焦点丢失 vs 外部刷新矛盾"
```

---

## 阶段 2：主面板 + 行控件重写

**目标:** 重写 `SMaterialParameterRow`（接 VM）和 `SMaterialLayoutProPanel`（双栏 + 自适应），这是用户最先看到的核心界面。

**实现说明:** UI 层的 Slate 构建代码（SNew 嵌套）参考现有 `SMaterialParameterRow.cpp` 的模式，本计划聚焦于**新接口签名 + VM 绑定逻辑 + 关键差异点**，避免重复 400 行无增量信息的 SNew 代码。

### Task 2.1: 重写 SMaterialParameterRow 头文件

**Files:**
- Rewrite: `Source/MaterialLayoutPro/Public/Widgets/SMaterialParameterRow.h`

**核心变化:** 从绑定 `FMLPParameterInfo` 改为绑定 `FMLPParamVM` + `FMLPSession`。

- [ ] **Step 1: 重写头文件**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Model/MaterialLayoutViewModel.h"

class FMLPSession;
class SMaterialParameterRow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMaterialParameterRow) {}
        /** The parameter VM this row displays/edits. */
        SLATE_ARGUMENT(TSharedPtr<FMLPParamVM>, ParamVM)
        /** The session (for interaction lock). */
        SLATE_ARGUMENT(TSharedPtr<FMLPSession>, Session)
        /** Whether this row is selected. */
        SLATE_ARGUMENT(bool, bSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    TSharedPtr<FMLPParamVM> VM;
    TSharedPtr<FMLPSession> Session;

    /** Build the value editor widget based on VM->Type. */
    TSharedRef<SWidget> BuildValueEditor();

    // --- Value commit callbacks (write to VM, mark dirty) ---

    void OnScalarCommitted(float NewValue, ETextCommit::Type CommitType);
    void OnVectorChanged(FLinearColor NewColor);
    void OnTextureChanged(UObject* NewTexture);
    void OnBoolChanged(bool bNewValue);

    // --- Interaction lock hooks ---

    void OnEditFocusReceived();
    void OnEditFocusLost();
};
```

**与旧版的关键差异:**
1. `SLATE_ARGUMENT` 从 `TSharedPtr<FMLPParameterInfo>` 改为 `TSharedPtr<FMLPParamVM>`
2. 新增 `Session` 参数（交互锁）
3. 删除所有 delegate（`OnScalarChangedDelegate` 等）—— 值直接写 VM，由 Session.PushDirty 统一回写
4. 删除 `OnDragDetected/OnDrop`（拖拽移到主面板的 ListView 层处理）

---

### Task 2.2: 重写 SMaterialParameterRow 实现

**Files:**
- Rewrite: `Source/MaterialLayoutPro/Private/Widgets/SMaterialParameterRow.cpp`

- [ ] **Step 1: 重写实现 —— 构造 + 行布局**

构造函数骨架（Slate 布局参考旧版 `SMaterialParameterRow.cpp:121-195` 的 SHorizontalBox 结构，保持类型 pill + 名字 + 值 + 状态徽章布局不变，但数据源改为 VM）：

```cpp
#include "Widgets/SMaterialParameterRow.h"
#include "MaterialLayoutProTheme.h"
// ... includes 同旧版（SBorder/SBox/SBoxPanel/STextBlock/SNumericEntryBox/SCheckBox/SColorBlock 等）

#define LOCTEXT_NAMESPACE "SMaterialParameterRow"

void SMaterialParameterRow::Construct(const FArguments& InArgs)
{
    VM = InArgs._ParamVM;
    Session = InArgs._Session;

    if (!VM.IsValid())
    {
        ChildSlot [ SNew(STextBlock).Text(LOCTEXT("InvalidRow", "无效")) ];
        return;
    }

    // 获取类型颜色/缩写 —— FMLPParameterInfo 的静态辅助方法
    // 注意：这些方法在 FMLPParameterInfo 上，VM 没有。
    // 方案：用 EMLPParameterType 直接映射颜色（复用 FMLPTheme 的 Type* token）。
    const FLinearColor TypeColor = FMLPTheme::GetTypeColorForType(VM->Type);   // 新增辅助（见下方）
    const FText TypeAbbr = FMLPTheme::GetTypeAbbrForType(VM->Type);            // 新增辅助（见下方）

    // 背景色（选中态）
    auto GetBgColor = [this]() -> FLinearColor
    {
        return InArgs._bSelected
            ? FLinearColor(FMLPTheme::SelectionBg().R, FMLPTheme::SelectionBg().G, FMLPTheme::SelectionBg().B, 0.35f)
            : FLinearColor(FMLPTheme::Surface().R, FMLPTheme::Surface().G, FMLPTheme::Surface().B, 0.6f);
    };

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor_Lambda(GetBgColor)
        .BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
        .Padding(FMargin(8.f, 3.f, 6.f, 3.f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0, 0, 6, 0))
            [
                FMLPTheme::MakeTypePill(TypeAbbr, TypeColor)
            ]
            + SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).Padding(FMLPTheme::PadSM())
            [
                SNew(STextBlock)
                .Text(FText::FromName(VM->Name))
                .Font(FMLPTheme::FontBody())
                .ColorAndOpacity(VM->bHasDuplicateName ? FMLPTheme::Warning() : FMLPTheme::Foreground())
            ]
            + SHorizontalBox::Slot().FillWidth(0.35f).VAlign(VAlign_Center).Padding(FMLPTheme::PadSM())
            [
                BuildValueEditor()
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMLPTheme::PadSM())
            [
                // 状态徽章
                SNew(SBorder)
                .BorderBackgroundColor(VM->GetUsageBgColor())   // 新增 VM 辅助（见下方）
                .BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
                .Padding(FMargin(4.f, 1.f)).HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(VM->GetUsageLabel())                  // 新增 VM 辅助（见下方）
                    .Font(FMLPTheme::FontBadge())
                    .ColorAndOpacity(VM->GetUsageColor())       // 新增 VM 辅助（见下方）
                ]
            ]
        ]
    ];
}
```

**注意：** VM 需要几个显示辅助方法（`GetUsageLabel/GetUsageColor/GetUsageBgColor`）和 Theme 需要类型映射辅助。这些加在 Task 2.3。

- [ ] **Step 2: 重写实现 —— 值编辑器 BuildValueEditor()**

```cpp
TSharedRef<SWidget> SMaterialParameterRow::BuildValueEditor()
{
    if (!VM.IsValid()) return SNew(STextBlock).Text(FText::GetEmpty());

    switch (VM->Type)
    {
    case EMLPParameterType::Scalar:
    {
        // 绑定到 VM->ScalarValue（TAttribute 读取，提交时写回）
        TWeakPtr<FMLPParamVM> WeakVM = VM;
        TWeakPtr<FMLPSession> WeakSession = Session;
        return SNew(SNumericEntryBox<float>)
            .Value_Lambda([WeakVM]() -> TOptional<float> {
                if (auto V = WeakVM.Pin()) return TOptional<float>(V->ScalarValue);
                return TOptional<float>();
            })
            .Font(FMLPTheme::FontBody())
            .AllowSpin(true)
            .OnValueChanged_Lambda([WeakVM, WeakSession](float NewValue) {
                if (auto V = WeakVM.Pin()) { V->ScalarValue = NewValue; V->bDirty = true; }
            })
            .OnValueCommitted(this, &SMaterialParameterRow::OnScalarCommitted);
    }
    case EMLPParameterType::Vector:
    {
        TWeakPtr<FMLPParamVM> WeakVM = VM;
        TWeakPtr<FMLPSession> WeakSession = Session;
        return SNew(SColorBlock)
            .Color_Lambda([WeakVM]() -> FLinearColor {
                if (auto V = WeakVM.Pin()) return V->VectorValue;
                return FLinearColor::White;
            })
            .OnMouseButtonDown_Lambda([this, WeakVM, WeakSession](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply {
                if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
                if (!WeakVM.IsValid()) return FReply::Unhandled();
                auto V = WeakVM.Pin();
                FColorPickerArgs Args;
                Args.bUseAlpha = true;
#if ENGINE_MAJOR_VERSION >= 5
                Args.InitialColor = V->VectorValue;
#else
                Args.InitialColorOverride = V->VectorValue;
#endif
                Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, V](FLinearColor NewColor) {
                    OnVectorChanged(NewColor);
                });
                OpenColorPicker(Args);
                return FReply::Handled();
            });
    }
    case EMLPParameterType::Texture:
    {
        // 纹理选择按钮 —— 弹出 ContentBrowser 资源选择器
        // 提交时调用 OnTextureChanged
        // （Slate 结构参考旧版 SMaterialParameterRow.cpp:328-386，只把数据源改为 VM->TextureValue）
        TWeakPtr<FMLPParamVM> WeakVM = VM;
        auto GetTexName = [WeakVM]() -> FText {
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
            .OnClicked_Lambda([this]() -> FReply {
                // 弹出 ContentBrowser 选择器（同旧版逻辑）
                // 选择后调用 OnTextureChanged(SelectedAsset)
                // 完整实现参考旧版 SMaterialParameterRow.cpp:346-385
                // ... ContentBrowser 弹出代码 ...
                return FReply::Handled();
            });
    }
    case EMLPParameterType::StaticBool:
    case EMLPParameterType::StaticSwitch:
    {
        TWeakPtr<FMLPParamVM> WeakVM = VM;
        return SNew(SCheckBox)
            .IsChecked_Lambda([WeakVM]() -> ECheckBoxState {
                if (auto V = WeakVM.Pin()) return V->BoolValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                return ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this](ECheckBoxState State) {
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
```

**与旧版的关键差异:** 值控件的 `Value_Lambda` 从读 `Expression->DefaultValue` 改为读 `VM->ScalarValue`。`OnValueChanged` 直接写 `VM->ScalarValue` + `bDirty=true`，不再立即回写 Expression。

- [ ] **Step 3: 重写实现 —— 回调 + 交互锁**

```cpp
void SMaterialParameterRow::OnScalarCommitted(float NewValue, ETextCommit::Type CommitType)
{
    // 值已在 OnValueChanged 时写入 VM，这里只需标记 dirty + 触发推送
    if (VM.IsValid()) { VM->bDirty = true; }
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

void SMaterialParameterRow::OnEditFocusReceived()
{
    if (Session.IsValid()) Session->BeginInteract();
}

void SMaterialParameterRow::OnEditFocusLost()
{
    if (Session.IsValid()) Session->EndInteract();
}

#undef LOCTEXT_NAMESPACE
```

---

### Task 2.3: 添加 VM/Theme 显示辅助方法

**Files:**
- Modify: `Source/MaterialLayoutPro/Public/Model/MaterialLayoutViewModel.h`（给 FMLPParamVM 加显示辅助）
- Modify: `Source/MaterialLayoutPro/Public/MaterialLayoutProTheme.h`（给 FMLPTheme 加类型映射）

- [ ] **Step 1: 给 FMLPParamVM 加显示辅助方法**

在 `MaterialLayoutViewModel.h` 的 `FMLPParamVM` 末尾 `PushToExpression()` 声明后添加：

```cpp
    // --- Display helpers (for UI badges) ---

    /** Usage status label text. */
    FText GetUsageLabel() const;
    /** Usage status foreground color. */
    FLinearColor GetUsageColor() const;
    /** Usage status background color. */
    FLinearColor GetUsageBgColor() const;
```

在 `MaterialLayoutViewModel.cpp` 末尾添加实现：

```cpp
// ============================================================================
// FMLPParamVM — Display helpers
// ============================================================================

FText FMLPParamVM::GetUsageLabel() const
{
    switch (Usage)
    {
    case EMLPParameterUsage::Used:     return FText::FromString(TEXT("已使用"));
    case EMLPParameterUsage::Unused:   return FText::FromString(TEXT("未使用"));
    case EMLPParameterUsage::HalfUsed: return FText::FromString(TEXT("部分"));
    case EMLPParameterUsage::Indirect: return FText::FromString(TEXT("间接"));
    default:                           return FText::FromString(TEXT("未知"));
    }
}

FLinearColor FMLPParamVM::GetUsageColor() const
{
    switch (Usage)
    {
    case EMLPParameterUsage::Used:     return FMLPTheme::StatusUsed();
    case EMLPParameterUsage::Unused:   return FMLPTheme::StatusUnused();
    case EMLPParameterUsage::HalfUsed: return FMLPTheme::StatusHalfUsed();
    case EMLPParameterUsage::Indirect: return FMLPTheme::StatusIndirect();
    default:                           return FMLPTheme::StatusUnknown();
    }
}

FLinearColor FMLPParamVM::GetUsageBgColor() const
{
    // Background = foreground color with low alpha
    FLinearColor C = GetUsageColor();
    C.A = 0.25f;
    return C;
}
```

注意：`MaterialLayoutViewModel.cpp` 需新增 `#include "MaterialLayoutProTheme.h"`。

- [ ] **Step 2: 给 FMLPTheme 加类型颜色/缩写映射**

在 `MaterialLayoutProTheme.h` 的 class 内末尾（`TabHover()` 之后）添加：

```cpp
    // Type → color mapping (for pill badges without needing FMLPParameterInfo).
    static FLinearColor GetTypeColorForType(EMLPParameterType Type)
    {
        switch (Type)
        {
        case EMLPParameterType::Scalar:       return TypeScalar();
        case EMLPParameterType::Vector:       return TypeVector();
        case EMLPParameterType::Texture:      return TypeTexture();
        case EMLPParameterType::StaticBool:
        case EMLPParameterType::StaticSwitch: return TypeStatic();
        default:                              return Muted();
        }
    }

    static FText GetTypeAbbrForType(EMLPParameterType Type)
    {
        switch (Type)
        {
        case EMLPParameterType::Scalar:       return FText::FromString(TEXT("S"));
        case EMLPParameterType::Vector:       return FText::FromString(TEXT("V"));
        case EMLPParameterType::Texture:      return FText::FromString(TEXT("T"));
        case EMLPParameterType::StaticBool:   return FText::FromString(TEXT("SB"));
        case EMLPParameterType::StaticSwitch: return FText::FromString(TEXT("SS"));
        default:                              return FText::FromString(TEXT("?"));
        }
    }
```

注意：`MaterialLayoutProTheme.h` 需新增 `#include "Model/MaterialParameterInfo.h"`（为了 `EMLPParameterType`）。

- [ ] **Step 3: 编译验证**

```bash
F:\UE4\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development -Project="E:\Project\UE\WJGZ 4.26\WJGZ.uproject" -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```
Expected: 编译成功（SMaterialParameterRow 已重写并接 VM，但尚未被主面板引用 —— 主面板仍在用旧代码，所以不冲突）。

---

### Task 2.4: 重写 SMaterialLayoutProPanel 头文件

**Files:**
- Rewrite: `Source/MaterialLayoutPro/Public/Widgets/SMaterialLayoutProPanel.h`

- [ ] **Step 1: 重写头文件**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialInstance;
class FMLPSession;
class SEditableTextBox;
class SWidgetSwitcher;

class MATERIALLAYOUTPRO_API SMaterialLayoutProPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMaterialLayoutProPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SMaterialLayoutProPanel() override;

    // SCompoundWidget — 用于检测尺寸变化实现自适应
    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
    // --- Data ---
    TSharedPtr<FMLPSession> Session;
    TWeakObjectPtr<UMaterial> TargetMaterial;
    TWeakObjectPtr<UMaterialInstance> TargetMaterialInstance;

    // --- Selection ---
    TSharedPtr<FMLPParamVM> SelectedParam;

    // --- Layout state ---
    bool bIsWideMode = true;     // true = 双栏, false = 单栏
    float CachedWidth = 0.f;

    // --- UI containers ---
    TSharedPtr<SWidgetSwitcher> ModeSwitcher;  // 切换宽/窄模式
    TSharedPtr<SBox> LeftTreeContainer;        // 左栏树容器（宽模式）
    TSharedPtr<SBox> RightDetailContainer;     // 右栏详情容器（宽模式）
    TSharedPtr<SBox> NarrowListContainer;      // 单栏列表容器（窄模式）

    // --- Selection ---
    void SelectParam(TSharedPtr<FMLPParamVM> Param);

    // --- Selection change ---
    void OnSelectionChanged(UObject* Selection);
    void RefreshParameters();

    // --- UI builders ---
    TSharedRef<SWidget> BuildToolbar();
    TSharedRef<SWidget> BuildStatusBar();
    TSharedRef<SWidget> BuildWideMode();     // 双栏
    TSharedRef<SWidget> BuildNarrowMode();   // 单栏
    TSharedRef<SWidget> BuildLeftTree();     // 左栏两级树
    TSharedRef<SWidget> BuildRightDetail();  // 右栏详情
    TSharedRef<SWidget> BuildNarrowList();   // 窄栏手风琴列表

    // --- Adaptive ---
    void UpdateLayoutMode(float Width);

    // --- Status text ---
    FText GetStatusText() const;

    // --- Toolbar handlers (保留现有逻辑，只改数据源) ---
    FReply OnRefreshClicked();
    FReply OnSelectMaterialClicked();
    FReply OnOpenMaterialEditorClicked();
    FReply OnArchiveUnusedClicked();
    FReply OnDeleteUnusedClicked();
    FReply OnAutoGroupClicked();
    FReply OnBulkRenameClicked();
    FReply OnExportClicked();
    FReply OnImportClicked();
    FReply OnSortWorkbenchClicked();
    FReply OnParameterEditorClicked();
    FReply OnGroupByCommentClicked();
    FReply OnApplyChangesClicked();  // 新增：推送 dirty 回材质

    FText GetTargetMaterialName() const;
};
```

**与旧版关键差异:**
1. 删除 `IDetailsView` / `UMLPEditorData` 相关成员
2. 新增 `FMLPSession` 作为数据源
3. 新增 `Tick` override + `SWidgetSwitcher` 实现自适应
4. 工具栏新增「应用更改」按钮（`OnApplyChangesClicked` → `Session->PushDirty()`）
5. 新增 `SelectedParam` 跟踪选中项

---

### Task 2.5: 重写 SMaterialLayoutProPanel 实现

**Files:**
- Rewrite: `Source/MaterialLayoutPro/Private/Widgets/SMaterialLayoutProPanel.cpp`

- [ ] **Step 1: 重写 Construct + 自适应逻辑**

```cpp
#include "Widgets/SMaterialLayoutProPanel.h"
#include "MaterialLayoutProTheme.h"
#include "MaterialLayoutProSettings.h"
#include "Model/MaterialLayoutViewModel.h"
// ... 其余 includes 同旧版

#define LOCTEXT_NAMESPACE "SMaterialLayoutProPanel"

SMaterialLayoutProPanel::~SMaterialLayoutProPanel()
{
    USelection::SelectionChangedEvent.RemoveAll(this);
}

void SMaterialLayoutProPanel::Construct(const FArguments& InArgs)
{
    // 创建 Session
    Session = MakeShared<FMLPSession>();

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FMLPTheme::Background())
        .BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
        .Padding(FMargin(6.f, 4.f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 6)) [ BuildToolbar() ]
            + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 4)) [ BuildStatusBar() ]
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                // WidgetSwitcher 在宽/窄模式间切换
                SAssignNew(ModeSwitcher, SWidgetSwitcher)
                + SWidgetSwitcher::Slot().Index(0) [ BuildWideMode() ]    // 宽模式
                + SWidgetSwitcher::Slot().Index(1) [ BuildNarrowMode() ]  // 窄模式
            ]
        ]
    ];

    USelection::SelectionChangedEvent.AddSP(SharedThis(this), &SMaterialLayoutProPanel::OnSelectionChanged);
    OnSelectionChanged(nullptr);
}

void SMaterialLayoutProPanel::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    UpdateLayoutMode(AllottedGeometry.GetLocalSize().X);
}

void SMaterialLayoutProPanel::UpdateLayoutMode(float Width)
{
    // 滞回区：>496 转双栏，<464 转单栏（避免抖动）
    bool bNewMode = bIsWideMode;
    if (!bIsWideMode && Width > 496.f) bNewMode = true;
    else if (bIsWideMode && Width < 464.f) bNewMode = false;

    if (bNewMode != bIsWideMode)
    {
        bIsWideMode = bNewMode;
        if (ModeSwitcher.IsValid())
        {
            ModeSwitcher->SetActiveWidgetIndex(bIsWideMode ? 0 : 1);
        }
    }
    CachedWidth = Width;
}
```

- [ ] **Step 2: 重写 BuildWideMode + BuildLeftTree + BuildRightDetail**

```cpp
TSharedRef<SWidget> SMaterialLayoutProPanel::BuildWideMode()
{
    return SNew(SHorizontalBox)
    + SHorizontalBox::Slot().FillWidth(0.38f)
    [
        SAssignNew(LeftTreeContainer, SBox)
        [
            BuildLeftTree()
        ]
    ]
    + SHorizontalBox::Slot().FillWidth(0.62f).Padding(FMLPTheme::PadMD())
    [
        SAssignNew(RightDetailContainer, SBox)
        [
            BuildRightDetail()
        ]
    ];
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildLeftTree()
{
    // 两级树：分组节点（可折叠）+ 参数子节点
    // 搜索框 + STreeView<TSharedPtr<FMLPGroupVM>>
    // 点参数节点 → SelectParam(VM)
    //
    // 完整 Slate 实现参考设计文档形态一截图。
    // STreeView 的 OnGenerateRow 生成分组 header（▼分组名(计数)）
    // OnGenerateRow 子项生成参数行（▸名字 + 类型pill）
    //
    // 骨架：
    return SNew(SVerticalBox)
    + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 4))
    [
        // 搜索框
        SNew(SEditableTextBox)
        .HintText(LOCTEXT("SearchHint", "搜索参数..."))
        .Font(FMLPTheme::FontSmall())
    ]
    + SVerticalBox::Slot().FillHeight(1.0f)
    [
        // 参数树（用 SScrollBox + 手写分组列表，或 STreeView）
        // 详见 Task 2.5 Step 3 的树构建逻辑
        SNew(SScrollBox)
    ];
}

TSharedRef<SWidget> SMaterialLayoutProPanel::BuildRightDetail()
{
    // 右栏详情：选中参数的类型pill + 名字 + 状态 + 值编辑器 + 分组/优先级
    // 若 SelectedParam 无效，显示空状态提示
    // 用 SWidgetSwitcher 或条件渲染
    //
    // 骨架：若 SelectedParam 有效，显示 SMaterialParameterRow(SelectedParam, Session, true)
    //       否则显示「选择一个参数查看详情」
    return SNew(SBorder)
    .BorderBackgroundColor(FMLPTheme::Surface())
    .BorderImage(MLP_STYLE::GetBrush("WhiteBrush"))
    .Padding(FMLPTheme::PadMD())
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            // 标题行：类型pill + 参数名
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                FMLPTheme::MakeTypePill(
                    SelectedParam.IsValid() ? FMLPTheme::GetTypeAbbrForType(SelectedParam->Type) : FText::GetEmpty(),
                    SelectedParam.IsValid() ? FMLPTheme::GetTypeColorForType(SelectedParam->Type) : FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(FMLPTheme::PadSM())
            [
                SNew(STextBlock)
                .Text(SelectedParam.IsValid() ? FText::FromName(SelectedParam->Name) : LOCTEXT("NoSel", "未选择参数"))
                .Font(FMLPTheme::FontTitle())
                .ColorAndOpacity(FMLPTheme::Foreground())
            ]
        ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(0, 8, 0, 0))
        [
            // 值编辑区：若选中了参数，显示完整的 SMaterialParameterRow
            SelectedParam.IsValid()
                ? StaticCastSharedRef<SWidget>(
                    SNew(SMaterialParameterRow)
                    .ParamVM(SelectedParam)
                    .Session(Session)
                    .bSelected(true))
                : StaticCastSharedRef<SWidget>(
                    SNew(STextBlock)
                    .Text(LOCTEXT("SelectPrompt", "从左侧选择一个参数查看详情"))
                    .ColorAndOpacity(FMLPTheme::Muted())
                    .Font(FMLPTheme::FontBody()))
        ]
    ];
}
```

- [ ] **Step 3: 重写 BuildNarrowMode（手风琴单栏）**

```cpp
TSharedRef<SWidget> SMaterialLayoutProPanel::BuildNarrowMode()
{
    // 窄模式：无右栏，点参数行原地展开
    // 用 SScrollBox + 分组 header + 可展开参数行
    //
    // 每行：[类型pill] 名字 [值摘要]  ← 点行展开 SMaterialParameterRow
    return SAssignNew(NarrowListContainer, SBox)
    [
        SNew(SScrollBox)
        // 内容在 RefreshParameters() 中动态填充
    ];
}
```

- [ ] **Step 4: 重写 RefreshParameters（数据源改为 Session）**

```cpp
void SMaterialLayoutProPanel::RefreshParameters()
{
    if (!Session.IsValid()) return;

    if (TargetMaterial.IsValid())
    {
        Session->TargetMaterial = TargetMaterial;
        Session->PullAll();   // 重建 VM 树（交互锁保护）

        // 刷新左栏树 + 右栏详情
        if (LeftTreeContainer.IsValid())    LeftTreeContainer->SetContent(BuildLeftTree());
        if (RightDetailContainer.IsValid()) RightDetailContainer->SetContent(BuildRightDetail());
        if (NarrowListContainer.IsValid())  NarrowListContainer->SetContent(BuildNarrowList());
    }
    else
    {
        Session->Groups.Reset();
        // 显示空状态
    }
}
```

**与旧版关键差异:** 从 `EditorData->BuildFromMaterial` + `DetailsView->SetObject` 改为 `Session->PullAll()` + 手动重建树/详情。

- [ ] **Step 5: 重写工具栏 handlers**

工具栏按钮的 handler 逻辑大部分不变（它们操作的是材质本身，然后调 `RefreshParameters()`）。关键变化：

1. **新增「应用更改」按钮** — 调用 `Session->PushDirty()` 把 VM 的改动推回材质：

```cpp
FReply SMaterialLayoutProPanel::OnApplyChangesClicked()
{
    if (Session.IsValid())
    {
        Session->PushDirty();
        // PushDirty 后需要刷新（值已回写，重新拉取确保一致）
        RefreshParameters();
    }
    return FReply::Handled();
}
```

2. 状态栏显示 dirty 状态：

```cpp
FText SMaterialLayoutProPanel::GetStatusText() const
{
    if (!TargetMaterial.IsValid() && !TargetMaterialInstance.IsValid())
        return LOCTEXT("NS", "未选择材质");

    if (!Session.IsValid() || Session->Groups.Num() == 0)
        return LOCTEXT("NP", "未找到参数");

    int32 Total = 0;
    for (const auto& G : Session->Groups) Total += G->Parameters.Num();

    FString Base = FString::Printf(TEXT("%d 个参数 | %d 个分组"), Total, Session->Groups.Num());
    if (Session->HasDirty()) Base += TEXT("  |  ● 有未提交改动");

    return FText::FromString(Base);
}
```

3. 其余 handler（`OnArchiveUnusedClicked` / `OnDeleteUnusedClicked` / `OnAutoGroupClicked` / `OnGroupByCommentClicked` / `OnExportClicked` / `OnImportClicked` / `OnSortWorkbenchClicked` / `OnParameterEditorClicked`）—— **逻辑保持不变**，它们操作材质后调用 `RefreshParameters()`，而 RefreshParameters 现在走 Session。只需删除旧版对 `UMLPEditorData` 的依赖即可。

- [ ] **Step 6: 编译验证 + 编辑器内验收**

```bash
F:\UE4\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development -Project="E:\Project\UE\WJGZ 4.26\WJGZ.uproject" -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```
Expected: 编译成功。

**编辑器内验收清单：**
- [ ] 打开 WJGZ 项目，打开「材质布局 Pro」面板
- [ ] 在内容浏览器选择一个有参数的材质 → 面板显示参数树
- [ ] 宽面板：左栏两级树 + 右栏详情可见
- [ ] 拖窄面板：切换到单栏手风琴模式
- [ ] 选中参数 → 右栏显示值编辑器
- [ ] 编辑值 → 状态栏显示「● 有未提交改动」
- [ ] 点「应用更改」→ 值回写材质，dirty 清除

- [ ] **Step 7: 提交**

```bash
cd F:\Cache\AI\MaterialLayoutPro
git add Source/MaterialLayoutPro/Public/Widgets/SMaterialParameterRow.h \
        Source/MaterialLayoutPro/Private/Widgets/SMaterialParameterRow.cpp \
        Source/MaterialLayoutPro/Public/Widgets/SMaterialLayoutProPanel.h \
        Source/MaterialLayoutPro/Private/Widgets/SMaterialLayoutProPanel.cpp \
        Source/MaterialLayoutPro/Public/Model/MaterialLayoutViewModel.h \
        Source/MaterialLayoutPro/Private/Model/MaterialLayoutViewModel.cpp \
        Source/MaterialLayoutPro/Public/MaterialLayoutProTheme.h
git commit -m "feat(panel): 重写主面板为双栏+自适应，行控件接入 VM

- SMaterialParameterRow: 绑定 FMLPParamVM，值控件读写 VM 不直绑 Expression
- SMaterialLayoutProPanel: 双栏(宽)/手风琴(窄)自适应，480px阈值+滞回
- 新增「应用更改」按钮 → Session.PushDirty()
- FMLPParamVM/FMLPTheme: 添加类型/状态显示辅助方法"
```

---
