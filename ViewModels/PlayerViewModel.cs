using DR2CDebugTool.Models;
using DR2CDebugTool.Services;
using System;
using System.Collections.ObjectModel;

namespace DR2CDebugTool.ViewModels
{
    public class PlayerViewModel : BaseViewModel
    {
        private readonly MemoryService _memory;
        public ObservableCollection<CharacterInfo> CharacterList { get; } = [];
        public ObservableCollection<PlayerStat> PlayerStats { get; } = [];
        public uint PlayerSlots {get; set;} = 0x100;

        private int _currentCharacterIndex = 0;
        public int CurrentCharacterIndex
        {
            get => _currentCharacterIndex;
            set => SetProperty(ref _currentCharacterIndex, value);
        }

        private string _playerName = "";
        public string PlayerName
        {
            get => _playerName;
            set => SetProperty(ref _playerName, value);
        }

        private int _playerHealth;
        public int PlayerHealth
        {
            get => _playerHealth;
            set => SetProperty(ref _playerHealth, value);
        }

        private string _playerPerk = "";
        public string PlayerPerk
        {
            get => _playerPerk;
            set => SetProperty(ref _playerPerk, value);
        }

        private string _playerTrait = "";
        public string PlayerTrait
        {
            get => _playerTrait;
            set => SetProperty(ref _playerTrait, value);
        }

        private string _weaponCharInfo = LanguageManager.Get("NoPlayerSelectedText");
        public string WeaponCharInfo
        {
            get => _weaponCharInfo;
            set => SetProperty(ref _weaponCharInfo, value);
        }

        public string OffsetInfoText
        {
            get
            {
                if (_memory.Settings == null) return "Settings not loaded";
                var s = _memory.Settings;
                return $"Struct Offset: 0x{s.PlayerArrayOffset:X}  |  Size: 0x{s.PlayerStructSize:X}  |  HP: 0x{s.PlayerHealthOffset:X}";
            }
        }

        public PlayerViewModel(MemoryService memory)
        {
            _memory = memory;
            InitializeStats();
        }

        public void RefreshOffsetInfo() => OnPropertyChanged(nameof(OffsetInfoText));

        private void InitializeStats()
        {
            string[] statKeys = [
                "MORALE", "ATTITUDE", "COMPOSURE", "CHARM", "WITS",
                "LOYALTY", "MEDICAL", "MECHANICAL", "SHOOTING",
                "STRENGTH", "DEXTERITY", "FITNESS", "VITALITY"
            ];
            PlayerStats.Clear();
            for (int i = 0; i < statKeys.Length; i++)
            {
                PlayerStats.Add(new PlayerStat
                {
                    StatKey = statKeys[i],
                    Index = i,
                    DisplayName = LanguageManager.Get("STAT_" + statKeys[i])
                });
            }
        }

        public void ScanCharacters()
        {
            if (!_memory.IsReady) return;
            IntPtr lastbaseAddr = _memory.GetPlayerBase(CurrentCharacterIndex);
            CurrentCharacterIndex = -1;

            CharacterList.Clear();
            for (int i = 0; i < PlayerSlots; i++)
            {
                IntPtr baseAddr = _memory.GetPlayerBase(i);
                if (baseAddr == IntPtr.Zero) break;

                try
                {
                    int health = _memory.ReadHealth(baseAddr);
                    string name = _memory.ReadPlayerName(baseAddr);
                    if (string.IsNullOrEmpty(name) && health == 0) break;

                    string perk = _memory.ReadPerk(baseAddr);
                    string trait = _memory.ReadTrait(baseAddr);

                    CharacterList.Add(new CharacterInfo
                    {
                        Index = i,
                        BaseAddress = baseAddr,
                        Name = name,
                        Health = health,
                        IsPlayer = !string.IsNullOrEmpty(perk) || !string.IsNullOrEmpty(trait)
                    });
                    if (lastbaseAddr == baseAddr)
                    {
                        SelectCharacter(i);
                    }
                }
                catch { break; }
            }
        }

        public void SelectCharacter(int index)
        {
            CurrentCharacterIndex = index;
            ReadPlayerStats();
            ReadPlayerInfo();
        }

        public void ReadPlayerInfo()
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                PlayerName = _memory.ReadPlayerName(playerBase);
                PlayerHealth = _memory.ReadHealth(playerBase);
                PlayerPerk = _memory.ReadPerk(playerBase);
                PlayerTrait = _memory.ReadTrait(playerBase);
                WeaponCharInfo = $"{PlayerName} (Index: {_currentCharacterIndex})";
            }
            catch { }
        }

        public void WritePlayerName()
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;
            _memory.WritePlayerName(playerBase, PlayerName);
            ReadPlayerInfo();
        }

        public void WritePlayerHealth()
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;
            _memory.WriteHealth(playerBase, PlayerHealth);
        }

        public void ReadPlayerStats()
        {
            if (!_memory.IsReady)
            {
                foreach (var stat in PlayerStats) { stat.BaseValue = 0; stat.BonusValue = 0; stat.Known = false; }
                return;
            }

            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                var s = _memory.Settings;
                // 0x1BC: stat_display_flags[13] - 0=不显示, 1=显示
                IntPtr flagsAddr = playerBase + s.PlayerDisplayFlagOffset;
                for (int i = 0; i < PlayerStats.Count; i++)
                {
                    PlayerStats[i].BaseValue = (sbyte)_memory.ReadByte(playerBase + s.PlayerBaseOffset + i);
                    PlayerStats[i].BonusValue = (sbyte)_memory.ReadByte(playerBase + s.PlayerBonusOffset + i);
                    PlayerStats[i].Known = _memory.ReadByte(flagsAddr + i) == 1;
                }
            }
            catch { }
        }

        public void WriteKnownFlags()
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                var s = _memory.Settings;
                IntPtr flagsAddr = playerBase + s.PlayerDisplayFlagOffset;
                for (int i = 0; i < PlayerStats.Count; i++)
                {
                    _memory.WriteByte(flagsAddr + i, (byte)(PlayerStats[i].Known ? 1 : 0));
                }
            }
            catch { }
        }

        public void WriteBaseStats()
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                var s = _memory.Settings;
                for (int i = 0; i < PlayerStats.Count; i++)
                {
                    _memory.WriteByte(playerBase + s.PlayerBaseOffset + i, (byte)PlayerStats[i].BaseValue);
                }
            }
            catch { }
        }

        public void WriteBonusStats()
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(_currentCharacterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                var s = _memory.Settings;
                for (int i = 0; i < PlayerStats.Count; i++)
                {
                    _memory.WriteByte(playerBase + s.PlayerBonusOffset + i, (byte)(sbyte)PlayerStats[i].BonusValue);
                }
            }
            catch { }
        }
    }
}