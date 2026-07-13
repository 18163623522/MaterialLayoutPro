# Material Layout Pro

> Unreal Engine 4.26 材质参数管理插件，参考 Core Grip Tech 的 *Material Layout Pro* 设计思路，用于在编辑器中可视化、分组、批量编辑材质参数。

## 参考链接

- 原版 Fab 商品页：https://www.fab.com/listings/723825e2-5792-400d-a9be-28423d1330b
- 原版文档（GitBook）：https://core-grip-tech.gitbook.io/core-grip-tech-docs

---

## 当前功能

- **材质编辑器嵌入面板**：材质编辑器工具栏按钮打开可停靠"参数布局"侧边栏；材质实例编辑器自动显示"实例分组"面板。
- **Nomad Tab 面板**：`Window -> Material Layout Pro` 打开全局可停靠面板。
- **参数扫描**：自动读取当前选中的 `UMaterial` 或 `UMaterialInstance` 的参数节点。
- **分组折叠展示**：按 `Group` 字段对参数分组，支持展开/折叠（▶/▼ + 全折叠/全展开按钮）、按排序优先级排序。
- **搜索/过滤**：实例面板按参数名实时过滤 + 匹配计数状态栏；搜索时自动展开折叠的组。
- **类型 / 状态徽章**：
  - 类型：Scalar / Vector / Texture / Static Switch
  - 状态：Used / Unused / Half-Used / Indirect / Unknown
- **参数去重检测**：同名参数标 ⚠ 警告徽章（两面板），提示用户重命名。
- **多选操作**：支持 `Ctrl` 单选、`Shift` 范围选择。
- **批量改 Group**：输入 Group 名称后一键应用到所有选中参数。
- **值编辑**：标量直接输入；Vector 参数可直接输入 R/G/B/A 数值或点色块开颜色选择器；纹理内容浏览器选；静态开关勾选。
- **实例覆盖管理**：实例面板勾选/切换单个参数覆盖；全部启用/重置覆盖按钮；改值自动启用覆盖。
- **拖拽精确插入**：拖 `::` 手柄到任意行之间精确插入（含跨组），蓝线落点指示器。
- **右键菜单**：两面板参数行右键弹菜单（复制名/定位节点/移动到分组子菜单/切换覆盖）。
- **节点联动**：单击参数高亮材质编辑器图中对应节点；双击跳转定位。
- **定位资产**：工具栏按钮在内容浏览器中选中并定位当前材质。
- **自动分组**：根据参数名前缀自动归类（规则可在 Project Settings 配置；工具栏"分组规则"按钮直达）。
- **按注释分组**：根据材质编辑器中的 Comment 框区域将内部参数归到同名 Group。
- **重置排序号**：一键把所有参数 SortPriority 按组内顺序重排为 0,1,2,...。
- **归档未使用参数**：一键将 Unused 参数移到 `Deprecated` 分组。
- **删除未使用参数**：一键从材质表达式列表中移除 Unused 参数节点。
- **批量重命名对话框**：Find & Replace / 正则预览，对选中参数批量改名。
- **导出 CSV**：将参数列表导出为 `Name,Type,Group,SortPriority,Usage,Value` 格式。
- **导入 CSV**：预览对话框（匹配/未知/将变化计数 + 彩色行预览）确认后写回 Group/SortPriority/Value；引号稳健解析。
- **分组预设模板**：保存/应用 Name->Group 映射为 .json 模板，跨材质复用。
- **工具栏"更多"菜单**：低频按钮收进下拉菜单，保持窗口紧凑。
- **Undo/Redo 支持**：所有写操作都通过 `FScopedTransaction` 包裹 + `RF_Transactional` 标志，Ctrl+Z 可撤回。

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
    │   ├── MaterialLayoutProSettings.h       # 项目设置(自动分组规则等)
    │   ├── Model/
    │   │   ├── MaterialParameterInfo.h       # 参数数据模型
    │   │   └── MaterialParameterScanner.h    # 扫描器接口(Private/)
    │   └── Widgets/
    │       ├── SMaterialLayoutProPanel.h     # 材质面板
    │       ├── SMaterialParameterRow.h       # 材质参数行
    │       ├── SMaterialBulkRenameDialog.h   # 批量重命名弹窗
    │       ├── SMaterialCSVImportDialog.h    # CSV 导入预览对话框
    │       ├── SMaterialInstanceGroupPanel.h # 实例分组面板 + 拖拽/折叠
    │       └── ...
    └── Private/
        ├── MaterialLayoutPro.cpp             # 模块:工具栏/TAB/事件绑定
        ├── MaterialLayoutProCommands.cpp
        ├── MaterialLayoutProStyle.cpp
        ├── MaterialLayoutProSettings.cpp     # 设置默认值
        ├── MaterialInstanceGroupData.cpp     # 实例分组持久化(AssetUserData)
        ├── Model/
        │   ├── MaterialParameterScanner.cpp  # 含去重检测
        │   ├── MaterialParameterUsageAnalyzer.cpp
        │   └── MaterialLayoutViewModel.cpp   # 材质 VM(PushToExpression)
        └── Widgets/
            ├── SMaterialLayoutProPanel.cpp   # 材质面板(搜索/折叠/右键/模板/CSV)
            ├── SMaterialParameterRow.cpp      # 材质参数行(Vector RGBA 编辑等)
            ├── SMaterialBulkRenameDialog.cpp
            ├── SMaterialCSVImportDialog.cpp   # CSV 预览 + Value 写回
            └── SMaterialInstanceGroupPanel.cpp # 实例面板(搜索/折叠/拖拽/右键/覆盖)
