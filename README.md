# Death Road to Canada Debug Tool

## The CSharpProject won't update(maybe), If you want to get latest version please go to the cpp branch

A memory editing utility for the game **Death Road to Canada**, designed to help players inspect and modify in‑game data such as character stats, entities, weapons, and resources.  
Built with C# and a multi‑language UI.

---

## Features

- **Process Attachment** – Attach to the running game process with a single click.
- **Player Stats** – View and edit health, perks, traits, position, AI settings, and debug flags (no‑collide, invisible, invincible, glow, etc.).
- **Entity Management** – Scan all game entities (humans, zombies, items, projectiles, furniture, vehicles), filter by type, teleport, swap positions, and update entity properties.
- **Weapon Pool** – Browse and modify weapon slots, stack counts, and lock status.
- **Global & Player Resources** – Edit food, gasoline, medical supplies, and ammunition (pistol, rifle, shotgun).
- **Advanced Options** – Bypass stat limits (requires manual console commands or built‑in support).
- **Multi‑Language Support** – Switch between English (en‑US), Simplified Chinese (zh‑CN), and Japanese (ja‑JP) on the fly.
- **Customizable Offsets** – Adjust memory offsets and structure sizes via the Settings window to accommodate different game versions.

---

## Usage

1. Launch **Death Road to Canada**.
2. Open the debug tool and click **Attach** to select the game process.
3. Navigate through the tabs:
   - **Player Stats** – Select a character slot and modify attributes.
   - **Entities** – Scan for entities, choose a source/target, and perform actions (teleport, swap).
   - **Weapons** – Refresh weapon slots and edit each weapon’s data.
   - **Resources** – Adjust global and player‑specific resource values.
   - **Advanced** – Access extra tweaks (e.g., stat limit removal).
4. After making changes, click **Apply** to write the new values to memory.

---

## Configuration

Open the **Settings** window to fine‑tune memory layout parameters (all values are in hexadecimal unless stated otherwise):

| Category             | Setting                          | Description                                 |
|----------------------|----------------------------------|---------------------------------------------|
| **Player**           | Player Array Offset              | Base offset of the player array             |
|                      | Player Struct Size               | Size of each player structure               |
|                      | Player Slots                     | Maximum number of player slots              |
| **Entity**           | Entity Pool Offset               | Offset to the entity pool                   |
|                      | Entity Size                      | Size of each entity structure               |
|                      | Entity Slots                     | Maximum number of entity slots              |
| **Weapon**           | Weapon Pool Offset               | Offset to the weapon pool                   |
|                      | Weapon Size                      | Size of each weapon structure               |
|                      | Weapons Slots                    | Maximum number of weapon slots              |
| **Storage**          | Storage Resource Offset          | Offset for global resource storage          |
|                      | Storage Weapon Offset            | Offset for storage weapon array             |
|                      | Storage Weapon Size              | Size of each storage weapon structure       |
|                      | Storage Weapon Slots             | Number of storage weapon slots              |

> **Note:** Incorrect values may cause the tool to fail or crash. Only adjust if you know the game’s memory layout.

---

## Localization

The tool currently supports three languages:

- `en-US` – English
- `zh-CN` – Simplified Chinese
- `ja-JP` – Japanese

Change the language in the **Settings** window.  
All UI strings are defined in `LanguageManager.cs`. To add a new language, duplicate an existing dictionary and translate the keys.

---


The `LanguageManager` provides:
- `CurrentLanguage` – property to get/set the active language code.
- `Get(string key)` – returns the translated string for the current language.
- `Combine` / `Combine3` – helpers to concatenate translated tokens (e.g., `Read` + `Entities` → `"Read Entities"`).

---

## Contributing

Contributions are welcome! You can help by:

- Reporting bugs or suggesting features.
- Adding support for additional languages.
- Improving the memory offset detection or stability.

Please open an issue or submit a pull request with your changes.

---

## Disclaimer

This tool modifies the memory of a running game. Use at your own risk. The author is not responsible for any game corruption, crashes, or bans (if applicable). Always back up your save files before using debug tools.
