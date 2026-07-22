using DR2CDebugTool.Helpers;
using DR2CDebugTool.Models;
using DR2CDebugTool.Services;
using System;
using System.Collections.ObjectModel;
using System.Linq;

namespace DR2CDebugTool.ViewModels
{
    public class WeaponViewModel : BaseViewModel
    {
        private readonly MemoryService _memory;

        public ObservableCollection<WeaponSlotBinding> WeaponSlots { get; } = [];
        public ObservableCollection<StorageWeaponSlot> StorageWeapons { get; } = [];
        public ObservableCollection<WeaponInfo> FilteredWeaponPool { get; } = [];

        private string _weaponCharInfo = LanguageManager.Get("NoPlayerSelected");
        public string WeaponCharInfo
        {
            get => _weaponCharInfo;
            set => SetProperty(ref _weaponCharInfo, value);
        }

        private string _weaponCountText = LanguageManager.Combine("Weapons", "0", ": ");
        public string WeaponCountText
        {
            get => _weaponCountText;
            set => SetProperty(ref _weaponCountText, value);
        }

        private string _weaponSearchText = "";
        public string WeaponSearchText
        {
            get => _weaponSearchText;
            set { SetProperty(ref _weaponSearchText, value); FilterWeaponPool(); }
        }

        public WeaponViewModel(MemoryService memory)
        {
            _memory = memory;
            InitializeSlots();
        }

        private void InitializeSlots()
        {
            WeaponSlots.Clear();
            foreach (char letter in "abc")
            {
                char upperLetter = (char)(letter - 0x20);
                WeaponSlots.Add(new WeaponSlotBinding
                {
                    SlotId = $"slot_{letter}",
                    DisplayName = LanguageManager.Combine("Weapons", upperLetter.ToString())
                });
            }
        }

        public void InitializeStorage(int slotCount)
        {
            StorageWeapons.Clear();
            for (int i = 0; i < slotCount; i++)
            {
                StorageWeapons.Add(new StorageWeaponSlot { SlotIndex = i });
            }
        }

        public int ScanWeaponPool()
        {
            int count = _memory.WeaponManager.ScanWeapons();
            WeaponCountText = LanguageManager.Combine("Weapons", count.ToString(), ": ");
            FilterWeaponPool();
            return count;
        }

        public void RefreshWeaponSlots(int characterIndex, string? characterName)
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(characterIndex);
            if (playerBase == IntPtr.Zero) return;

            var weapons = _memory.WeaponManager.ReadCharacterWeapons(playerBase);
            ApplyCharacterWeaponSlots(weapons);

            WeaponCharInfo = $"{(characterName ?? "Unknown")} (Index: {characterIndex})";
            ApplyStorageWeaponSlots();
        }

        private void ApplyCharacterWeaponSlots(CharacterWeapon weapons)
        {
            var slotValues = new[]
            {
                new { Slot = WeaponSlots.Count > 0 ? WeaponSlots[0] : null, Weapon = weapons.WeaponA, Stack = weapons.StackA, Locked = weapons.NoDropA == 1 },
                new { Slot = WeaponSlots.Count > 1 ? WeaponSlots[1] : null, Weapon = weapons.WeaponB, Stack = weapons.StackB, Locked = weapons.NoDropB == 1 },
                new { Slot = WeaponSlots.Count > 2 ? WeaponSlots[2] : null, Weapon = weapons.WeaponC, Stack = weapons.StackC, Locked = weapons.NoDropC == 1 }
            };

            foreach (var entry in slotValues)
            {
                if (entry.Slot == null) continue;
                entry.Slot.WeaponId = (int)entry.Weapon;
                entry.Slot.StackCount = (int)entry.Stack;
                entry.Slot.IsLocked = entry.Locked;
                entry.Slot.WeaponName = GetWeaponName((int)entry.Weapon);
            }
        }

