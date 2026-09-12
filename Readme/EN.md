# Death Road to Canada Debug Tool

A game memory debugging/modification tool for *Death Road to Canada*, built with **Qt6** and C++. It reads the target process memory to display and modify character attributes, entity states, global resources, and weapon data in real time. It is suitable for game research, mod development, or advanced player analysis.

> **⚠️ Note**  
> This tool is intended for learning, research, and legitimate debugging purposes only. Do not use it for any unauthorized purposes.

---

## ✨ Features

- **Process Management**  
  Enumerate all system processes, filter by name or PID, and attach with one click. Automatically detaches when the target process exits.

- **Character Panel**  
  - Displays basic information for all characters in the current party (name, perk, trait, description)  
  - The character drop-down marks current mission party members with **★**  
  - Real-time editing of **13 core stats** (morale, attitude, composure, charisma, etc.) with base/temporary/bonus values, plus "known" checkboxes  
  - Modify character resources (None, Food, Gas, Medical, Pistol, Rifle, Shotgun, Junk — 8 types)  
  - Manage **3 weapon slots** (weapon ID, quantity, locked state)  
  - Adjust HP, speed bonus, gender, pet flags, and status flags

- **Entity Panel** (visual graphics view)  
  - Filter entities by area and display them as icons on a map grid  
  - Supports **Select mode** (click/rubber-band selection), **Resize/Zoom mode** (wheel zoom), and **Move mode** (drag canvas)  
  - In Select mode, entities can be **dragged directly** to a new position (coordinates are written back to memory on release)  
  - Right-click context menu provides:  
    - **Set as Center** (marks a target entity for later teleport/swap; the center entity is shown with a yellow circle marker)  
    - **Flags** (No Collision, Invisible/Unseen, Not Drawn, No Hit, No Pickup, Glow)  
    - **Edit Position / Velocity / Physics / Other** (numeric adjustment dialogs)  
    - **Teleport to Center** (move selected entities to the center entity's coordinates)  
    - **Random Swap** (randomly swap positions and areas among selected entities)  
    - **Clone** (duplicate selected entities)  
    - **Destroy** (remove entities)  
    - **Set as Player Entity** (set the selected entity as the current player entity)  
    - **New** (spawn Human, Zombie, Item, Projectile, Furniture, Pickup, Weapon, Vehicle, Special Pickup)  
    - **Set Render Layer** (set the camera area)  
  - The area drop-down dynamically shows areas that actually exist

- **Global (Mission) Panel**  
  - Modify global resources (8 types)  
  - Manage **15 storage weapon slots** (weapon ID and quantity)  
  - Script console

- **Script Console**  
  Send custom script commands to the game (requires support from the game's script system).

- **Scheduled Refresh**  
  Customizable refresh interval (default 500 ms); data is synchronized automatically. Refresh is paused while editing to avoid interfering with input.

- **Configuration Persistence**  
  All memory offsets, refresh frequency, language preferences, etc. are saved in `config.json`, making it easy to adjust quickly after game updates.

---

## 📦 Requirements

- **OS**: Windows (tested on Win10/11)
- **Build tools**:  
  - CMake 3.19 or later  
  - A C++17-compatible compiler (MSVC 2019+ / MinGW-w64 8.0+)
- **Qt version**: **Qt 6.5** or later (required components: `Core`, `Widgets`, `LinguistTools`)
- **Optional**: Qt Creator (recommended for project management and CMake integration)

---

## 🔧 Build and Run

### 1. Get the source
```bash
git clone https://github.com/yourusername/drtc-debug-tool.git
cd drtc-debug-tool
```

### 2. Using Qt Creator (recommended)
- Open Qt Creator, choose **"Open Project"**, and select `CMakeLists.txt`.
- Select a Qt 6.5+ kit (e.g. `Desktop Qt 6.5.0 MinGW 64-bit`).
- Click **"Build"** (or press Ctrl+B). The generated executable will be under a subdirectory of `build/`.
- After building, you can find `DR2CDebugTool.exe` under `build/Release/` or `build/`.

### 3. Deploy and run
- Run `DR2CDebugTool.exe` directly. If Qt dynamic libraries are missing, add Qt's `bin` directory to PATH or use `windeployqt`.
- On first run, `config.json` is generated automatically if it does not exist.

---

## 📖 Usage Guide

### Launch and Attach to Process
1. Start the tool. The process list is shown at the top of the main window.  
2. Enter a process name (e.g. `prog`) or PID in the filter box and press Enter to filter.  
3. Select the game process (usually `prog.exe`) from the drop-down list.  
4. Click **"Attach"**. The status bar shows the attach result and module base address.  
5. The tool begins automatic data refresh and enables the UI controls.

### Configure Memory Offsets
If a game update causes data reads to fail, click the **"Settings"** button to open the offset dialog:  
- Configure the start offset, structure size, and maximum count for **Characters**, **Entities**, **Weapons**, and **Global** separately.  
- Adjust the global refresh interval (milliseconds).  
- Switch the UI language (choose "System" or any loaded translation file; currently includes Chinese, English, and Japanese, and can be extended). A restart is required for the language change.
- Changes take effect immediately after saving; no restart is required for offsets.

### Character Operations
- In the **"Character"** tab, select a character from the drop-down list.  
- All editable fields (text boxes, spin boxes, check boxes) can be modified directly and are written to memory automatically on commit.  
- The "Base / Temporary / Bonus" columns in the stats table can be edited by double-clicking; the effective value is calculated automatically. Note that "Temporary" values are usually controlled by status flags, so manually changing them is often not meaningful.  
- The "Known" column can be checked to change whether a character stat is known.  
- Clicking a weapon slot button opens the weapon selection dialog, where you can choose from the loaded weapon list.  
- When modifying status flags (Status 1 / Status 2), it is recommended to understand their meaning bit by bit.

### Entity Operations (Visual View)
- In the **"Entity"** tab, first filter the area of interest via the "Area" drop-down (default is "All").  
- Three interaction modes are available at the top right of the view:  
  - **Select** (default): click or rubber-band select entities. After selection, use the right-click menu or drag them directly.  
  - **Resize/Zoom**: use the mouse wheel to zoom the view; useful for inspecting the overall layout.  
  - **Move**: drag to pan the canvas; convenient for browsing large areas.  
- Right-click any entity (or empty space) to open the context menu:  
  - If multiple entities are selected, most operations apply to all selected items.  
  - **"Set as Center"**: marks an entity as the target (used as the reference for later teleport/swap). The center entity is shown with a yellow circle marker.  
  - **"Flags"** submenu: batch enable/disable special properties (No Collision, Invisible, etc.). The displayed flag state is the logical AND of all selected entities.  
  - **"Edit Position / Velocity / Physics / Other"**: opens a dialog for precise numeric adjustment. The dialog shows the current values of the first selected entity.  
  - **"Teleport to Center"**: moves selected entities to the center entity's coordinates.  
  - **"Random Swap"**: randomly swaps positions (including areas) among selected entities.  
  - **"Clone"**: duplicates selected entities at their current positions (if the type supports it).  
  - **"Destroy"**: completely removes entities.  
  - **"Set as Player Entity"**: sets the selected entity as the current player entity.  
  - **"New"** submenu: spawns a new entity at the mouse position (type selectable).  
    > **Note**: When spawning non-character entities such as **Items, Projectiles, Furniture, Pickups, Weapons, Vehicles, Special Pickups**, you usually need to set a valid **Sprite ID** as well; otherwise the entity will not display correctly in the game. Use the "Edit Other" dialog to assign a valid sprite ID to the newly spawned entity.  
  - **"Set Render Layer"**: sets the current area as the camera's render target. For example, after teleporting the player-controlled character to another area, use this function to set the new area as the render layer; otherwise the camera may stay stuck in the top-left corner.  
- While dragging entities, automatic refresh is paused. On release, the new coordinates are written back and refresh resumes. This also applies to multi-selection dragging.

### Global (Mission) Operations
- The **"Global"** tab shows global resources and storage weapons.  
- The character panel's drop-down marks current party members with **★**; member names are automatically synchronized from character data.  
- Resource tables can be edited directly. Click the table after editing to trigger the write.  
- Storage weapon slots are arranged as button + quantity spin box pairs. Click the button to choose a weapon ID, and use the spin box to adjust the quantity (range 0–999).

### Script Console
- In the command input box at the bottom of the "Global" tab, enter an in-game script command (e.g. `spawn`) and press Enter to send it.  
- Because the game supports UTF-8, commands may contain Unicode characters (the game's built-in console cannot input such characters).  
- The execution result (success/failure) is shown in the status bar.

---

## 🗂 Project Structure

| File/Directory | Description |
|-----------|------|
| `mainwindow.cpp/.h/.ui` | Main window UI and logic, including all signals/slots, data refresh, and modification operations |
| `Setting/addrsetting.cpp/.h/.ui` | Offset settings dialog; manages configuration read/write |
| `Memory/memorymanager.cpp/.h` | Low-level wrappers for process enumeration, attach/detach, memory read/write, entity allocation/free, etc. |
| `Memory/gamedatareader.cpp/.h` | Reads/writes game data structures (characters, entities, global state, etc.) according to offsets |
| `Delegates/spinboxdelegate.cpp/.h` | Delegate that provides a QSpinBox editor for tables |
| `WeaponDialog/weapondialog.cpp/.h` | Weapon selection dialog; displays the loaded weapon name list |
| `Languages/` | Qt translation files (`.ts`; supports English, Chinese, and Japanese) |
| `Struct/` | Reverse-engineered game data structure definitions (`thing`, `character`, `mission_state`, etc.) |
| `config.json` | Automatically generated/read configuration file; stores offsets, refresh interval, language, etc. |
| `CMakeLists.txt` | Main CMake project configuration file |

---

## ⚠️ Notes

- **Game Version Compatibility**: Offsets may change with game updates. If data looks incorrect, update the offsets via the "Settings" function (you can relocate them with tools such as Cheat Engine).  
- **Stability**: Memory modification carries a risk of crashing. Back up your saves in advance. If the game crashes, simply restart the game; the tool does not need to be restarted.  
- **Editing Conflicts**: While editing fields, automatic refresh is paused to prevent input interruption caused by data overwrites. However, do not use other memory modification tools at the same time to avoid conflicts.  
- **Read-Only Use**: If you only need to view data, avoid accidentally modifying fields; by default, any UI change is written to memory immediately.  
- **Language Support**: UI languages are loaded dynamically from the translations directory, supporting the system language and all available translation files (currently Chinese, English, and Japanese). You can switch freely in Settings and add custom translations.  
- **Destroying an Entity vs Deleting a Character**: Executing **"Destroy"** in the Entity panel only removes the entity object in the game world (such as a character model or item model). It does **not** release the character data (character slot) bound to that entity.

---

## 🤝 Acknowledgements

- Thanks to Rocketcat Games and Madgarden for creating this excellent game.  
- This tool is based on reverse-engineering research; structure definitions reference publicly available community information and personal analysis.

---

## 📜 License

This project is for personal learning and research use only. Commercial use or use for infringement is prohibited.  
Unmodified official releases may be freely distributed. If you modify and distribute it, the original author assumes no responsibility.