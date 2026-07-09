# 交接文档 — MaterialLayoutPro UI 重做

> **用途**：在另一台电脑上接手此项目的开发。读完此文档即可开始编码。

---

## 1. 项目一句话

MaterialLayoutPro 是一个 **UE 4.26 编辑器插件**，用 Slate 提供材质参数的集中管理面板（可视化、分组、排序、批量编辑、清理未使用参数）。

**当前正在做的事**：把全部 UI 从 IDetailsView 方案重写为纯手写 Slate + ViewModel 快照层。

---

## 2. 环境要求

| 项 | 要求 |
|---|---|
| 引擎 | **UE 4.26**（必须，项目依赖 4.26 API） |
| 编译器 | MSVC（Visual Studio 2019/2022） |
| 平台 | Windows x64 |
| Git | 需要 |

### 2.1 当前开发机路径（换电脑后需调整）

| 角色 | 当前路径 | 说明 |
|---|---|---|
| **源码仓库** | `F:\Cache\AI\MaterialLayoutPro` | Git 仓库根目录 |
| **UE4 引擎** | `F:\UE4` | UE 4.26 安装路径 |
| **目标项目** | `E:\Project\UE\WJGZ 4.26\WJGZ.uproject` | 插件部署到这里测试 |
| **部署目标** | `E:\Project\UE\WJGZ 4.26\Plugins\MaterialLayoutPro` | 编译产物投放处 |

> **换电脑后**：克隆仓库到任意位置，修改编译命令中的引擎路径和项目路径即可。

---

## 3. 快速开始（新机器）

```bash
# 1. 克隆仓库
git clone https://github.com/18163623522/MaterialLayoutPro.git
cd MaterialLayoutPro

# 2. 把插件软链或复制到你的 UE 项目的 Plugins 目录
#    方式 A（推荐）：建符号链接（改一处即生效）
mklink /D "你的项目\Plugins\MaterialLayoutPro" "仓库路径\MaterialLayoutPro"
#    方式 B：直接复制文件夹

# 3. 编译（替换路径为你的环境）
你的UE4路径\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development ^
  -Project="你的项目路径\xxx.uproject" ^
  -Module=MaterialLayoutPro -WaitMutex -FromMsBuild

# 4. 打开 UE4 编辑器，启用插件
#    编辑 → 插件 → 搜索 "Material Layout Pro" → 勾选 → 重启
```

---

## 4. 项目结构

```
MaterialLayoutPro/
├── MaterialLayoutPro.uplugin        # 插件描述符
├── README.md                         # 原版说明
├── .gitignore                        # 已排除 .superpowers/ Binaries/ Intermediate/
│
├── docs/
│   ├── HANDOFF.md                    # ← 本文档
│   └── superpowers/
│       ├── specs/
│       │   └── 2026-07-09-ui-rewrite-design.md   # ★ 设计文档（必读）
│       └── plans/
│           └── 2026-07-09-ui-rewrite.md          # ★ 实现计划（阶段1-2已写，3-6待续）
│
└── Source/MaterialLayoutPro/
    ├── MaterialLayoutPro.Build.cs    # 模块依赖声明
    │
    ├── Public/                       # 头文件
    │   ├── MaterialLayoutPro.h       # 模块入口
    │   ├── MaterialLayoutProTheme.h  # ★ 样式令牌系统（颜色/字体/间距/辅助控件）
    │   ├── MaterialLayoutProSettings.h
    │   ├── MLPEditorData.h           # ⚠️ 待删除（被 VM 取代）
    │   ├── Model/
    │   │   ├── MaterialParameterInfo.h      # 参数信息结构（不动）
    │   │   └── MaterialParameterDragDrop.h
    │   ├── Style/MaterialLayoutProStyle.h
    │   ├── Commands/MaterialLayoutProCommands.h
    │   └── Widgets/
    │       ├── SMaterialLayoutProPanel.h     # 主面板（待重写）
    │       ├── SMaterialParameterRow.h       # 参数行控件（待重写）
    │       ├── SMaterialParameterEditor.h    # 参数编辑器（待重写）
    │       ├── SMaterialSortWorkbench.h      # 排序工作台（待重写）
    │       ├── SMaterialBulkRenameDialog.h   # 批量重命名（待重写）
    │       └── SMaterialLayoutPreviewDialog.h # 预览对话框（待重写）
    │
    └── Private/                      # 实现文件（与 Public 对应）
        ├── Model/
        │   ├── MaterialParameterScanner.h/.cpp   # ★ 扫描器（不动，VM 复用它）
        │   ├── MaterialParameterUsageAnalyzer.h/.cpp  # 使用分析器（不动）
        │   └── MaterialParameterInfo.cpp
        └── Widgets/（与 Public 对应的 .cpp）
```

