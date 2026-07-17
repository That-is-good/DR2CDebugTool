using DR2CDebugTool.Models;
using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Text;
using System.Windows;

namespace DR2CDebugTool.Helpers
{
    public class WeaponManager
    {
        private IntPtr _processHandle = IntPtr.Zero;
        private IntPtr _moduleBase = IntPtr.Zero;
        private Settings _settings = null!;
        private const int WeaponNameLength = 64;

        public ObservableCollection<WeaponInfo> WeaponList { get; } = [];

        public WeaponManager()
        {
        }

        public void Initialize(IntPtr processHandle, IntPtr moduleBase, Settings settings)
        {
            _processHandle = processHandle;
            _moduleBase = moduleBase;
            _settings = settings;
        }

        public bool IsReady() => _processHandle != IntPtr.Zero && _moduleBase != IntPtr.Zero && _settings != null;

        /// <summary>
        /// 扫描武器池 - 索引就是ID
        /// </summary>
        public int ScanWeapons()
        {
            if (!IsReady())
                return 0;

            WeaponList.Clear();

            try
            {
                IntPtr poolStart = _moduleBase + (int)_settings.WeaponPoolOffset;
                uint maxWeapons = _settings.MaxWeapons;
                uint weaponSize = _settings.WeaponSize;

                for (uint i = 0; i < maxWeapons; i++)
                {
                    IntPtr weaponAddr = (IntPtr)(poolStart + i * weaponSize);
                    
                    // 读取武器名称 (偏移 0x00)
                    string name = ReadStringAt(weaponAddr, WeaponNameLength);
                    if (string.IsNullOrEmpty(name)) continue;

                    WeaponList.Add(new WeaponInfo
                    {
                        Id = (int)i,  // 索引就是ID
                        Name = name,
                        BaseAddress = weaponAddr
                    });
                }

                return WeaponList.Count;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"{LanguageManager.Get("WeaponScanError")}: {ex.Message}", LanguageManager.Get("WeaponScanError"), MessageBoxButton.OK, MessageBoxImage.Error);
                return 0;
            }
        }

        private string ReadStringAt(IntPtr address, int maxLen)
        {
            if (address == IntPtr.Zero) return "";
            try
            {
                byte[] buffer = MemoryHelper.ReadBytes(_processHandle, address, maxLen);
                int len = 0;
                while (len < buffer.Length && buffer[len] != 0) len++;
                return Encoding.UTF8.GetString(buffer, 0, len);
            }
            catch
            {
                return "";
            }
        }

        /// <summary>
        /// 读取角色武器数据 - A、B、C三个槽位
        /// </summary>
        public CharacterWeapon ReadCharacterWeapons(IntPtr characterBase)
        {
            var weapons = new CharacterWeapon();

            if (!IsReady() || characterBase == IntPtr.Zero)
                return weapons;

            try
            {
                // 槽位 A
                weapons.WeaponA = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2B4);
                weapons.StackA = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2B0);
                weapons.NoDropA = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2B8);
                
                // 槽位 B
                weapons.WeaponB = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2C0);
                weapons.StackB = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2BC);
                weapons.NoDropB = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2C4);
                
                // 槽位 C
                weapons.WeaponC = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2CC);
                weapons.StackC = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2C8);
                weapons.NoDropC = MemoryHelper.ReadUInt32(_processHandle, characterBase + 0x2D0);
            }
            catch { }

            return weapons;
        }

        /// <summary>
        /// 批量写入所有武器数据
        /// </summary>
        public bool WriteAllWeaponSlots(IntPtr characterBase, CharacterWeapon weapons)
        {
            if (!IsReady() || characterBase == IntPtr.Zero)
                return false;

            try
            {
                // 槽位 A
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2B4, weapons.WeaponA);
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2B0, weapons.StackA);
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2B8, weapons.NoDropA);
                
                // 槽位 B
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2C0, weapons.WeaponB);
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2BC, weapons.StackB);
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2C4, weapons.NoDropB);
                
                // 槽位 C
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2CC, weapons.WeaponC);
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2C8, weapons.StackC);
                MemoryHelper.WriteUInt32(_processHandle, characterBase + 0x2D0, weapons.NoDropC);
                return true;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 根据武器ID(索引)查找武器名称
        /// </summary>
        public string GetWeaponName(int weaponId)
        {
            foreach (var w in WeaponList)
            {
                if (w.Id == weaponId)
                    return w.Name;
            }
            return $"ID:{weaponId} ({LanguageManager.Get("ENTITY_TYPE_UNKNOWN")})";
        }

        /// <summary>
        /// 根据武器ID(索引)查找武器信息
        /// </summary>
        public WeaponInfo? GetWeaponById(int weaponId)
        {
            foreach (var w in WeaponList)
            {
                if (w.Id == weaponId)
                    return w;
            }
            return null;
        }
    }

    /// <summary>
    /// 武器槽绑定 - 包含武器ID、堆叠数量、锁定状态
    /// </summary>
    public class WeaponSlotBinding : INotifyPropertyChanged
    {
        public string SlotId { get; set; } = "";
        public string DisplayName { get; set; } = "";

        // 武器ID (选择哪个武器)
        private int _weaponId;
        public int WeaponId
        {
            get => _weaponId;
            set { _weaponId = value; OnPropertyChanged(); OnPropertyChanged(nameof(WeaponName)); }
        }

        // 武器名称 (显示用)
        private string _weaponName = LanguageManager.Get("Empty");
        public string WeaponName
        {
            get => _weaponName;
            set { _weaponName = value; OnPropertyChanged(); }
        }

        // 堆叠数量 (StackedWeapon)
        private int _stackCount;
        public int StackCount
        {
            get => _stackCount;
            set { _stackCount = value; OnPropertyChanged(); }
        }

        // 是否锁定/不可丢弃 (No-Drop: 0=可丢弃, 1=不可丢弃)
        private bool _isLocked;
        public bool IsLocked
        {
            get => _isLocked;
            set { _isLocked = value; OnPropertyChanged(); }
        }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}