        public void ApplyStorageWeaponSlots()
        {
            try
            {
                var s = _memory.Settings;
                IntPtr storageAddr = _memory.ModuleBase + (int)s.StorageWeaponOffset;
                uint slotSize = s.StorageWeaponSize;

                for (int i = 0; i < s.StorageWeaponSlots && i < StorageWeapons.Count; i++)
                {
                    IntPtr slotAddr = (IntPtr)(storageAddr + i * slotSize);
                    uint weaponId = _memory.ReadUInt32(slotAddr);
                    uint count = _memory.ReadUInt32(slotAddr + 4);

                    StorageWeapons[i].WeaponId = (int)weaponId;
                    StorageWeapons[i].Count = (int)count;
                    StorageWeapons[i].WeaponName = GetWeaponName((int)weaponId);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Error reading storage weapons: {ex.Message}");
            }
        }

        public void ApplyAllSlots(int characterIndex)
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(characterIndex);
            if (playerBase == IntPtr.Zero) return;

            if (WeaponSlots.Count < 3) return;

            var weapons = new CharacterWeapon
            {
                WeaponA = (uint)WeaponSlots[0].WeaponId,
                StackA = (uint)WeaponSlots[0].StackCount,
                NoDropA = (uint)(WeaponSlots[0].IsLocked ? 1 : 0),
                WeaponB = (uint)WeaponSlots[1].WeaponId,
                StackB = (uint)WeaponSlots[1].StackCount,
                NoDropB = (uint)(WeaponSlots[1].IsLocked ? 1 : 0),
                WeaponC = (uint)WeaponSlots[2].WeaponId,
                StackC = (uint)WeaponSlots[2].StackCount,
                NoDropC = (uint)(WeaponSlots[2].IsLocked ? 1 : 0)
            };

            if (_memory.WeaponManager.WriteAllWeaponSlots(playerBase, weapons))
            {
                RefreshWeaponSlots(characterIndex, null);
            }
        }

        public void ApplyStorageWeapons()
        {
            if (!_memory.IsReady) return;
            try
            {
                var s = _memory.Settings;
                IntPtr storageAddr = _memory.ModuleBase + (int)s.StorageWeaponOffset;
                uint slotSize = s.StorageWeaponSize;

                for (int i = 0; i < s.StorageWeaponSlots && i < StorageWeapons.Count; i++)
                {
                    IntPtr slotAddr = (IntPtr)(storageAddr + i * slotSize);
                    _memory.WriteUInt32(slotAddr, (uint)StorageWeapons[i].WeaponId);
                    _memory.WriteUInt32(slotAddr + 4, (uint)StorageWeapons[i].Count);
                }

                ApplyStorageWeaponSlots();
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine($"Error applying storage weapons: {ex.Message}");
            }
        }

        public void UpdateWeaponName(int weaponId)
        {
            string name = GetWeaponName(weaponId);
            // Update weapon name for the slot that has this weaponId
            foreach (var slot in WeaponSlots)
            {
                if (slot.WeaponId == weaponId)
                    slot.WeaponName = name;
            }
            foreach (var slot in StorageWeapons)
            {
                if (slot.WeaponId == weaponId)
                    slot.WeaponName = name;
            }
        }

        public void FilterWeaponPool()
        {
            string search = WeaponSearchText?.Trim().ToLower() ?? "";
            FilteredWeaponPool.Clear();

            var source = _memory.WeaponManager.WeaponList;
            if (string.IsNullOrEmpty(search))
            {
                foreach (var w in source)
                    FilteredWeaponPool.Add(w);
            }
            else
            {
                var filtered = source
                    .Where(w => w.Name.ToLower().Contains(search) || w.Id.ToString().Contains(search));
                foreach (var w in filtered)
                    FilteredWeaponPool.Add(w);
                WeaponCountText = $"{LanguageManager.Get("Weapons")}: {FilteredWeaponPool.Count} (Filtered)";
                return;
            }
            WeaponCountText = LanguageManager.Combine("Weapons", source.Count.ToString(), ": ");
        }

        private string GetWeaponName(int weaponId)
        {
            return weaponId == 0 ? LanguageManager.Get("Empty") : _memory.WeaponManager.GetWeaponName(weaponId);
        }
    }
}