### 不动的文件（重做时不要改）

| 文件 | 原因 |
|---|---|
| `MaterialLayoutProTheme.h` | 样式令牌系统已完善，所有窗口统一消费 |
| `MaterialParameterScanner.*` | 扫描逻辑稳定，VM 复用它 |
| `MaterialParameterUsageAnalyzer.*` | 使用分析逻辑稳定 |
| `MaterialParameterInfo.*` | 参数信息结构，VM 的数据源 |
| `MaterialLayoutProSettings.*` | 插件设置 |
| `MaterialLayoutProStyle.*` | 样式注册 |
| `MaterialLayoutProCommands.*` | 命令/菜单注册 |

---

## 5. 当前状态：UI 重做进行中

### 5.1 进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| 设计 | 需求分析 + 方案确定 | ✅ 完成 |
| 阶段 1 | ViewModel 层（FMLPParamVM/FMLPGroupVM/FMLPSession） | 📋 计划已写，**未编码** |
| 阶段 2 | 主面板 + 行控件重写 | 📋 计划已写，**未编码** |
| 阶段 3 | 参数编辑器 | 🔶 计划待展开 |
| 阶段 4 | 排序工作台 | 🔶 计划待展开 |
| 阶段 5 | 批量重命名 + 预览对话框 | 🔶 计划待展开 |
| 阶段 6 | 删除 MLPEditorData + 全量验收 | 🔶 计划待展开 |

> **重要**：代码**一行都还没改**。当前仓库的源码是重做前的状态（IDetailsView 方案）。所有改动方案都在文档里。

### 5.2 Git 状态

```
b3af776 docs: 添加 UI 重做实现计划（阶段1-2 完成，阶段3-6 待续）
d04a530 chore: baseline 提交（UI 重做前的现状快照）
4dd6941 docs: 在 README 中添加原版参考链接
```

- `d04a530` 是 **baseline**（重做前的完整可编译状态，万一改坏了从这里回退）
- `b3af776` 是**计划文档**（不含任何代码改动）

---

## 6. 核心设计速览

### 6.1 为什么重做

当前主面板用 `IDetailsView` + `UMLPEditorData`（Instanced 数组）渲染参数，问题：
- 引擎无法正确处理 Instanced 数组的折叠分组
- 丢失了精心设计的类型 pill / 状态徽章 / 自定义值编辑器
- 主面板和独立窗口用两套不同的数据流

### 6.2 新架构（三层）

```
UMaterial（引擎对象，值的真正归属地）
    ↓ Scanner 扫描（不动）
FMLPParameterInfo（只读扫描结果，不动）
    ↓ Session.PullAll() 转换
═══════════════════════════════════════
ViewModel 快照层（新增 · 核心）
  FMLPParamVM   → 单参数快照（值+类型+状态，可编辑）
  FMLPGroupVM   → 分组容器
  FMLPSession   → 会话管理 + 交互锁
    ↓ TAttribute 绑定（控件实例稳定，不随刷新重建）
═══════════════════════════════════════
UI 层（全部重写 · 手写 Slate）
  主面板（双栏+自适应）+ 4 个独立窗口
```

### 6.3 核心创新：交互锁（解决焦点丢失）

上次手写 Slate 失败的根因：值直接绑引擎对象，"外部刷新"和"用户输入"冲突。

**这次解法**：VM 作为中间快照层 + 交互锁计数器
- 用户编辑控件 → 写 VM（同步，不碰引擎对象）→ 控件焦点保持
- 外部变更 → 检查 `InteractingCount`，空闲才 Pull，编辑中暂缓
- 刷新时不重建控件（VM 指针稳定）→ 焦点不丢

