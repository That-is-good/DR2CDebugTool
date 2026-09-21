# DR2CInternalOverlay

基于 **ImGui + OpenGL3 + Win32 + MinHook** 的 32 位 Windows 游戏内部覆盖层 / 调试工具。  
通过 DLL 注入目标进程后，可在游戏内显示调试面板，并对实体、角色、物品、车辆等数据进行实时查看与修改。

> 本项目中的游戏函数地址与全局变量地址均为硬编码 RVA，仅适配特定游戏版本。版本不匹配时可能导致崩溃或修改无效。

---

## 功能特性

- ImGui 内部调试面板，`Insert` 键显示 / 隐藏
- 实体悬停检测与屏幕高亮
- 右键点击实体打开编辑弹窗
- 支持拖动实体位置
- 实体字段编辑：
  - 位置、速度、物理
  - 地图、血量、精灵、AI 状态
  - No collision / No pickup / Unseen / Invisible / No hit / Glow
- 角色编辑（`type == 1` 且非僵尸）：
  - 名称、生命、速度加成
  - 13 项属性：Base / Bonus / Total
  - 8 种资源
  - 3 个武器槽：武器选择、Stack、Lock
- 物品编辑（`type == 3 && subtype == 1`）：
  - Amount、Loot 类型
- 车辆编辑（`type == 3 && subtype == 3`）：
  - Chassis、Engine、Armour、Speed、Repair、MPG 等
- 实体克隆与销毁
- 游戏帧率修改
- 当前地图层读写
- 日志输出到 DLL 同目录同名 `.log` 文件

---

## 构建要求

- Windows
- CMake >= 3.19
- **32 位 MinGW-w64 工具链**
  - 例如 `i686-w64-mingw32-gcc` / `i686-w64-mingw32-g++`
  - 或 MSYS2 的 `mingw32` 环境
- C++17
- 依赖库：
  - **ImGui 1.92.9b**，放置于 `imgui-1.92.9b/`
  - **MinHook**，放置于 `MinHook/`
    - 需要 `MinHook/include/MinHook.h`
    - 需要 32 位静态库 `MinHook/lib/MinHook.a`
- 系统链接库：
  - `opengl32`
  - `gdi32`
  - `user32`
  - `dwmapi`

> CMake 会检查 `CMAKE_SIZEOF_VOID_P`，如果不是 4 字节会直接报错：  
> `DR2CInternalOverlay must be built with a 32-bit MinGW toolchain`

---

## 目录结构

```text
.
├── CMakeLists.txt
├── dllmain.cpp
├── imgui_ui.cpp
├── imgui_ui.h
├── imgui-1.92.9b/    -- 自行下载
│   ├── imgui.cpp
│   ├── imgui_draw.cpp
│   ├── imgui_tables.cpp
│   ├── imgui_widgets.cpp
│   └── backends/
│       ├── imgui_impl_win32.cpp
│       └── imgui_impl_opengl3.cpp
└── MinHook/
    ├── include/
    │   └── MinHook.h
    └── lib/
        └── MinHook.a
```

---

## 构建方法

### 1. 准备依赖

确保以下目录存在：

```text
imgui-1.92.9b/
MinHook/
```

其中 `MinHook/lib/MinHook.a` 必须是 32 位版本。

### 2. 生成并编译

```bash
mkdir build
cd build

cmake -G "MinGW Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
  ..

cmake --build . --config Release
```

如果使用 MSYS2 MinGW 32-bit 环境，也可以：

```bash
mkdir build
cd build

cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make -j
```

构建成功后输出：

```text
build/bin/DR2CInternalOverlay.dll
```

---

## 使用方法

1. 启动 32 位目标游戏。
2. 使用任意 DLL 注入器将 `DR2CInternalOverlay.dll` 注入游戏进程。
3. 按 `Insert` 切换调试面板显示状态。
4. 将鼠标移动到游戏内实体上，可看到高亮框。
5. 右键点击实体，打开 `Entity Edit` 编辑弹窗。
6. 勾选 `Drag entity` 后，可按住左键拖动实体。
7. 可在调试面板中修改：
   - Pick radius
   - Game FrameRate
   - Current Map
   - Log to file
8. 勾选 `Log to file` 后，日志会写入 DLL 同目录下的同名 `.log` 文件。

---

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `Insert` | 显示 / 隐藏调试面板 |
| 鼠标右键 | 编辑悬停实体 |
| 鼠标左键 | 勾选 `Drag entity` 后拖动实体 |

---

## 调试面板说明

- `Drag entity`：允许左键拖动实体。
- `Log to file`：将调试信息输出到日志文件。
- `Pick radius (world)`：实体拾取半径。
- `Game FrameRate`：读取 / 修改游戏帧率。
- `Current Map`：读取 / 修改当前地图层。
- 调试信息：
  - moduleBase
  - g_ScreenScale
  - camera
  - mouse
  - hover / edit / drag 槽位

---

## 实体编辑弹窗

右键实体后打开，主要包含：

- `Entity`
  - Position
  - Map
  - Velocity
  - Physics
  - Hitpoints
  - Sprite
  - AI state
  - No collision / No pickup / Unseen / Invisible / No hit / Glow
- `Character`
  - 仅当 `type == 1` 且非僵尸时显示
  - 可编辑名称、生命、速度加成、属性、资源、武器
- `Item`
  - 仅当 `type == 3 && subtype == 1` 时显示
  - 可编辑 Amount、Loot
- `Vehicle`
  - 仅当 `type == 3 && subtype == 3` 时显示
  - 可编辑底盘、引擎、装甲、速度、维修、MPG 等
- `Clone`
  - 复制当前实体
- `Destroy`
  - 销毁当前实体
- `Close`
  - 关闭弹窗

---

## 导出接口

`imgui_ui.cpp` 对外提供以下接口，供 `dllmain.cpp` 或钩子代码调用：

```cpp
bool InitializeInternalUi(HWND window, HMODULE module);
void ShutdownInternalUi();
void RenderInternalUi();
LRESULT HandleInternalWindowMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void CheckPanelHotkey();

void QueueEntityWrite(unsigned int slot, const Dr2cEntityView &entity, unsigned int mask);
void ApplyPendingEntityWrite();
void ClearPendingEntityWrite();
```

典型集成方式：

- 在 OpenGL 渲染循环中调用 `RenderInternalUi()`
- 在窗口消息过程中调用 `HandleInternalWindowMessage(...)`
- 在 DLL 初始化时调用 `InitializeInternalUi(...)`
- 在 DLL 卸载时调用 `ShutdownInternalUi()`

---

## 注意事项

- 必须使用 32 位 MinGW 构建，否则 CMake 会报错。
- 游戏内地址为硬编码 RVA，仅适配特定版本。
- 目标游戏需要以 OpenGL 渲染。
- `MinHook.a` 必须为 32 位静态库。
- 建议在修改前备份存档。
- 本项目仅供学习、调试和单机修改使用。
- 使用风险自负。

---

## 免责声明

本项目仅供技术学习与研究。  
作者不对任何因使用本工具导致的存档损坏、游戏崩溃或其他损失负责。

---

## 许可证

未指定许可证。  
如需开源发布，请自行补充 `LICENSE` 文件。