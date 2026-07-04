# Material Layout Pro

> Unreal Engine 4.26 材质参数管理插件，参考 Core Grip Tech 的 *Material Layout Pro* 设计思路，用于在编辑器中可视化、分组、批量编辑材质参数。

---

## 当前功能

- **Nomad Tab 面板**：`Window → Material Layout Pro` 打开全局可停靠面板。
- **参数扫描**：自动读取当前选中的 `UMaterial` 或 `UMaterialInstance` 的参数节点。
- **分组折叠展示**：按 `Group` 字段对参数分组，支持展开/折叠、按排序优先级排序。
- **类型 / 状态徽章**：
  - 类型：Scalar / Vector / Texture / Static Switch
  - 状态：Used / Unused / Half-Used / Indirect / Unknown
- **多选操作**：支持 `Ctrl` 单选、`Shift` 范围选择。
- **批量改 Group**：输入 Group 名称后一键应用到所有选中参数。
- **自动分组**：根据参数名前缀自动归类（如 `Tex_` → Textures、`R_` → Channels）。
- **按注释分组**：根据材质编辑器中的 Comment 框区域将内部参数归到同名 Group。
- **归档未使用参数**：一键将 Unused 参数移到 `Deprecated` 分组。
- **删除未使用参数**：一键从材质表达式列表中移除 Unused 参数节点。
- **批量重命名对话框**：Find & Replace / 正则预览，对选中参数批量改名。
- **导出 CSV**：将参数列表导出为 `Name,Type,Group,SortPriority,Usage,Value` 格式。
- **Undo/Redo 支持**：所有写操作都通过 `FScopedTransaction` 包裹。

---

## 工程结构

```
MaterialLayoutPro/
├── MaterialLayoutPro.uplugin      # 插件描述符
├── Config/
│   └── FilterPlugin.ini
├── Resources/
│   └── Icon128.png
└── Source/MaterialLayoutPro/
    ├── MaterialLayoutPro.Build.cs
    ├── Public/
    │   ├── MaterialLayoutPro.h
    │   ├── MaterialLayoutProCommands.h
    │   ├── MaterialLayoutProStyle.h
    │   ├── MaterialLayoutProTheme.h          # 设计 token / 颜色 / helper
    │   ├── Model/
    │   │   ├── MaterialParameterInfo.h       # 参数数据模型
    │   │   └── MaterialParameterScanner.h    # 扫描器接口
    │   └── Widgets/
    │       ├── SMaterialLayoutProPanel.h     # 主面板
    │       ├── SMaterialParameterRow.h       # 参数行
    │       └── SMaterialBulkRenameDialog.h   # 批量重命名弹窗
    └── Private/
        ├── MaterialLayoutPro.cpp
        ├── MaterialLayoutProCommands.cpp
        ├── MaterialLayoutProStyle.cpp
        ├── Model/
        │   ├── MaterialParameterScanner.cpp
        │   └── MaterialParameterUsageAnalyzer.cpp
        └── Widgets/
            ├── SMaterialLayoutProPanel.cpp
            ├── SMaterialParameterRow.cpp
            └── SMaterialBulkRenameDialog.cpp
```

---

## 技术要点

- **命名隔离**：避免与引擎的 `FMaterialParameterInfo`/`EMaterialParameterType` 冲突，插件内部使用 `FMLPParameterInfo` / `EMLPParameterType`。
- **参数扫描**：遍历 `UMaterial->Expressions`，识别 `UMaterialExpressionParameter` 及其子类。
- **使用状态分析**：通过反射遍历 `FExpressionInput` 属性，判断参数是否被连接到输出节点。
- **Slate 声明式 UI**：使用 `SCompoundWidget`、`SVerticalBox`、`SHorizontalBox`、`SBorder`、`SListView` 等构建面板。
- **批量操作优化**：`ArchiveUnused` / `AutoGroup` / `GroupByComment` 等内部操作使用 `ApplyGroupChangeInternal`，避免在循环中反复刷新 UI。
- **4.26 兼容**：
  - `SBorder::Padding` 使用 `FMargin` 而不是两个 `float`。
  - 不使用 `SBorder::CornerRadius`（4.26 不存在），圆点使用 `SImage` 实现。
  - 节点坐标使用 `MaterialExpressionEditorX` / `MaterialExpressionEditorY`。
  - 使用 `FDesktopPlatformModule::Get()` 直接调用文件对话框。

---

## 安装与编译

### 方式一：直接作为项目插件

1. 将本仓库复制到目标项目的 `Plugins/MaterialLayoutPro` 目录。
2. 在 `.uproject` 或项目设置中启用 `MaterialLayoutPro`。
3. 启动编辑器，首次加载时会触发自动编译。

### 方式二：使用 RunUAT 打包

```bash
"Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin ^
  -Plugin="/path/to/MaterialLayoutPro/MaterialLayoutPro.uplugin" ^
  -Package="/path/to/output" ^
  -Rocket
```

---

## 使用方式

1. 在 Content Browser 中选中一个材质。
2. 打开 `Window → Material Layout Pro` 面板。
3. 面板会自动读取选中材质的参数并展示。
4. 选中参数后：
   - 在 `Group` 输入框输入分组名并点击 `Set`。
   - 点击 `Auto Group` 按前缀自动分组。
   - 点击 `Archive Unused` 将未使用参数移到 `Deprecated`。
   - 点击 `Delete Unused` 删除未使用参数。
   - 点击 `Bulk Rename` 打开批量重命名对话框。
   - 点击 `Export` 导出 CSV。

---

## 完整开发计划（Roadmap）

### Phase 1 — 面板功能完善（当前阶段）

