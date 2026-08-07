# Death Road to Canada Debug Tool

This project is a debug / memory editing tool for the game *Death Road to Canada*, built with **Qt6** and C++. It reads the game process memory to display and modify character attributes, entity states, party resources, and item data in real time, making it useful for game researchers, mod developers, or advanced players.

> **⚠️ Important Notice**  
> This tool is intended for learning, research, and legitimate debugging purposes only.
---

## ✨ Features

- **Process Management**  
  List all system processes, filter by name or PID, and attach/detach to the target process with one click.

- **Character Panel**  
  - Display basic info (name, perk, trait, description, etc.) for all characters in the current party  
  - View and modify **13 core attributes** (morale, attitude, composure, charm, etc.) as well as temporary and bonus values  
  - Edit character resources (food, gas, medical, ammunition, and 5 other types)  
  - Manage **3 weapon slots** (weapon ID, stack count, lock status)  
  - Modify health, speed bonus, gender, pet flag, and state flags

- **Entity Panel**  
  - Filter entity list by type (human/zombie/item/projectile/etc.) and by map area  
  - View detailed entity information: position, velocity, physics parameters, AI state, sprite ID, hit points, etc.  
  - Real‑time modification of coordinates, velocity, mass, friction, collision/visibility flags, etc.  
  - Entity operations: **Set Target** (for teleport/swap), **Teleport to Target**, **Swap Positions**, **Destroy Entity**  
  - **Spawn Entity** (supports humans, zombies, various items/weapons/furniture, etc.)

- **Mission Panel**  
  - Display current party members (automatically linked to character names)  
  - Modify global resources (food, gas, medical, etc.)  
  - Manage **15 storage weapon slots** (weapon ID and quantity)

- **Script Console**  
  Send custom script commands to the game (requires the game’s internal scripting support).

- **Auto Refresh**  
  Configurable refresh interval (default 500ms) with automatic data synchronisation.

- **Persistent Configuration**  
  All memory offsets, refresh rate, and language preferences are saved in `config.json`, allowing easy switching between game versions.

---

## 📦 Requirements

- **Operating System**: Windows
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
- Open Qt Creator and select **“Open Project”**, then choose `CMakeLists.txt` in the project root.
- Select a kit with Qt 6.5+ (e.g., `Desktop Qt 6.5.0 MinGW 64-bit`).
- Click **“Build”** (or Ctrl+B). The executable will be placed in a subfolder under `build/`.

After building, you can find `DR2CDebugTool.exe` in `build/Release/` or `build/`.

### 3. Run and Deploy
- Run the executable directly (you may need to add Qt DLLs to your PATH, or use `windeployqt` to deploy).

---

## 📖 User Guide

### Launch and Attach to Process
1. Start the tool – the main window shows a list of processes (you can filter by name or PID).
2. Select the game process from the dropdown (usually `prog.exe`).
3. Click **“Attach”**. The status bar will confirm a successful attachment and show the module base address.
4. The tool starts refreshing data automatically, and the UI controls become enabled.

### Memory Offset Settings
If a game update breaks data reading, click the **“Settings”** button to open the offset dialog:
- Configure start offsets, structure sizes, and maximum counts for **Characters**, **Entities**, **Weapons**, and **Mission**.
- Adjust the global refresh interval (in milliseconds).
- Choose the UI language (System, English, 中文, 日本語), need restart.
- Save – changes take effect immediately without restarting.

### Character Operations
- In the **“Character”** tab, select a character from the drop‑down list.
- All editable fields support direct modification (press Enter or lose focus to submit).
- In the attribute table, the “Base / Temp / Bonus” values are editable; the effective value is computed automatically.
- Click a weapon slot button to open a weapon selection dialog and choose from the list of loaded weapons.

### Entity Operations
- In the **“Entity”** tab, filter the list by type/area and click on an entity to select it.
- The right panel shows detailed data that can be modified in real time.
- **Set Target**: mark the currently selected entity as the target (button highlights). You can then teleport or swap positions with it.
- **Spawn Entity**: choose a type from the spawn combo box and click “Spawn” – the new entity appears in the current map (with a random position).

### Mission Operations
- The **“Mission”** tab displays current party members, global resources, and storage weapons.
- Member names are automatically matched to character names and are read‑only.
- Resource values are directly editable, and storage weapon slots also use a button to select weapon IDs, with a spin box for quantity.

### Script Console
- Type a game script command (e.g., `spawn_thing`) in the bottom input field and press Enter.
- The status bar will show whether the command was executed successfully.

---

## 🗂 Project Structure

| File / Directory | Description |
|------------------|-------------|
| `mainwindow.cpp/.h/.ui` | Main window UI and logic – all signals/slots, data refresh, and modifications |
| `Setting/addrsetting.cpp/.h/.ui` | Address setting dialog – manages offsets and configuration I/O |
| `Memory/memorymanager.cpp/.h` | Low‑level process enumeration, attach/detach, memory read/write, entity allocation/free |
| `Memory/gamedatareader.cpp/.h` | Reads game data using offsets and converts to high‑level structures (character/entity/mission) |
| `Delegates/spinboxdelegate.cpp/.h` | QSpinBox editor delegate for table cells |
| `WeaponDialog/weapondialog.cpp/.h` | Weapon selection dialog |
| `Languages/` | Qt translation files (`.ts`, supporting English, Chinese, Japanese) |
| `Struct` | Reverse‑engineered game data structure definitions (`thing`, `character`, `weapon`, `mission_state`, etc.) |
| `config.json` | Auto‑generated/read configuration file storing offsets and user preferences |
| `CMakeLists.txt` | Main CMake project configuration file |

---

## ⚠️ Important Notes

- **Game Version Compatibility**: Offsets may change with game updates. If data becomes invalid, use the **Settings** dialog to update the offsets (you can re‑locate them with tools like Cheat Engine).
- **Stability**: Memory editing carries a risk of crashes. It is advisable to back up your save files. If the game crashes, simply restart it.
- **Read‑Only Mode**: If you only want to view data, avoid modifying fields; however, the tool writes any UI changes to memory immediately, so be careful.
- **Language Support**: The interface supports Chinese, English, and Japanese.

---

## 🤝 Acknowledgements

- Thanks to the game developers Rocketcat Games and Madgarden for creating this wonderful game.
- This project is based on reverse‑engineering research; the structure definitions come from community resources and personal analysis.

---

## 📜 License

This project is intended for personal learning and research purposes only.

---

For questions or suggestions, feel free to open an Issue or submit a Pull Request.