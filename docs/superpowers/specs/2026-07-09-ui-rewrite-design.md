# MaterialLayoutPro UI 重做设计

**日期**: 2026-07-09
**状态**: 待批准
**引擎**: UE 4.26（`#if ENGINE_MAJOR_VERSION >= 5` 兼容 5.x）
**仓库**: `F:\Cache\AI\MaterialLayoutPro\Source\MaterialLayoutPro\`

---

## 1. 背景与动机

### 1.1 当前状态

主面板 `SMaterialLayoutProPanel` 当前使用 `IDetailsView` + `UMLPEditorData`（Instanced 数组）方案渲染参数。该方案存在以下问题：

- **分组折叠失效** — 引擎无法正确处理 Instanced 数组的折叠组
- **视觉失控** — `SMaterialParameterRow`（522 行，含类型 pill / 状态徽章 / 自定义值编辑器）被弃用，丢失了精心设计的视觉信息
- **数据流割裂** — 主面板用 `UMLPEditorData`，独立窗口用 `TArray<TSharedPtr<FMLPParameterInfo>>` 深拷贝，两套并行

### 1.2 上次手写 Slate 的失败根因

上次手写 Slate 值编辑器被否，根因是数据流设计无法兼顾两个矛盾需求：

| 方案 | 死因 |
|---|---|
| TAttribute 每帧拉取 Expression | 用户输入时值被覆盖（卡顿 + 输入丢失） |
| 手动 Set + 监听提交 | 外部变更不刷新；列表重建时焦点丢失 |

**根源**：值存在 `UMaterialExpression`（引擎对象）里，UI 直接绑定它，导致"外部刷新"和"用户输入不被打断"天然冲突。

### 1.3 本次重做目标

- 放弃 `IDetailsView`，回到手写 Slate，完全掌控每一行的视觉和交互
- 引入 **ViewModel 快照层**，根治焦点丢失 / 输入被打断问题
- 统一 5 个窗口的数据流和视觉风格

---

## 2. 整体架构

```
UMaterial / UMaterialInstance  （引擎对象，值的真正归属地）
        ↓ Scan（FMaterialParameterScanner，不动）
FMLPParameterInfo               （只读扫描结果，不动）
        ↓ Session.PullAll() 转换
═══════════════════════════════════════════════════
ViewModel 快照层（新增 · 核心）
  FMLPParamVM   FMLPGroupVM   FMLPSession
        ↓ TAttribute 绑定（控件实例稳定，不随刷新重建）
═══════════════════════════════════════════════════
UI 层（全部重写）
  主面板 · 参数编辑器 · 排序工作台 · 批量重命名 · 预览对话框
```

### 三个核心收益

1. **焦点问题根治** — UI 控件绑定 VM 而非引擎对象，实例稳定不重建
2. **数据流统一** — 5 个窗口共享同一套 Pull/Push，不再各搞各的
3. **视觉统一** — 全部走 `FMLPTheme` 令牌系统

---

## 3. ViewModel 层设计（核心）

### 3.1 设计原则

- VM 是 **plain struct + TSharedPtr**，不依赖 UHT（4.26 友好）
- VM **不取代** `FMLPParameterInfo`，而是在其之上加一层（Scanner 保持不动）
- 值用**独立字段**而非 union（简单直接，内存浪费可忽略）

### 3.2 数据结构

```cpp
// ① 单个参数快照 —— UI 控件绑定到这里
struct FMLPParamVM
{
    // 标识（从 FMLPParameterInfo 拷贝）
    FName Name;
    EMLPParameterType Type;
    TWeakObjectPtr<UMaterialExpression> SourceExpression;

    // 值快照（可编辑，按 Type 取用）
    float       ScalarValue  = 0.f;
    FLinearColor VectorValue = FLinearColor::White;
    TSoftObjectPtr<UTexture> TextureValue;
    bool        BoolValue    = false;

    // 组织（可编辑）
    FName   Group;
    int32   SortPriority = 0;

    // 状态（只读展示）
    EMLPParameterUsage Usage = EMLPParameterUsage::Unknown;

    // 编辑状态
    bool bDirty = false;        // 自上次 Push 后是否有改动

    void PullFromExpression();  // 从 SourceExpression 读值到快照
    void PushToExpression();    // 快照写回 SourceExpression
};

// ② 分组
struct FMLPGroupVM
{
    FName Name;
    int32 SortPriority = 0;
    bool  bExpanded = true;
    TArray<TSharedPtr<FMLPParamVM>> Parameters;
};

// ③ 会话 —— 管理一次材质编辑的完整快照 + 交互锁
class FMLPSession
{
public:
    TWeakObjectPtr<UMaterial> TargetMaterial;
    TArray<TSharedPtr<FMLPGroupVM>> Groups;

    // 交互锁（计数器，支持多控件并发）
    int32 InteractingCount = 0;
    bool  bPendingRefresh = false;

    void PullAll();               // 从材质全量重建 VM（仅当 InteractingCount==0）
    void PushDirty();             // 所有 bDirty 项一次性回写（单个 FScopedTransaction）