### 6.4 主面板布局

- **宽面板（>480px）**：左栏两级树（分组+参数）+ 右栏详情编辑
- **窄面板（≤480px）**：手风琴单栏，点行原地展开
- 480px 阈值，16px 滞回区防抖动

---

## 7. 怎么继续开发

### 方式一：让 AI 助手接手

直接对 AI 说：

> **"读取 MaterialLayoutPro 仓库的 docs/superpowers/plans/2026-07-09-ui-rewrite.md，继续执行 UI 重做计划，从阶段 1 开始"**

AI 会：
1. 读取计划文件（阶段 1-2 已有完整代码）
2. 补全阶段 3-6 的任务
3. 逐阶段编码 → 编译 → 提交

### 方式二：手动开发

1. **先读设计文档**：`docs/superpowers/specs/2026-07-09-ui-rewrite-design.md`
2. **再读实现计划**：`docs/superpowers/plans/2026-07-09-ui-rewrite.md`
   - 计划里阶段 1-2 有**完整的可复制代码**（VM 头文件、实现文件、主面板骨架）
3. 按计划逐 Task 编码，每完成一个 Task 编译验证

### 编译命令模板

```bash
# 替换 <UE4路径> 和 <项目路径>
<UE4路径>\Engine\Build\Windows\UnrealBuildTool.exe UE4Editor Win64 Development -Project="<项目路径>\xxx.uproject" -Module=MaterialLayoutPro -WaitMutex -FromMsBuild
```

### 提交规范

每个阶段完成后提交一次（分级回退）：
```bash
git add -A
git commit -m "feat(阶段名): 简述"
```

---

## 8. 关键技术备忘

### 8.1 UE 版本兼容宏（已建立约定，必须遵守）

```cpp
#if ENGINE_MAJOR_VERSION >= 5
  #include "Styling/AppStyle.h"
  #define MLP_STYLE FAppStyle
#else
  #include "EditorStyleSet.h"
  #define MLP_STYLE FEditorStyle
#endif
```

表达式集合访问：
```cpp
#if ENGINE_MAJOR_VERSION >= 5
  M->GetExpressionCollection().RemoveExpression(Expr);
#else
  M->Expressions.Remove(Expr);
#endif
```

颜色选择器初始化：
```cpp
#if ENGINE_MAJOR_VERSION >= 5
  Args.InitialColor = Color;
#else
  Args.InitialColorOverride = Color;
#endif
```

### 8.2 Pull/Push 逻辑参考

新 VM 的 Pull/Push 直接参考现有 `MLPEditorData.cpp`（第 19-92 行），已验证可用。计划文档阶段 1 有完整代码。

### 8.3 样式系统

所有颜色/字体/间距用 `FMLPTheme`（`MaterialLayoutProTheme.h`，173 行）：
- 颜色：`FMLPTheme::Background()` / `Surface()` / `Foreground()` / `Accent()` / `TypeScalar()` ...
- 间距：`FMLPTheme::PadSM()` / `PadMD()` / `PadLG()` ...
- 字体：`FMLPTheme::FontBody()` / `FontHeading()` / `FontBadge()` ...
- 辅助控件：`FMLPTheme::MakeTypePill()` / `MakeBadge()` / `MakeSeparator()`

---

## 9. 已知问题 / 注意事项

1. **只有 1 次 baseline + 文档提交** — 当前无代码改动，回退到 `d04a530` 就是原始状态
2. **`.superpowers/` 目录** — 是 AI 头脑风暴工具的临时文件，已在 `.gitignore` 排除，不影响项目
3. **无单元测试** — 纯编辑器 Slate 插件，验证靠编译通过 + 编辑器内手动验收
4. **MaterialInstance 支持** — 当前 Scanner 对 MI 会 fallback 到 base material，参数编辑器的「应用到实例」功能待重写时完善

---

## 10. 联系点

- **仓库**：https://github.com/18163623522/MaterialLayoutPro.git
- **设计文档**：`docs/superpowers/specs/2026-07-09-ui-rewrite-design.md`
- **实现计划**：`docs/superpowers/plans/2026-07-09-ui-rewrite.md`

**接手第一件事**：通读设计文档（spec），它包含了所有决策的来龙去脉和理由。
