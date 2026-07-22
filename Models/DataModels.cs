using System.ComponentModel;

namespace DR2CDebugTool.Models
{
    public class PlayerStat : INotifyPropertyChanged
    {
        public string StatKey { get; set; } = "";
        public int Index { get; set; }

        private string _displayName = "";
        public string DisplayName
        {
            get => _displayName;
            set { _displayName = value; OnPropertyChanged(); }
        }

        public string Name => DisplayName;

        private int _baseValue = 0;
        public int BaseValue
        {
            get => _baseValue;
            set { _baseValue = value; OnPropertyChanged(); OnPropertyChanged(nameof(EffectiveValue)); }
        }

        private int _bonusValue = 0;
        public int BonusValue
        {
            get => _bonusValue;
            set { _bonusValue = value; OnPropertyChanged(); OnPropertyChanged(nameof(EffectiveValue)); }
        }
        public int EffectiveValue => BaseValue + BonusValue;

        private bool _known = false;
        public bool Known
        {
            get => _known;
            set { _known = value; OnPropertyChanged(); }
        }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
    public class CharacterInfo
    {
        public int Index { get; set; }
        public IntPtr BaseAddress { get; set; }
        public string Name { get; set; } = "";
        public int Health { get; set; }
        public bool IsPlayer { get; set; }
        public string DisplayInfo => $"{Index}: {Name} {(IsPlayer ? "👤" : "🧟")} HP:{Health}";
    }
    public class Position
    {
        public float PosX { get; set; } = 0;
        public float PosY { get; set; } = 0;
        public float PosZ { get; set; } = 0;
        public float VelX { get; set; } = 0;
        public float VelY { get; set; } = 0;
        public float VelZ { get; set; } = 0;
        public byte AreaId { get; set; } = 0;
    }
    public class EntityInfo
    {
        public int Index { get; set; }
        public ushort EntityId { get; set; }
        public IntPtr BaseAddress { get; set; }
        public byte EntityType { get; set; }
        public string TypeName { get; set; } = "";
        public byte SubType { get; set; }
        public string SubTypeName { get; set; } = "";
        public int Health { get; set; }
        public Position Pos { get; set; } = new();

        // 调试属性
        public byte NoCollide { get; set; }
        public byte Invisible { get; set; }
        public byte Invincible { get; set; }
        public float Mass { get; set; }
        public float Friction { get; set; }
        public byte Glow { get; set; }
        public int AIState { get; set; }
        public int AIWait { get; set; }

        public string DisplayInfo => $"#{EntityId} [{TypeName}]";
    }
    public class ProcessInfo
    {
        public int Id { get; set; }
        public string? Name { get; set; }
        public string DisplayName => $"{Name} ({Id})";
    }
    public class WeaponInfo
    {
        public int Id { get; set; }           // 索引 = ID
        public string Name { get; set; } = ""; // 武器名称 (偏移0x00)
        public IntPtr BaseAddress { get; set; }
        
        public string DisplayInfo => $"{Id}: {Name}";
    }
    public class CharacterWeapon
    {
        // 武器槽 A
        public uint WeaponA { get; set; }      // 0x2B4 武器ID
        public uint StackA { get; set; }       // 0x2B0 堆叠数量
        public uint NoDropA { get; set; }      // 0x2B8 是否锁定 (0=可丢弃, 1=不可丢弃)
        
        // 武器槽 B
        public uint WeaponB { get; set; }      // 0x2C0 武器ID
        public uint StackB { get; set; }       // 0x2BC 堆叠数量
        public uint NoDropB { get; set; }      // 0x2C4 是否锁定
        
        // 武器槽 C
        public uint WeaponC { get; set; }      // 0x2CC 武器ID
        public uint StackC { get; set; }       // 0x2C8 堆叠数量
        public uint NoDropC { get; set; }      // 0x2D0 是否锁定
    }
    public class ResourceEntry : INotifyPropertyChanged
    {
        public string Key { get; set; } = "";
        public string Name { get; set; } = "";
        
        private int _value;
        public int Value
        {
            get => _value;
            set { _value = value; OnPropertyChanged(); }
        }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
    public class StorageWeaponSlot : INotifyPropertyChanged
    {
        public int SlotIndex { get; set; }
        
        private int _weaponId;
        public int WeaponId
        {
            get => _weaponId;
            set { _weaponId = value; OnPropertyChanged(); OnPropertyChanged(nameof(WeaponName)); }
        }
        
        private string _weaponName = LanguageManager.Get("Empty");
        public string WeaponName
        {
            get => _weaponName;
            set { _weaponName = value; OnPropertyChanged(); }
        }
        
        private int _count;
        public int Count
        {
            get => _count;
            set { _count = value; OnPropertyChanged(); }
        }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string? name = null)
            => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}