    void BeginInteract();         // 控件 OnFocusReceived → InteractingCount++
    void EndInteract();           // 控件 OnFocusLost → --；归零且有 pending 则 PullAll
};
```

### 3.3 交互锁设计取舍

交互锁用**计数器**而非 bool：用户可能在拖拽过程中焦点切换，计数器能准确跟踪"当前有几个控件在交互"。

### 3.4 新增文件

| 文件 | 内容 |
|---|---|
| `Public/Model/MaterialLayoutViewModel.h` | `FMLPParamVM` / `FMLPGroupVM` / `FMLPSession` |
| `Private/Model/MaterialLayoutViewModel.cpp` | Pull/Push 实现 |

---

## 4. 主面板设计（SMaterialLayoutProPanel）

### 4.1 布局：双栏 + 自适应

呈现形式：`SDockTab`（NomadTab，可停靠，宽度由用户拖动决定）。

**自适应阈值**：480px（`OnViewportResized` / `SWidget::Tick` 中检测 Geometry.Width）

#### 形态一：宽面板（> 480px）— 双栏

```
┌─ 标题栏 ──────────────────────────────────────────┐
│ 目标材质: M_BaseCharacter        [刷新][定位][打开] │
├─ 工具栏（次级操作）────────────────────────────────┤
│ [自动分组][按注释] | [归档][删除] | [重命名]       │
│ [导出][导入] | [排序工作台][参数编辑器]            │
├──────────────┬────────────────────────────────────┤
│ 左栏 38%     │ 右栏（详情）                       │
│ 两级树       │ 选中参数的完整编辑控件             │
│              │                                    │
│ 🔍 搜索...   │ [SCALAR] BaseColor                 │
│ ▼ 基础颜色(5)│ ● 已使用                           │
│   ▸BaseColor │                                    │
│   ▸Roughness │ 值: [  1.000  ]                    │
│   ▸Tint      │ 分组: [基础颜色] 优先级: [0]        │
│ ▶ 法线 (3)   │ 来源: ScalarParameter · GUID a3f2..│
│ ▶ 已废弃 (2) │                                    │
├──────────────┴────────────────────────────────────┤
│ 10 个参数 | 3 个分组 | 2 未使用  | ● 有未提交改动  │
└───────────────────────────────────────────────────┘
```

左栏两级树：
- **分组节点**（▼基础颜色）：可折叠，显示参数计数
- **参数子节点**（▸BaseColor）：带类型 pill（S/V/T）和状态点（●）
- 支持：搜索过滤、拖拽排序、多选

右栏详情：
- 类型 pill + 参数名 + 使用状态徽章
- 值编辑控件（按类型：SNumericEntryBox / 颜色块 / 纹理选择器 / 复选框）
- 分组、优先级编辑
- 来源节点信息（类型 + GUID）

#### 形态二：窄面板（≤ 480px）— 手风琴单栏

```
┌─ 标题栏（紧凑）──────────┐
│ M_BaseCharacter    [☰]  │
├─────────────────────────┤
│ ▼ 基础颜色 (5)          │
│ [S] BaseColor    1.000  │
│     基础颜色·P0·●已使用  │  ← 点行原地展开编辑控件
│ [V] Tint         ■ RGBA │
│     基础颜色·P2·●部分   │
│ ▶ 法线 (3)              │
├─────────────────────────┤
│ 10 | 3 组 | 2 未用       │
└─────────────────────────┘
```

窄形态：点参数行原地展开编辑控件（手风琴式），无右栏。

### 4.2 值编辑器（SMaterialParameterRow 重写）

每行根据 `EMLPParameterType` 生成对应编辑控件，全部绑定到 `FMLPParamVM`：

| 参数类型 | 控件 | 绑定 |
|---|---|---|
| Scalar | `SNumericEntryBox<float>` | VM.ScalarValue |
| Vector | `SColorBlock` + RGBA 输入 | VM.VectorValue |
| Texture | `SComboButton` + 资源选择 | VM.TextureValue |
| StaticBool | `SCheckBox` | VM.BoolValue |
| StaticSwitch | `SCheckBox` | VM.BoolValue |

所有控件：
- `OnFocusReceived` → `Session.BeginInteract()`
- `OnFocusLost` → `Session.EndInteract()`
- `OnCommitted` → 写 VM + `bDirty = true`

---

## 5. 独立窗口设计（全部重写）

所有独立窗口统一接入 `FMLPSession`，不再各自深拷贝 `FMLPParameterInfo`。

### 5.1 参数编辑器（SMaterialParameterEditor）

**职责**：Houdini 风格虚拟标签页参数编辑。

- 标签页：用户自定义分组（独立于材质 Group）
- 每标签页内：参数行 + 值编辑器
- 操作：添加/删除标签页、拖拽参数到其他标签页
- 应用：单个 `FScopedTransaction` 写回材质 + 可选写回实例

### 5.2 排序工作台（SMaterialSortWorkbench）

**职责**：拖拽重排分组和参数优先级。

- 可拖拽 `SListView` / `STreeView`
- 拖拽分组：调整组间顺序
- 拖拽参数：跨组移动 + 组内排序
- 工作副本基于 VM，应用时写回 Group + SortPriority

### 5.3 批量重命名（SMaterialBulkRenameDialog）

**职责**：查找/替换 + 正则 + 预览。

- 输入：查找文本、替换文本、正则开关
- 预览列表：旧名 → 新名对照
- 应用：写回 `ParameterName`，单个事务

### 5.4 预览对话框（SMaterialLayoutPreviewDialog）

**职责**：批量变更确认预览。

- 列表：每行显示 参数名 · 旧分组 → 新分组
- 确认/取消按钮
- 被"归档未使用""自动分组""按注释分组"的 DryRun 模式调用

---

## 6. 样式系统

沿用现有 `FMLPTheme`（MaterialLayoutProTheme.h，173 行，已完善）：

- 颜色令牌：Background / Surface / SurfaceAlt / Foreground / Muted / Accent / Destructive / 类型色
- 间距令牌：PadXS(2) / PadSM(4) / PadMD(8) / PadLG(12) / PadXL(16)
- 字体令牌：FontSmall(9) / FontBody(10) / FontHeading(10b) / FontTitle(12b) / FontMono / FontBadge(8b)
- 辅助控件：MakeSeparator / MakeBadge / MakeDotBadge / MakeTypePill

**本设计不修改 FMLPTheme**，所有新窗口统一消费它。

---

## 7. 兼容性

- `#if ENGINE_MAJOR_VERSION >= 5` → `FAppStyle` else `FEditorStyle`（现有约定保持）
- 表达式集合访问：UE5 `GetExpressionCollection().RemoveExpression()` / 4.26 `Expressions.Remove()`
- VM 为 plain struct，不依赖 UHT / UCLASS / USTRUCT，4.26 无障碍