目标：让全局 Nomad Tab 面板接近原版 Material Layout Pro 的 80% 体验，无需嵌入 Material Editor 即可独立工作。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P0 | 分组折叠 UI | ✅ | 按 Group 折叠，显示参数数量 |
| P0 | 类型 / 状态徽章 | ✅ | Scalar/Vector/Texture/StaticSwitch + Used/Unused/HalfUsed/Indirect |
| P0 | 多选操作 | ✅ | Ctrl 单选、Shift 范围选择 |
| P0 | 批量改 Group | ✅ | 输入框 + Set 按钮应用到选中项 |
| P0 | 未使用参数归档 / 删除 | ✅ | 移动到 Deprecated / 直接删除节点 |
| P0 | 自动分组 | ✅ | 硬编码前缀规则 |
| P0 | 按注释分组 | ✅ | 根据 Comment 框区域分组 |
| P1 | 批量重命名对话框 | ✅ | Find/Replace + 正则预览 |
| P1 | 导出 CSV | ✅ | 导出 Name/Type/Group/Priority/Usage/Value |
| P1 | 可配置的自动分组规则 | ⏳ | 从 INI 或设置对话框读取前缀规则 |
| P1 | 右键菜单 | ⏳ | 改 Group、改 Priority、复制名称 |
| P2 | 搜索 / 过滤 | ⏳ | 按名称、类型、状态过滤 |
| P2 | 排序自定义 | ⏳ | 按名称、类型、优先级、使用状态排序 |
| P2 | 节点参数提示 | ⏳ | 鼠标悬停显示 Value、Expression 路径等 |

### Phase 2 — 材质实例支持

目标：不仅支持 `UMaterial`，也支持 `UMaterialInstance` 及其继承链。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P0 | 读取实例参数 | ⏳ | 识别父材质参数与实例覆盖参数 |
| P0 | 区分默认值与覆盖值 | ⏳ | 在 UI 中标识哪些参数被实例覆盖 |
| P1 | 在实例上修改 Group | ⏳ | 修改父材质参数的分组，影响所有实例 |
| P1 | 显示实例覆盖值 | ⏳ | 在参数行展示实例实际使用的值 |

### Phase 3 — 与 Material Editor 集成

目标：将面板真正嵌入到 Material Editor 中，实现双向联动。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P0 | Material Editor 工具栏入口 | ⏳ | 在材质编辑器工具栏添加打开面板按钮 |
| P0 | 面板中选中高亮节点 | ⏳ | 在面板点击参数时，在 Material Editor 中高亮对应表达式节点 |
| P0 | 节点选中同步面板 | ⏳ | 在 Material Editor 中选中参数节点时，面板同步选中 |
| P1 | 拖拽到 Comment 自动分组 | ⏳ | 将节点拖入 Comment 区域自动按 Comment 命名分组 |
| P1 | 双击定位节点 | ⏳ | 双击参数行跳转到 Material Editor 对应节点位置 |

### Phase 4 — 高级功能

目标：提供参数整理、清理、迁移等高级批量能力。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P1 | 批量改 Sort Priority | ⏳ | 对选中参数统一偏移或设置优先级 |
| P1 | 使用链路可视化 | ⏳ | 显示参数的上游输入 / 下游输出节点 |
| P1 | 一键清理 Deprecated | ⏳ | 删除所有 Deprecated 分组中的节点 |
| P2 | 重命名同步 Material Instance | ⏳ | 参数改名时同步更新实例覆盖记录 |
| P2 | 导入 CSV 反向修改 | ⏳ | 从 CSV 读取 Group/Priority 并写回材质 |
| P2 | 参数去重检测 | ⏳ | 检测同名 GUID 冲突或重复参数名 |
| P3 | 预设模板 | ⏳ | 保存/加载常用的分组规则为模板 |
| P3 | 批量创建参数 | ⏳ | 根据 CSV/JSON 批量在材质中创建参数节点 |

---

## 关键设计决策

1. **为什么用独立 Nomad Tab 而不是直接嵌入 Material Editor？**
   - 独立面板可以快速验证核心功能，不依赖 Material Editor 的私有 API。
   - Phase 3 再通过 `MaterialEditor` 模块扩展工具栏和节点联动。

2. **参数命名空间隔离**
   - 引擎已有 `FMaterialParameterInfo` 和 `EMaterialParameterType`，为避免头文件冲突和歧义，插件内部全部使用 `FMLPParameterInfo` / `EMLPParameterUsage` / `EMLPParameterType` 前缀。

3. **写操作为什么拆分成 `ApplyXxx` 和 `ApplyXxxInternal`？**
   - 批量操作（如 `AutoGroup`）需要在一次 `FScopedTransaction` 中完成，并只在最后调用一次 `PostEditChange` / `RefreshParameters`。
   - 单个参数修改（行内编辑）需要立即刷新并单独记录事务。

4. **状态分析为什么不直接依赖引擎的 `GetAllParameterInfo`？**
   - 引擎接口只返回参数名，无法判断该参数是否被实际连接到最终输出。
   - 通过反射遍历 `FExpressionInput` 可以更准确地判断 Used / Unused / Half-Used。

---

## 依赖

- Unreal Engine 4.26+
- 依赖模块：`Core`, `CoreUObject`, `Engine`, `InputCore`, `Slate`, `SlateCore`, `EditorStyle`, `UnrealEd`, `ToolMenus`, `Projects`, `RenderCore`, `RHI`, `MaterialEditor`, `EditorWidgets`, `PropertyEditor`, `ApplicationCore`, `LevelEditor`, `EditorSubsystem`, `AssetRegistry`, `DesktopPlatform`, `ContentBrowser`

---

## 许可证

MIT
