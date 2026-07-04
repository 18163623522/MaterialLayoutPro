# Material Layout Pro

> Unreal Engine 4.26 材质参数管理插件，参考 Core Grip Tech 的 *Material Layout Pro* 设计思路，用于在编辑器中可视化、分组、批量编辑材质参数。

---

## 当前功能

- **Nomad Tab 面板**：Window → Material Layout Pro 打开全局可停靠面板。
- **参数扫描**：自动读取当前选中的 `UMaterial` 或 `UMaterialInstance` 的参数节点。
- **分组折叠展示**：按 `Group` 字段对参数分组，支持展开/折叠、按排序优先级排序。
- **类型 / 状态徽章**：
  - 类型：Scalar / Vector / Texture / Static Switch
  - 状态：Used / Unused / Half-Used / Indirect / Unknown
- **多选操作**：支持 Ctrl 单选、Shift 范围选择。
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

## 后续计划（Roadmap）

### Phase 1 - 面板完善（当前）
- [x] 分组折叠 UI
- [x] 类型 / 状态徽章
- [x] 多选与批量改 Group
- [x] 未使用参数归档/删除
- [x] 批量重命名对话框
- [x] 导出 CSV
- [ ] 可配置的自动分组规则（INI / 设置对话框）
- [ ] 右键菜单（改 Group / 改 Priority / 复制名称）
- [ ] 参数搜索 / 过滤
- [ ] 排序自定义（按名称、类型、优先级）

### Phase 2 - 材质实例支持
- [ ] 读取并展示 `UMaterialInstance` 的覆盖参数
- [ ] 区分父材质默认值与实例覆盖值
- [ ] 在实例上直接修改参数分组

### Phase 3 - 与材质编辑器集成
- [ ] 在 Material Editor 工具栏添加入口按钮
- [ ] 选中面板中的参数时在 Material Editor 中高亮对应节点
- [ ] 双向选择：在 Material Editor 选中节点时面板同步选中
- [ ] 将节点拖拽到 Comment 区域自动按 Comment 分组

### Phase 4 - 高级功能
- [ ] 批量改 Sort Priority（带优先级偏移）
- [ ] 参数使用链路可视化（上游 / 下游）
- [ ] 一键清理 Deprecated 分组
- [ ] 参数重命名时同步更新 Material Instance 覆盖
- [ ] 导入 CSV 反向修改参数 Group / Priority

---

## 依赖

- Unreal Engine 4.26+
- 依赖模块：`Core`, `CoreUObject`, `Engine`, `InputCore`, `Slate`, `SlateCore`, `EditorStyle`, `UnrealEd`, `ToolMenus`, `Projects`, `RenderCore`, `RHI`, `MaterialEditor`, `EditorWidgets`, `PropertyEditor`, `ApplicationCore`, `LevelEditor`, `EditorSubsystem`, `AssetRegistry`, `DesktopPlatform`, `ContentBrowser`

---

## 许可证

MIT