---

## 8. 实现顺序与提交策略

### 8.1 提交策略

每个窗口（含 VM 层）完成后一次提交，支持分级回退。首次提交为 baseline（保存当前状态）。

### 8.2 实现顺序

| 步骤 | 内容 | 验证标准 |
|---|---|---|
| 0 | Baseline 提交（保存现状） | 现有代码可编译 |
| 1 | ViewModel 层 | Pull/Push 单元测试通过 |
| 2 | 主面板（双栏 + 自适应） | 编译通过，选中材质能显示参数树 + 详情编辑 |
| 3 | 参数编辑器 | 虚拟标签页 + 值编辑 + 应用到材质 |
| 4 | 排序工作台 | 拖拽重排 + 应用 |
| 5 | 批量重命名 | 查找/替换 + 正则 + 预览 |
| 6 | 预览对话框 | 变更预览 + 确认 |

### 8.3 编译验证命令

```bash
F:\UE4\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development \
  -Project="E:\Project\UE\WJGZ 4.26\WJGZ.uproject" \
  -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```

部署目标：`E:\Project\UE\WJGZ 4.26\Plugins\MaterialLayoutPro\`

---

## 9. 不做的事（YAGNI）

- ❌ 不修改 `FMLPTheme`（已完善）
- ❌ 不修改 `FMaterialParameterScanner`（职责单一，不动）
- ❌ 不引入新的第三方库
- ❌ 不做 Material Instance 的独立编辑面板（仅在参数编辑器中支持写回实例）
- ❌ 不做撤销/重做的自定义栈（依赖 UE 的 FScopedTransaction）

---

## 10. 风险

| 风险 | 应对 |
|---|---|
| 交互锁边界 case（如窗口失焦） | `EndInteract` 在所有焦点丢失路径触发；加超时保底 |
| 自适应阈值抖动（宽窄频繁切换） | 加 16px 滞回区（>496 才转双栏，<464 才转单栏） |
| 拖拽排序与 UE DragDrop 系统集成 | 复用现有 `MaterialParameterDragDrop.h`（如适用） |
| 4.26 缺少某些 Slate API | 实现 时遇到再查，记录在 spec 附录 |

---

## 附录：决策记录

| # | 决策 | 选择 | 日期 |
|---|---|---|---|
| 1 | 重做目标 | 放弃 IDetailsView，手写 Slate | 2026-07-09 |
| 2 | 范围 | 主面板 + 全部 5 窗口 | 2026-07-09 |
| 3 | 主面板视觉 | C 双栏 + 自适应 | 2026-07-09 |
| 4 | 自适应 | 宽双栏 / 窄手风琴单栏，阈值 480px | 2026-07-09 |
| 5 | 值编辑器数据流 | ViewModel 快照层 + 交互锁 | 2026-07-09 |
| 6 | VM 与 Scanner 关系 | VM 在 ParameterInfo 之上加层，不取代 | 2026-07-09 |
| 7 | 窗口重做程度 | 5 个全部从零重写 | 2026-07-09 |
| 8 | Git 策略 | 每窗口一提交，分级回退 | 2026-07-09 |
| 9 | 实现顺序 | VM→主面板→重交互→简单 | 2026-07-09 |
