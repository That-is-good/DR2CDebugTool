# Death Road to Canada Debug Tool

## 📖 Full Documentation (Language Versions)

- [English Version](./Readme/EN.md)  
- [中文版本](./Readme/ZH_CN.md)  
- [日本語版](./Readme/JA.md)

Each language version contains detailed feature descriptions, usage guides, project structure, and important notes.

---

> A real‑time memory debugging and editing tool for *Death Road to Canada*, built with **Qt6** and C++.  
> While the game is running, you can view and modify character attributes, entity states, global resources, and weapon data – useful for game research, mod development, or advanced data analysis.

---

## ✨ Key Features

- **Process Management** – enumerate system processes, filter by name/PID, attach/detach with one click; automatically detaches when the target process exits
- **Character Editing** – modify 13 core attributes (base/temp/bonus + known flag), 8 resources, 3 weapon slots, health, speed, gender, pet, and status flags; party members are marked with **★** in the character drop-down
- **Entity Visualization** – display entities as icons on a map grid; support select / zoom / pan modes; **drag‑and‑drop to move entities** (single or multi‑selection); right‑click menu offers:
  - Set as Center (target marker with yellow circle)
  - Flags (no collision, invisible, not drawn, no hit, no pickup, glow)
  - Edit Position / Velocity / Physics / Other
  - Teleport to Center, Random Swap, Clone, Destroy, Set as Player Entity
  - Spawn (9 types: Human, Zombie, Item, Projectile, Furniture, Pickup, Weapon, Vehicle, Special Pickup)
  - Set Render Layer
  - Area drop-down dynamically lists existing areas
- **Global Resources** – adjust 8 resource types (food, gas, medical, etc.) and manage 15 storage weapon slots
- **Script Console** – send in‑game script commands (supports UTF‑8 characters)
- **Auto Refresh** – customisable refresh interval (default 500 ms); automatically pauses while editing to avoid conflicts
- **Persistent Configuration** – offsets, refresh rate, and language preferences saved in `config.json` for easy migration across game updates

---

## 🚀 Quick Start

### Requirements
- Windows 10/11
- CMake 3.19+, C++17 compiler (MSVC 2019+ / MinGW‑w64 8.0+)
- Qt 6.5+ (components: `Core`, `Widgets`, `LinguistTools`)

### Build
```bash
git clone https://github.com/yourusername/drtc-debug-tool.git
cd drtc-debug-tool
```
Open `CMakeLists.txt` with Qt Creator, select a kit with Qt 6.5+, and click “Build”.  
The executable will be generated under `build/Release/` or `build/`.

### Run
Launch `DR2CDebugTool.exe` directly (if Qt DLLs are missing, use `windeployqt` or add Qt’s `bin` directory to your PATH).  
After startup, select the game process (usually `prog.exe`) from the process list and click “Attach” to begin.

---

## ⚠️ Important Notes

- This tool is built based on reverse‑engineering of a specific game version; offsets may change with game updates – adjust them via the “Settings” dialog.
- Memory editing carries a risk of crashes; please back up your save files beforehand.
- **Destroying an entity** in the Entity panel only removes the game‑world object (model, item, etc.); it does **not** release the bound character slot.
- When spawning non‑character entities (items, projectiles, furniture, etc.), you usually need to set a valid **Sprite ID** via the “Edit Other” dialog, otherwise they may not render correctly.
- This tool is intended for personal learning and research only. Commercial use or any action that infringes upon others’ rights is prohibited.
- Unmodified official releases may be freely redistributed. If you distribute a modified version, you assume all responsibility.

---

## 📜 License

This project is for personal learning and research purposes only. Commercial use or any form of infringement is strictly prohibited.  
For full license details, please refer to the license statements in each language document.

---

For questions or suggestions, feel free to open an Issue or submit a Pull Request.