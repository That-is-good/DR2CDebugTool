# Death Road to Canada Debug Tool

A real‑time memory debugging and editing tool for the game *Death Road to Canada*, built with **Qt6** and C++.  
It reads the target process memory to display and modify character attributes, entity states, global resources, and weapon data on the fly – useful for game researchers, mod developers, or advanced players.

> **⚠️ Important Notice**  
> This tool is intended for learning, research, and legitimate debugging purposes only. Do not use it for any illicit activities.

---

## ✨ Features

- **Process Management**  
  Enumerate all system processes, filter by name or PID, and attach/detach with one click.

- **Character Panel**  
  - Show basic info (name, perk, trait, description) for all characters in the current party  
  - Real‑time editing of **13 core attributes** (morale, attitude, composure, charm, etc.) including base, temp, and bonus values  
  - Modify character resources (food, gas, medical, ammo, and 4 other types)  
  - Manage **3 weapon slots** (weapon ID, stack count, lock status)  
  - Adjust health, speed bonus, gender, pet flag, and two status‑flag fields (bit‑wise)

- **Entity Panel** (Graphical View)  
  - Filter entities by map area; displayed as icons on a grid layout  
  - Three interaction modes: **Select** (click/box select), **Zoom** (scroll wheel), **Pan** (drag canvas)  
  - In *Select* mode, **drag‑and‑drop** entities directly to new positions (writes back on release)  
  - Rich right‑click context menu:  
    - **Set as Target** – mark an entity as a reference for teleport/swap  
    - **Flags** – toggle nocollide, unseen, invisible, no_hit, nopick, glow (batch for multiple selections)  
    - **Edit Position / Velocity / Physics / Other** – open dialogs for precise numeric adjustment  
    - **Teleport to Target** – move selected entities to the target’s coordinates  
    - **Random Swap** – shuffle positions and areas among selected entities  
    - **Clone** – duplicate selected entities (if the type supports it)  
    - **Destroy** – remove entities from the game  
    - **Spawn** – create new entities: human, zombie, item, projectile, furniture, pickup, weapon, vehicle, special pickup  
    - **Set Render Layer** – change the camera’s rendering area

- **Mission (Global) Panel**  
  - Show current party members (automatically linked to character names)  
  - Edit global resources (8 types)  
  - Manage **15 storage weapon slots** (weapon ID and quantity)

- **Script Console**  
  Send custom script commands directly to the game (requires internal scripting support).

- **Auto Refresh**  
  Configurable refresh interval (default 500ms); editing fields temporarily pauses refresh to avoid interference.

- **Persistent Configuration**  
  All memory offsets, refresh rate, and language preferences are stored in `config.json` – easy to adjust when game versions change.

---

## 📦 Requirements

- **OS**: Windows (tested on 10/11)
- **Build Tools**:  
  - CMake 3.19 or higher  
  - C++17‑compatible compiler (MSVC 2019+ / MinGW‑w64 8.0+)
- **Qt Version**: **Qt 6.5** or later (required components: `Core`, `Widgets`, `LinguistTools`)
- **Optional**: Qt Creator (recommended for project management and CMake integration)

---

## 🔧 Building and Running

### 1. Get the Source
```bash
git clone https://github.com/yourusername/drtc-debug-tool.git
cd drtc-debug-tool
```

### 2. Using Qt Creator (Recommended)
- Open Qt Creator, select **“Open Project”**, and choose `CMakeLists.txt` in the project root.
- Select a kit with Qt 6.5+ (e.g., `Desktop Qt 6.5.0 MinGW 64-bit`).
- Click **“Build”** (or Ctrl+B). The executable will be placed in a subfolder under `build/`.
- After building, you can find `DR2CDebugTool.exe` in `build/Release/` or `build/`.

### 3. Deploy and Run
- Run `DR2CDebugTool.exe` directly (if Qt DLLs are missing, add Qt’s `bin` directory to your PATH or use `windeployqt`).
- On first launch, a `config.json` file will be created automatically (if not present).

---

## 📖 User Guide

### Launch and Attach to Process
1. Start the tool – the main window shows a list of running processes.  
2. Enter a filter (e.g., `prog`) or PID in the filter box and press Enter to narrow down.  
3. Select the game process from the dropdown (usually `prog.exe`).  
4. Click **“Attach”**. The status bar will confirm a successful attachment and show the module base address.  
5. The tool starts refreshing automatically; all UI controls become enabled.

### Memory Offset Settings
If a game update breaks data reading, click the **“Settings”** button to open the offset dialog:  
- Configure start offsets, structure sizes, and maximum counts for **Characters**, **Entities**, **Weapons**, and **Mission**.  
- Adjust the global refresh interval (in milliseconds).  
- Switch the UI language (choose “System” or any loaded translation; currently includes Chinese, English, and Japanese – you can add more). A restart is required for the language change to take effect.  
- Save – changes take effect immediately without restarting.

### Character Operations
- In the **“Character”** tab, select a character from the dropdown.  
- All editable fields (line edits, spin boxes, checkboxes) support direct modification; changes are written to memory on submission.  
- In the attribute table, the “Base / Temp / Bonus” values are editable (double‑click); the effective value is calculated automatically. Note that the “Temp” value is usually controlled by status flags and manual changes have little effect.  
- Click a weapon slot button to open the weapon selection dialog and pick from the list of loaded weapons.  
- The two status flags (属性1/属性2) are edited as integer bitmasks – think of them in binary.