```
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
| P0 | 分组折叠 UI | ✅ | 按 Group 折叠，显示参数数量；支持全折叠/全展开按钮 |
| P0 | 类型 / 状态徽章 | ✅ | Scalar/Vector/Texture/StaticSwitch + Used/Unused/HalfUsed/Indirect |
| P0 | 多选操作 | ✅ | Ctrl 单选、Shift 范围选择 |
| P0 | 批量改 Group | ✅ | 输入框 + Set 按钮应用到选中项 |
| P0 | 未使用参数归档 / 删除 | ✅ | 移动到 Deprecated / 直接删除节点 |
| P0 | 自动分组 | ✅ | 前缀规则（可配置） |
| P0 | 按注释分组 | ✅ | 根据 Comment 框区域分组 |
| P1 | 批量重命名对话框 | ✅ | Find/Replace + 正则预览 |
| P1 | 导出 CSV | ✅ | 导出 Name/Type/Group/Priority/Usage/Value |
| P1 | 可配置的自动分组规则 | ✅ | UDeveloperSettings (Project Settings)；工具栏"分组规则"按钮直达 |
| P1 | 右键菜单 | ✅ | 复制名/定位节点/移动到分组(子菜单)/切换覆盖 |
| P2 | 搜索 / 过滤 | ✅ | 实例面板按名称子串过滤 + 匹配计数状态栏 |
| P2 | 排序自定义 | ✅ | 工具栏下拉切换:按优先级/名称/类型/使用状态排序(仅视图) |
| P2 | 节点参数提示 | ⏳ | 鼠标悬停显示 Value、Expression 路径等 |

### Phase 2 — 材质实例支持

目标：不仅支持 `UMaterial`，也支持 `UMaterialInstance` 及其继承链。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P0 | 读取实例参数 | ✅ | 识别父材质参数与实例覆盖参数 |
| P0 | 区分默认值与覆盖值 | ✅ | 复选框/● 标识覆盖状态，可切换 |
| P1 | 在实例上修改 Group | ✅ | 独立分组持久化（AssetUserData，不动父材质）；支持拖拽精确插入 + 蓝线指示器 |
| P1 | 显示实例覆盖值 | ✅ | 参数行展示实例实际使用的值 |

### Phase 3 — 与 Material Editor 集成

目标：将面板真正嵌入到 Material Editor 中，实现双向联动。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P0 | Material Editor 工具栏入口 | ✅ | 材质/实例编辑器工具栏按钮 + 可停靠 Tab；实例编辑器自动显示面板 |
| P0 | 面板中选中高亮节点 | ✅ | 单击参数 -> 图中对应节点高亮（AddToSelection） |
| P0 | 节点选中同步面板 | ⏳ | 在 Material Editor 中选中参数节点时，面板同步选中 |
| P1 | 拖拽到 Comment 自动分组 | ✅ | 右键菜单"移动到分组"子菜单（替代拖拽到 Comment） |
| P1 | 双击定位节点 | ✅ | 双击参数行跳转到 Material Editor 对应节点位置 |

### Phase 4 — 高级功能

目标：提供参数整理、清理、迁移等高级批量能力。

| 优先级 | 功能点 | 状态 | 说明 |
|--------|--------|------|------|
| P1 | 批量改 Sort Priority | ✅ | "重置排序号"按钮：按组内顺序重排为 0,1,2,... |
| P1 | 使用链路可视化 | ✅ | 右键"查看使用链路"弹窗显示下游消费者+上游输入(类名+坐标) |
| P1 | 一键清理 Deprecated | ✅ | "归档未用"+"删除未用"按钮 |
| P2 | 重命名同步 Material Instance | ⏳ | 参数改名时同步更新实例覆盖记录 |
| P2 | 导入 CSV 反向修改 | ✅ | 含 Group/SortPriority/Value 写回 + 预览对话框（引号稳健解析） |
| P2 | 参数去重检测 | ✅ | 同名参数标 ⚠ 警告徽章（两面板） |
| P3 | 预设模板 | ✅ | 保存/应用 Name→Group 映射为 .json 模板（跨材质复用） |
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
- 依赖模块：`Core`, `CoreUObject`, `Engine`, `InputCore`, `Slate`, `SlateCore`, `EditorStyle`, `UnrealEd`, `ToolMenus`, `Projects`, `RenderCore`, `RHI`, `MaterialEditor`, `EditorWidgets`, `PropertyEditor`, `ApplicationCore`, `LevelEditor`, `EditorSubsystem`, `AssetRegistry`, `DesktopPlatform`, `ContentBrowser`, `DeveloperSettings`, `AppFramework`, `Json`

---

## 许可证

MIT