### Entity Operations (Graphical View)
- In the **“Entity”** tab, use the area filter combobox to show entities from a specific map area (default: “All”).  
- Three interaction modes are available via radio buttons in the top‑right:  
  - **Select** (default): click or rubber‑band select entities. You can also **drag** any selected entity to move it – the new position is written back when you release the mouse.  
  - **Zoom**: scroll the mouse wheel to zoom in/out.  
  - **Pan**: drag the canvas to navigate around.  
- Right‑click on an entity (or on empty space) to open the context menu:  
  - If multiple entities are selected, most actions apply to all of them.  
  - **“Set as Target”** marks the clicked entity as a target (used for teleport/swap operations).  
  - **“Flags”** submenu: batch toggle special properties (nocollide, unseen, invisible, no_hit, nopick, glow). The displayed flag state is the logical AND result of all selected entities.  
  - **“Edit Position / Velocity / Physics / Other”** open dialogs for fine‑tuning numeric values – the dialog shows the current values of the first selected entity.  
  - **“Teleport to Target”** moves selected entities to the target’s coordinates.  
  - **“Random Swap”** randomly swaps positions and area IDs among the selected entities.  
  - **“Clone”** duplicates selected entities (if the type is clonable).  
  - **“Destroy”** permanently removes selected entities.  
  - **“Spawn”** submenu: create a new entity at the mouse position – choose from human, zombie, item, projectile, furniture, pickup, weapon, vehicle, or special pickup.  
    > **Note**: When spawning non‑character entities such as **item, projectile, furniture, pickup, weapon, vehicle, or special pickup**, you usually need to set a valid **Sprite ID** in the “Edit Other” dialog, otherwise the entity may not appear correctly in the game.  
  - **“Set Render Layer”**: set the current area as the camera’s render target. For example, after teleporting a player‑controlled character to a different area, you must call this function to update the render layer; otherwise the camera may get stuck in the upper‑left corner.  
- While dragging entities, the auto‑refresh timer is temporarily paused to prevent scene rebuilds; it resumes after the drag is finished.

### Mission (Global) Operations
- The **“Mission”** tab shows party members, global resources, and storage weapons.  
- Member names are automatically linked to the character list and are read‑only.  
- Resource values are editable directly in the table.  
- Storage weapon slots are arranged as a grid of buttons + spin boxes – click a button to select a weapon ID, and adjust the quantity with the spin box.

### Script Console
- In the command input box at the bottom of the “Mission” tab, type a game script command (e.g., `spawn`) and press Enter.  
- Since the game supports UTF‑8, commands may include Unicode characters (which the built‑in game console cannot input).  
- The status bar will show whether the command was executed successfully.

---

## 🗂 Project Structure

| File / Directory | Description |
|------------------|-------------|
| `mainwindow.cpp/.h/.ui` | Main window UI and logic – all signals/slots, data refresh, and modification handling |
| `Setting/addrsetting.cpp/.h/.ui` | Offset settings dialog – manages offsets and configuration I/O |
| `Memory/memorymanager.cpp/.h` | Low‑level process enumeration, attach/detach, memory read/write, entity allocation/free |
| `Memory/gamedatareader.cpp/.h` | Reads game data using offsets and converts to high‑level structures (character/entity/mission) |
| `Delegates/spinboxdelegate.cpp/.h` | QSpinBox editor delegate for table cells |
| `WeaponDialog/weapondialog.cpp/.h` | Weapon selection dialog showing loaded weapon names |
| `Languages/` | Qt translation files (`.ts`, supporting English, Chinese, Japanese) |
| `Struct/` | Reverse‑engineered game data structure definitions (`thing`, `character`, `mission_state`, etc.) |
| `config.json` | Auto‑generated/read configuration file storing offsets and user preferences |
| `CMakeLists.txt` | Main CMake project configuration file |

---

## ⚠️ Important Notes

- **Game Version Compatibility**: Offsets are version‑specific. If data becomes invalid, use the **Settings** dialog to update the offsets (you can re‑locate them with tools like Cheat Engine).  
- **Stability**: Memory editing carries a risk of crashes. It is advisable to back up your save files. If the game crashes, simply restart it – the tool does not need to be restarted.  
- **Editing Conflicts**: The auto‑refresh pauses while you are editing a field to avoid overwriting your input. However, do not use another memory editor simultaneously to prevent race conditions.  
- **Read‑Only Mode**: If you only want to view data, avoid modifying fields; the tool writes any UI changes to memory immediately.  
- **Language Support**: The UI language is dynamically loaded from the `translations` directory. It supports the system language and all available translation files (currently includes Chinese, English, and Japanese). You can freely switch in the settings and even add custom translations.  
- **Destroying Entities vs. Deleting Characters**: Executing **“Destroy”** in the entity panel only removes the game‑world object (e.g., character model, item model) – it **does not** free the character slot or character data associated with that entity.

---

## 🤝 Acknowledgements

- Thanks to the game developers Rocketcat Games and Madgarden for creating this wonderful game.  
- This project is based on reverse‑engineering research; structure definitions come from community resources and personal analysis.

---

## 📜 License

This project is for personal learning and research purposes only. Commercial use or any form of infringement is strictly prohibited.  
You are allowed to freely distribute unmodified official releases. If you distribute a modified version, you assume all responsibility, and the original author is not liable.