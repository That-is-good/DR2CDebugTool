using DR2CDebugTool.Controls;
using DR2CDebugTool.Helpers;
using DR2CDebugTool.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Data;

namespace DR2CDebugTool
{
    public partial class MainWindow : Window, INotifyPropertyChanged
    {
        private const int CharacterScanLimit = 30;
        private const int TextBufferLength = 64;
        private const int MaxEntityScanCount = 500;
        private const int EntityCountAddressOffset = 0x3CC318;

        private Settings _settings;
        private IntPtr _processHandle = IntPtr.Zero;
        private IntPtr _moduleBase = IntPtr.Zero;
        private int _currentCharacterIndex = 0;

        private EntityInfo? _currentEntity = null;
        private EntityInfo? _targetEntity = null;
        private WeaponManager _weaponManager = new();
        public ObservableCollection<ProcessInfo> AllProcesses { get; } = [];
        public ObservableCollection<ProcessInfo> FilteredProcesses { get; } = [];
        private ObservableCollection<WeaponSlotBinding> _weaponSlotBindings = [];
        private ObservableCollection<PlayerStat> _playerStats = [];
        private ObservableCollection<CharacterInfo> _characterList = [];
        private ObservableCollection<EntityInfo> _entityList = [];
        private ObservableCollection<EntityInfo> _filteredEntityList = [];
        private ObservableCollection<StorageWeaponSlot> _storageWeapons = [];
        private ObservableCollection<ResourceEntry> _globalResources = [];
        private ObservableCollection<ResourceEntry> _playerResources = [];

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged(string name) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        private bool HasProcessContext() => _processHandle != IntPtr.Zero && _moduleBase != IntPtr.Zero && _settings != null;

        private bool TryGetCurrentPlayerBase(out IntPtr playerBase)
        {
            playerBase = IntPtr.Zero;
            if (!HasProcessContext()) return false;

            playerBase = GetPlayerBase(_currentCharacterIndex);
            return playerBase != IntPtr.Zero;
        }

        private bool TryGetPlayerBase(int index, out IntPtr playerBase)
        {
            playerBase = IntPtr.Zero;
            if (!HasProcessContext()) return false;

            playerBase = GetPlayerBase(index);
            return playerBase != IntPtr.Zero;
        }

        private void SetStatus(string message) => StatusText = message;
        // 绑定属性
        public string TitleText => LanguageManager.Get("MainWindow_Title");
        public string RefreshText => LanguageManager.Get("Refresh");
        public string AttachText => LanguageManager.Get("Attach");

        public string TypeText => LanguageManager.Get("Type");
        public string SubTypeText => LanguageManager.Get("SubType");
        public string AreaText => LanguageManager.Get("Area");
        public string SearchText => LanguageManager.Get("Search");

        public string CharacterText => LanguageManager.Get("Character");
        public string PerkLabel => LanguageManager.Get("Perk");
        public string TraitLabel => LanguageManager.Get("Trait");

        public string PositionText => LanguageManager.Get("PositionText");
        public string BasicInfoText => LanguageManager.Get("BasicInfoText");
        public string DebugPropertiesText => LanguageManager.Get("DebugPropertiesText");
        public string AIPropertiesText => LanguageManager.Get("AIPropertiesText");
        public string PosText => LanguageManager.Get("Pos");
        public string VelText => LanguageManager.Get("Vel");
        public string SettingsText => LanguageManager.Get("Settings");
        public string PlayerStatsTab => LanguageManager.Get("PlayerStats");
        public string EntitiesTab => LanguageManager.Get("Entities");
        public string AdvancedTab => LanguageManager.Get("Advanced");
        public string ReadText => LanguageManager.Get("Read");
        public string WriteText => LanguageManager.Get("Write");
        public string ApplyText => LanguageManager.Get("Apply");
        public string NoCollideText => LanguageManager.Get("NoCollide");
        public string InvisibleText => LanguageManager.Get("Invisible");
        public string InvincibleText => LanguageManager.Get("Invincible");
        public string GlowText => LanguageManager.Get("Glow");
        public string MassText => LanguageManager.Get("Mass");
        public string FrictionText => LanguageManager.Get("Friction");
        public string AIStateText => LanguageManager.Get("AIState");
        public string AIWaitText => LanguageManager.Get("AIWait");

        public string ScanCharaText => LanguageManager.Get("ScanCharacters");
        // 实体相关绑定
        public string ScanEntitiesText => LanguageManager.Get("ScanEntity");
        public string EntityNoTargetSelected => LanguageManager.Get("NoTargetEntity");
        public string EntityNoSourceSelected => LanguageManager.Get("NoSourceEntity");
        public string EntityNameText => LanguageManager.Get("Name");
        public string EntityHPText => LanguageManager.Get("Health");
        public string EntityAllText => LanguageManager.Get("ENTITY_TYPE_ALL");
        public string EntityHumanText => LanguageManager.Get("ENTITY_TYPE_HUMAN");
        public string EntityZombieText => LanguageManager.Get("ENTITY_TYPE_ZOMBIE");
        public string EntityItemText => LanguageManager.Get("ENTITY_TYPE_ITEM");
        public string EntityProjectileText => LanguageManager.Get("ENTITY_TYPE_PROJECTILE");
        public string TeleportToTargetText => LanguageManager.Get("TeleportToTarget");
        public string SwapPositionsText => LanguageManager.Get("SwapPositions");
        public string SetTargetText => LanguageManager.Get("SetTarget");

        public string ScanWeaponPoolText => LanguageManager.Get("ScanWeaponPool");
        public string RefreshWeaponText => LanguageManager.Get("WeaponRefresh");
        public string WeaponsTab => LanguageManager.Get("Weapons");
        public string WeaponSoltText => LanguageManager.Get("WeaponSlot");
        public string WeaponNameText => LanguageManager.Get("WeaponName");
        public string CurrentCharacterText => LanguageManager.Get("CurrentCharacter");
        public string NoPlayerSelectedText => LanguageManager.Get("NoPlayerSelected");
        public string StackText => LanguageManager.Get("Stack");
        public string LockedText => LanguageManager.Get("NoDrop");
        public string StorageWeaponText => LanguageManager.Get("StorageWeapon");

        public string ResourcesTab => LanguageManager.Get("Resources");
        public string GlobalResourcesText => LanguageManager.Get("GlobalResources");
        public string PlayerResourcesText => LanguageManager.Get("PlayerResources");

        public string Error => LanguageManager.Get("Error");
        public string NewMaxText => LanguageManager.Get("NewMaxText");
        
        public string OffsetInfoText
        {
            get
            {
                if (_settings == null) return "Settings not loaded";
                return $"Struct Offset: 0x{_settings.PlayerArrayOffset:X}  |  Size: 0x{_settings.PlayerStructSize:X}  |  HP: 0x{_settings.PlayerHealthOffset:X}";
            }
        }

        private string _statusText = "";
        public string StatusText
        {
            get => _statusText;
            set { _statusText = value; OnPropertyChanged(nameof(StatusText)); }
        }
        public MainWindow()
        {
            InitializeComponent();

            DataContext = this;
            _settings = Settings.Load();
            LanguageManager.CurrentLanguage = _settings.Language;

            _filteredEntityList = [];
    
            InitializePlayerStats();
            InitializeBindings();
            InitializeWeaponTab();
            InitializeResources();

            StatusText = LanguageManager.Get("NotAttached");
        }
        ~MainWindow()
        {
            MemoryHelper.CloseHandle(_processHandle);
        }
        private void InitializeBindings()
        {
            if (ProcessComboBox != null)
            {
                ProcessComboBox.ItemsSource = FilteredProcesses;
                ProcessComboBox.DisplayMemberPath = "DisplayName";
                ProcessComboBox.SelectedValuePath = "Id";
            }
            
            if (CharacterListBox != null)
            {
                CharacterListBox.ItemsSource = _characterList;
                CharacterListBox.DisplayMemberPath = "DisplayInfo";
                CharacterListBox.SelectedValuePath = "Index";
            }
            
            // 实体表格绑定
            if (EntityDataGrid != null)
            {
                if (_filteredEntityList == null)
                {
                    _filteredEntityList = [];
                }
                EntityDataGrid.ItemsSource = _filteredEntityList;
            }
        }
        private void InitializeWeaponTab()
        {
            _weaponSlotBindings.Clear();
            foreach (char letter in "abc")
            {
                _weaponSlotBindings.Add(new WeaponSlotBinding { SlotId = $"slot_{letter}", DisplayName = $"{WeaponsTab} {(char)(letter - 0x20)}" });
            }
            
            WeaponSlotsGrid?.ItemsSource = _weaponSlotBindings;
            
            WeaponPoolGrid?.ItemsSource = _weaponManager.WeaponList;

            _storageWeapons.Clear();
            for (int i = 0; i < _settings.StorageWeaponSlots; i++)
            {
                _storageWeapons.Add(new StorageWeaponSlot { SlotIndex = i });
            }
            
            if (StorageWeaponsGrid != null)
                StorageWeaponsGrid.ItemsSource = _storageWeapons;
        }
        private void InitializePlayerStats()
        {
            string[] statKeys = [ 
                "MORALE", "ATTITUDE", "COMPOSURE", "CHARM", "WITS", 
                "LOYALTY", "MEDICAL", "MECHANICAL", "SHOOTING", 
                "STRENGTH", "DEXTERITY", "FITNESS", "VITALITY" 
            ];
            _playerStats.Clear();
            for (int i = 0; i < statKeys.Length; i++)
            {
                _playerStats.Add(new PlayerStat 
                { 
                    StatKey = statKeys[i], 
                    Index = i,
                    DisplayName = LanguageManager.Get("STAT_" + statKeys[i])
                });
            }
            PlayerStatsGrid.ItemsSource = _playerStats;
            RefreshColumnHeaders();
        }
        private void InitializeResources()
        {
            Dictionary<string, string> res = new(){
                {"food", LanguageManager.Get("Food")},
                {"gas", LanguageManager.Get("Gasoline")},
                {"medical", LanguageManager.Get("Medical")},
                {"bullet", LanguageManager.Get("PistolAmmo")},
                {"rifle", LanguageManager.Get("RifleAmmo")},
                {"shell", LanguageManager.Get("ShotgunAmmo")},
            };
            // 全局资源
            _globalResources.Clear();
            _playerResources.Clear();
            foreach (var e in res)
            {
                _globalResources.Add(new ResourceEntry { Key = e.Key, Name = e.Value });
                _playerResources.Add(new ResourceEntry { Key = e.Key, Name = e.Value });
            }      
            GlobalResourcesGrid?.ItemsSource = _globalResources;
            PlayerResourcesGrid?.ItemsSource = _playerResources;
        }
        private void RefreshColumnHeaders()
        {

            PlayerStatsGrid.Columns[0].Header = LanguageManager.Get("Attribute");
            PlayerStatsGrid.Columns[1].Header = LanguageManager.Get("BaseStat");
            PlayerStatsGrid.Columns[2].Header = LanguageManager.Get("BonusStat");
            PlayerStatsGrid.Columns[3].Header = LanguageManager.Get("EffectiveStat");

            EntityDataGrid.Columns[1].Header = TypeText;
            EntityDataGrid.Columns[2].Header = SubTypeText;
            EntityDataGrid.Columns[3].Header = EntityHPText;
            EntityDataGrid.Columns[4].Header = AreaText;
            string _axis = "XYZ";
            for (int i = 0; i < _axis.Length; ++i)
            {
                EntityDataGrid.Columns[5 + i].Header = PosText + _axis[i];
            }
            EntityDataGrid.Columns[8].Header = LanguageManager.Get("Address");

            WeaponSlotsGrid.Columns[0].Header = WeaponSoltText;
            WeaponSlotsGrid.Columns[1].Header = WeaponsTab + " ID";
            WeaponSlotsGrid.Columns[2].Header = WeaponNameText;
            WeaponSlotsGrid.Columns[3].Header = StackText;
            WeaponSlotsGrid.Columns[4].Header = LockedText;

            StorageWeaponsGrid.Columns[1].Header = WeaponsTab + " ID";
            StorageWeaponsGrid.Columns[2].Header = WeaponNameText;
            StorageWeaponsGrid.Columns[3].Header = StackText;

            WeaponPoolGrid.Columns[0].Header = WeaponsTab + " ID";
            WeaponPoolGrid.Columns[1].Header = WeaponNameText;

            GlobalResourcesGrid.Columns[0].Header = ResourcesTab;
            GlobalResourcesGrid.Columns[1].Header = StackText;

            PlayerResourcesGrid.Columns[0].Header = ResourcesTab;
            PlayerResourcesGrid.Columns[1].Header = StackText;
        }
        // ===== 辅助方法 =====
        private IntPtr GetPlayerBase(int index)
        {
            if (_moduleBase == IntPtr.Zero || _settings == null) return IntPtr.Zero;
            return _moduleBase + (int)_settings.PlayerArrayOffset + index * (int)_settings.PlayerStructSize;
        }

        private string ReadStringAt(IntPtr hProcess, IntPtr address, int maxLen)
        {
            if (address == IntPtr.Zero) return "";
            try
            {
                byte[] buffer = MemoryHelper.ReadBytes(hProcess, address, maxLen);
                int len = 0;
                while (len < buffer.Length && buffer[len] != 0) len++;
                return Encoding.UTF8.GetString(buffer, 0, len);
            }
            catch
            {
                return "";
            }
        }

        private void WriteStringAt(IntPtr hProcess, IntPtr address, string str, int maxLen)
        {
            if (address == IntPtr.Zero) return;
            byte[] strBytes = Encoding.UTF8.GetBytes(str);
            if (strBytes.Length >= maxLen)
                Array.Resize(ref strBytes, maxLen - 1);
            byte[] buffer = new byte[maxLen];
            Array.Copy(strBytes, buffer, strBytes.Length);
            buffer[strBytes.Length] = 0;
            MemoryHelper.WriteBytes(hProcess, address, buffer);
        }
        // ===== 进程管理 =====
        private void RefreshProcesses()
        {
            AllProcesses.Clear();
            var allProcs = Process.GetProcesses();
            foreach (var p in allProcs.OrderBy(p => p.ProcessName))
            {
                try
                {
                    AllProcesses.Add(new ProcessInfo { Id = p.Id, Name = p.ProcessName });
                }
                catch { }
            }
            ApplyFilter();
            if (FilteredProcesses.Count > 0)
                ProcessComboBox.SelectedIndex = 0;
            else
                SetStatus(LanguageManager.Get("NoProcesses"));
        }

        private void ApplyFilter()
        {
            string filter = ProcessFilterBox.Text?.Trim().ToLower() ?? "";
            FilteredProcesses.Clear();
            foreach (var p in AllProcesses)
            {
                if (string.IsNullOrEmpty(filter) || p.Name?.ToLower().Contains(filter) == true)
                    FilteredProcesses.Add(p);
            }
        }

        private void ProcessFilterBox_TextChanged(object sender, TextChangedEventArgs e) => ApplyFilter();
        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            // 确保控件已完全加载
            if (EntityTypeFilter != null && EntityAreaFilter != null && EntitySearchBox != null)
            {
                RefreshProcesses();
            }
            else
            {
                // 如果控件还没加载，延迟执行
                Dispatcher.BeginInvoke(new Action(() => RefreshProcesses()));
            }
        }
        private void RefreshProcesses_Click(object sender, RoutedEventArgs e) => RefreshProcesses();

        private void Attach_Click(object sender, RoutedEventArgs e)
        {
            if (ProcessComboBox.SelectedItem == null) return;
            var selected = (ProcessInfo)ProcessComboBox.SelectedItem;
            int pid = selected.Id;
            _processHandle = MemoryHelper.OpenProcessForMemory(pid);
            if (_processHandle == IntPtr.Zero)
            {
                SetStatus(LanguageManager.Get("FailedOpenProcess"));
                return;
            }
            try
            {
                var proc = Process.GetProcessById(pid);
                _moduleBase = proc.MainModule?.BaseAddress ?? IntPtr.Zero;
                _weaponManager.Initialize(_processHandle, _moduleBase, _settings);
                if (_moduleBase == IntPtr.Zero)
                {
                    SetStatus(LanguageManager.Get("FailedGetModuleBase"));
                    return;
                }
            }
            catch (Exception ex)
            {
                SetStatus($"{LanguageManager.Get("ErrorGettingModule")}: {ex.Message}");
                return;
            }
            SetStatus($"{LanguageManager.Get("Attached")} {pid} - Base: {_moduleBase.ToInt64():X}");
            OnPropertyChanged(nameof(OffsetInfoText));
            ScanCharacters();
            ScanEntities();
        }

        // ===== 玩家角色扫描 =====
        private void ScanCharacters_Click(object sender, RoutedEventArgs e) => ScanCharacters();

        private void ScanCharacters()
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            _characterList.Clear();

            for (int i = 0; i < CharacterScanLimit; i++)
            {
                if (!TryGetPlayerBase(i, out IntPtr baseAddr))
                    break;

                if (!TryReadCharacterInfo(i, baseAddr, out CharacterInfo? charInfo))
                    break;

                _characterList.Add(charInfo!);
            }

            if (_characterList.Count > 0)
            {
                CharacterListBox.SelectedIndex = 0;
                SetStatus($"{LanguageManager.Get("Found")} {_characterList.Count} {CharacterText}");
            }
        }

        private bool TryReadCharacterInfo(int index, IntPtr baseAddr, out CharacterInfo? charInfo)
        {
            charInfo = null;
            try
            {
                int health = MemoryHelper.ReadInt32(_processHandle, baseAddr + _settings.PlayerHealthOffset);
                string name = ReadStringAt(_processHandle, baseAddr + _settings.PlayerNameOffset, TextBufferLength);

                if (string.IsNullOrEmpty(name) && health == 0)
                    return false;

                string perk = ReadStringAt(_processHandle, baseAddr + _settings.PlayerPerkOffset, TextBufferLength);
                string trait = ReadStringAt(_processHandle, baseAddr + _settings.PlayerTraitOffset, TextBufferLength);

                charInfo = new CharacterInfo
                {
                    Index = index,
                    BaseAddress = baseAddr,
                    Name = name,
                    Health = health,
                    IsPlayer = !string.IsNullOrEmpty(perk) || !string.IsNullOrEmpty(trait)
                };
                return true;
            }
            catch
            {
                return false;
            }
        }

        private void CharacterListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (CharacterListBox.SelectedItem is CharacterInfo selected)
            {
                _currentCharacterIndex = selected.Index;
                ReadPlayerStats();
                ReadPlayerInfo();
                RefreshWeaponSlots_Click(sender, e);
            }
        }

        // ===== 玩家属性读写 =====
        private void ReadPlayerStats()
        {
            if (!HasProcessContext())
            {
                foreach (var stat in _playerStats) { stat.BaseValue = 0; stat.BonusValue = 0; }
                return;
            }

            if (!TryGetCurrentPlayerBase(out IntPtr roleBase)) return;
            try
            {
                for (int i = 0; i < _playerStats.Count; i++)
                {
                    IntPtr baseAddr = roleBase + _settings.PlayerBaseOffset + i;
                    _playerStats[i].BaseValue = MemoryHelper.ReadByte(_processHandle, baseAddr);
                    IntPtr bonusAddr = roleBase + _settings.PlayerBonusOffset + i;
                    _playerStats[i].BonusValue = (sbyte)MemoryHelper.ReadByte(_processHandle, bonusAddr);
                }
            }
            catch (Exception ex)
            {
                SetStatus($"{ReadText}: {ex.Message}");
            }
            
        }
        private void ReadPlayerStats_Click(object sender, RoutedEventArgs e) => ReadPlayerStats();
        // ===== 玩家信息读写 =====
        private void ReadPlayerInfo()
        {
            if (!HasProcessContext()) return;
            if (!TryGetCurrentPlayerBase(out IntPtr roleBase)) return;
            try
            {
                NameBox.Text = ReadStringAt(_processHandle, roleBase + _settings.PlayerNameOffset, TextBufferLength);
                int health = MemoryHelper.ReadInt32(_processHandle, roleBase + _settings.PlayerHealthOffset);
                HealthBox.Value = health;
                PerkBox.Text = ReadStringAt(_processHandle, roleBase + _settings.PlayerPerkOffset, TextBufferLength);
                TraitBox.Text = ReadStringAt(_processHandle, roleBase + _settings.PlayerTraitOffset, TextBufferLength);
            }
            catch { }
        }
        private void CharacterHealth_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!TryGetCurrentPlayerBase(out IntPtr roleBase)) return;
            MemoryHelper.WriteInt32(_processHandle, roleBase + _settings.PlayerHealthOffset, (int)HealthBox.Value);
            ReadPlayerInfo();
        }
        private void BaseValue_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!TryGetCurrentPlayerBase(out IntPtr roleBase)) return;
            try
            {
                for (int i = 0; i < _playerStats.Count; i++)
                {
                    var stat = _playerStats[i];
                    byte baseVal = (byte)stat.BaseValue;
                    IntPtr baseAddr = roleBase + _settings.PlayerBaseOffset + i;
                    MemoryHelper.WriteByte(_processHandle, baseAddr, baseVal);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"{Error}: {ex.Message}");
            }
        }
        private void BonusValue_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!TryGetCurrentPlayerBase(out IntPtr roleBase)) return;
            try
            {
                for (int i = 0; i < _playerStats.Count; i++)
                {
                    var stat = _playerStats[i];
                    sbyte bonusVal = (sbyte)stat.BonusValue;
                    IntPtr bonusAddr = roleBase + _settings.PlayerBonusOffset + i;
                    MemoryHelper.WriteByte(_processHandle, bonusAddr, (byte)bonusVal);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"{Error}: {ex.Message}");
            }
        }
        private void NameBox_OnKeyDown(object sender, KeyEventArgs e)
        {
            if (!TryGetCurrentPlayerBase(out IntPtr roleBase)) return;
            WriteStringAt(_processHandle, roleBase + _settings.PlayerNameOffset, NameBox.Text ?? "", TextBufferLength);
            ReadPlayerInfo();
        }
        // ===== 实体数据读写（统一方法） =====
        private Position ReadEntityPosition(IntPtr entityAddr)
        {
            if (_processHandle == IntPtr.Zero || entityAddr == IntPtr.Zero || _settings == null)
                return new();

            try
            {
                Position Pos = new()
                {
                    //坐标
                    PosX = MemoryHelper.ReadFloat(_processHandle, entityAddr + _settings.EntityPosXOffset),
                    PosY = MemoryHelper.ReadFloat(_processHandle, entityAddr + _settings.EntityPosYOffset),
                    PosZ = MemoryHelper.ReadFloat(_processHandle, entityAddr + _settings.EntityPosZOffset),

                    // 速度
                    VelX = MemoryHelper.ReadFloat(_processHandle, entityAddr + _settings.EntityVelXOffset),
                    VelY = MemoryHelper.ReadFloat(_processHandle, entityAddr + _settings.EntityVelYOffset),
                    VelZ = MemoryHelper.ReadFloat(_processHandle, entityAddr + _settings.EntityVelZOffset),

                    // 区域ID
                    AreaId = MemoryHelper.ReadByte(_processHandle, entityAddr + _settings.EntityAreaIdOffset),
                };
                return Pos;
            }
            catch
            {
                return new();
            }
        }
        private string GetSubTypeName(byte entityType, byte subType)
        {
            if (entityType == (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_ITEM)
            {
                return subType switch
                {
                    0x00 => LanguageManager.Get("Furniture"),
                    0x01 => LanguageManager.Get("Pickup"),
                    0x02 => LanguageManager.Get("Weapon"),
                    0x03 => LanguageManager.Get("Vehicle"),
                    0x04 => LanguageManager.Get("PickupSpec"),
                    _ => $"0x{subType:X2}"
                };
            }
            return "";
        }
        private static string GetTypeName(byte entityType)
        {
            string typeName = "ENTITY_TYPE_UNKNOWN";
                // 先验证再转换
            if (Enum.IsDefined(typeof(Settings.ENTITY_TYPE), entityType))
            {
                var enumValue = (Settings.ENTITY_TYPE)Enum.ToObject(typeof(Settings.ENTITY_TYPE), entityType);
                typeName = enumValue.ToString();
            }
            return LanguageManager.Get(typeName);
        }
        private EntityInfo? ReadEntityData(IntPtr entityAddr)
        {
            if (_processHandle == IntPtr.Zero || entityAddr == IntPtr.Zero || _settings == null)
                return null;

            try
            {
                ushort entityId = MemoryHelper.ReadUInt16(_processHandle, entityAddr);
                byte entityType = MemoryHelper.ReadByte(_processHandle, entityAddr + _settings.EntityTypeOffset);
                byte subType = MemoryHelper.ReadByte(_processHandle, entityAddr + 0x03);

                if (!Enum.IsDefined(typeof(Settings.ENTITY_TYPE), entityType))
                    return null;

                int health = MemoryHelper.ReadInt32(_processHandle, entityAddr + _settings.EntityHealthOffset);
                Position pos = ReadEntityPosition(entityAddr);

                // 读取调试属性
                byte noCollide = MemoryHelper.ReadByte(_processHandle, entityAddr + 0x0D);
                byte invisible = MemoryHelper.ReadByte(_processHandle, entityAddr + 0x13);
                byte invincible = MemoryHelper.ReadByte(_processHandle, entityAddr + 0x27A);
                float mass = MemoryHelper.ReadFloat(_processHandle, entityAddr + 0x58);
                float friction = MemoryHelper.ReadFloat(_processHandle, entityAddr + 0x5C);
                byte glow = MemoryHelper.ReadByte(_processHandle, entityAddr + 0x70);
                int aiState = MemoryHelper.ReadInt32(_processHandle, entityAddr + 0x288);
                int aiWait = MemoryHelper.ReadInt32(_processHandle, entityAddr + 0x2A8);

                string typeName = GetTypeName(entityType);
                string subTypeName = GetSubTypeName(entityType, subType);

                return new EntityInfo
                {
                    BaseAddress = entityAddr,
                    EntityId = entityId,
                    EntityType = entityType,
                    TypeName = typeName,
                    SubType = subType,
                    SubTypeName = subTypeName,
                    Health = health,
                    Pos = pos,
                    NoCollide = noCollide,
                    Invisible = invisible,
                    Invincible = invincible,
                    Mass = mass,
                    Friction = friction,
                    Glow = glow,
                    AIState = aiState,
                    AIWait = aiWait,
                };
            }
            catch
            {
                return null;
            }
        }

        // ===== 写入实体坐标 =====
        private bool WriteEntityPosition(IntPtr entityAddr, Position pos)
        {
            if (_processHandle == IntPtr.Zero || entityAddr == IntPtr.Zero || _settings == null)
                return false;

            try
            {
                MemoryHelper.WriteFloat(_processHandle, entityAddr + _settings.EntityPosXOffset, pos.PosX);
                MemoryHelper.WriteFloat(_processHandle, entityAddr + _settings.EntityPosYOffset, pos.PosY);
                MemoryHelper.WriteFloat(_processHandle, entityAddr + _settings.EntityPosZOffset, pos.PosZ);

                MemoryHelper.WriteFloat(_processHandle, entityAddr + _settings.EntityVelXOffset, pos.VelX);
                MemoryHelper.WriteFloat(_processHandle, entityAddr + _settings.EntityVelYOffset, pos.VelY);
                MemoryHelper.WriteFloat(_processHandle, entityAddr + _settings.EntityVelZOffset, pos.VelZ);

                MemoryHelper.WriteInt32(_processHandle, entityAddr + _settings.EntityAreaIdOffset, pos.AreaId);
                return true;
            }catch
            {
                return false;
            }
        }

        // ===== 实体扫描 =====
        private void ScanEntities_Click(object sender, RoutedEventArgs e){
            ScanEntities();
            UpdateAreaFilter();
            ApplyEntityFilter();
        }

        private void ScanEntities()
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            _entityList.Clear();

            try
            {
                IntPtr countAddr = _moduleBase + EntityCountAddressOffset;
                int entityCount = MemoryHelper.ReadInt32(_processHandle, countAddr);

                if (entityCount <= 0 || entityCount > MaxEntityScanCount)
                {
                    EntityStatus.Text = $"{LanguageManager.Get("InvalidEntityCount")}{entityCount}";
                    return;
                }

                EntityStatus.Text = $"{LanguageManager.Get("ScanEntities")}";

                IntPtr poolStart = _moduleBase + (int)_settings.EntityPoolOffset;
                uint foundCount = 0;

                for (int i = 0; i < _settings.EntitySlots && foundCount < entityCount; i++)
                {
                    IntPtr entityAddr = poolStart + i * (int)_settings.EntitySize;
                    ushort entityId = MemoryHelper.ReadUInt16(_processHandle, entityAddr);
                    if (entityId == 0) continue;

                    EntityInfo? entity = ReadEntityData(entityAddr);
                    if (entity == null) continue;

                    entity.Index = i;
                    _entityList.Add(entity);
                    ++foundCount;
                }

                EntityStatus.Text = $"{LanguageManager.Get("Total_Entity")}: {_entityList.Count}";
                EntityCountText?.Text = $"{EntitiesTab}: {_entityList.Count}";
            }
            catch (Exception ex)
            {
                EntityStatus.Text = $"{LanguageManager.Get("ScanError")}{ex.Message}";
            }
        }
        // ===== 实体筛选 =====
        private void UpdateAreaFilter()
        {
            try
            {
                if (EntityAreaFilter == null) return;
                
                EntityAreaFilter.Items.Clear();
                EntityAreaFilter.Items.Add(EntityAllText);
                
                if (_entityList != null && _entityList.Count > 0)
                {
                    var areas = _entityList.Select(e => e.Pos.AreaId).Distinct().OrderBy(a => a).ToList();
                    foreach (var area in areas)
                    {
                        EntityAreaFilter.Items.Add(area.ToString());
                    }
                }
                
                if (EntityAreaFilter.Items.Count > 0)
                {
                    EntityAreaFilter.SelectedIndex = 0;
                }
            }
            catch (Exception ex)
            {
                if (EntityAreaFilter != null)
                {
                    EntityAreaFilter.Items.Clear();
                    EntityAreaFilter.Items.Add(EntityAllText);
                    EntityAreaFilter.SelectedIndex = 0;
                }
                System.Diagnostics.Debug.WriteLine($"{LanguageManager.Get("FilterError")}: {ex.Message}");
            }
        }
        private void ApplyEntityFilter()
        {
            try
            {
                // 确保所有控件都已初始化
                if (EntityTypeFilter == null || EntityAreaFilter == null || EntitySearchBox == null)
                {
                    return;
                }

                // 确保集合不为 null
                if (_filteredEntityList == null)
                {
                    _filteredEntityList = [];
                    EntityDataGrid?.ItemsSource = _filteredEntityList;
                }

                // 使用 SelectedIndex 获取类型筛选（0=All, 1=Human, 2=Zombie, 3=Item, 4=Projectile）
                int typeIndex = EntityTypeFilter.SelectedIndex;
                
                string areaFilter = EntityAreaFilter.SelectedItem?.ToString() ?? "All";
                string searchText = EntitySearchBox.Text?.Trim() ?? "";

                _filteredEntityList.Clear();

                if (_entityList == null || _entityList.Count == 0)
                {
                    EntityCountText?.Text = $"{EntitiesTab}: 0";
                    return;
                }

                var query = _entityList.AsEnumerable();

                // 类型筛选 - 使用索引匹配
                if (typeIndex > 0) // 0 = All
                {
                    byte filterType = typeIndex switch
                    {
                        1 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_HUMAN,
                        2 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_ZOMBIE,
                        3 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_ITEM,
                        4 => (byte)Settings.ENTITY_TYPE.ENTITY_TYPE_PROJECTILE,
                        _ => 0
                    };
                    
                    if (filterType > 0)
                    {
                        query = query.Where(e => e.EntityType == filterType);
                    }
                }

                // 区域筛选
                if (areaFilter != "All" && int.TryParse(areaFilter, out int areaId))
                {
                    query = query.Where(e => e.Pos.AreaId == areaId);
                }

                // 搜索筛选
                if (!string.IsNullOrEmpty(searchText))
                {
                    if (ushort.TryParse(searchText, out ushort searchId))
                    {
                        query = query.Where(e => e.EntityId == searchId);
                    }
                    else
                    {
                        query = query.Where(e => e.TypeName.Contains(searchText, StringComparison.OrdinalIgnoreCase));
                    }
                }

                foreach (var entity in query)
                {
                    _filteredEntityList.Add(entity);
                }

                EntityCountText?.Text = $"{EntitiesTab}: {_filteredEntityList.Count} (filtered from {_entityList.Count})";
            }
            catch (Exception ex)
            {
                EntityStatus?.Text = $"{LanguageManager.Get("FilterError")}: {ex.Message}";
            }
        }
        private void EntityFilter_Changed(object sender, EventArgs e)
        {
            // 确保控件已初始化
            if (EntityTypeFilter == null || EntityAreaFilter == null || EntitySearchBox == null)
            {
                return;
            }
            ApplyEntityFilter();
        }

        private void RefreshEntityList_Click(object sender, RoutedEventArgs e)
        {
            ScanEntities();
            ApplyEntityFilter();
        }

        // ===== 实体选择 =====
        private void EntityDataGrid_SelectedCellsChanged(object sender, SelectedCellsChangedEventArgs e)
        {
            if (EntityDataGrid.SelectedItem is EntityInfo selected)
            {
                _currentEntity = selected;
                UpdateEntityUI(selected);
                EntityStatus.Text = $"{LanguageManager.Get("SelectedEntity")} #{selected.EntityId} ({selected.TypeName})";
            }
        }

        private void SetTarget_Click(object sender, RoutedEventArgs e)
        {
            if (_currentEntity == null)
            {
                TargetStatusText.Text = EntityNoTargetSelected;
                return;
            }

            _targetEntity = _currentEntity;
            TargetStatusText.Text = $"Target: #{_targetEntity.EntityId} ({_targetEntity.TypeName}) at ({_targetEntity.Pos.PosX:F2}, {_targetEntity.Pos.PosY:F2}, {_targetEntity.Pos.PosZ:F2})";
        }

        private void RefreshEntity(EntityInfo? _entity)
        {
            if (_entity == null || !HasProcessContext()) return;

            _entity = ReadEntityData(_entity.BaseAddress);
            UpdateEntityUI(_entity);
            EntityStatus.Text = $"{LanguageManager.Get("RefreshedEntity")} #{_entity.EntityId}";
        }
        // ===== 更新UI =====
        private void UpdateEntityUI(EntityInfo? entity)
        {
            if (entity == null) return;
            EntityIdBox.Text = entity.EntityId.ToString();
            EntityTypeBox.Text = entity.TypeName;
            EntitySubTypeBox.Text = entity.SubTypeName;
            EntityHPBox.Value = entity.Health;
            EntityAreaIdBox.Text = entity.Pos.AreaId.ToString();
            
            EntityPosXBox.Value = entity.Pos.PosX;
            EntityPosYBox.Value = entity.Pos.PosY;
            EntityPosZBox.Value = entity.Pos.PosZ;
            EntityVelXBox.Value = entity.Pos.VelX;
            EntityVelYBox.Value = entity.Pos.VelY;
            EntityVelZBox.Value = entity.Pos.VelZ;
            
            // 调试属性
            EntityNoCollide.IsChecked = entity.NoCollide == 1;
            EntityInvisible.IsChecked = entity.Invisible == 1;
            EntityInvincible.IsChecked = entity.Invincible == 1;
            EntityMass.Value = entity.Mass;
            EntityFriction.Value = entity.Friction;
            EntityGlow.IsChecked = entity.Glow == 1;
            EntityAIState.Value = entity.AIState;
            EntityAIWait.Value = entity.AIWait;
        }
        private void ApplyEntityPosition(EntityInfo? entity)
        {
            if (entity == null || _processHandle == IntPtr.Zero || _settings == null) return;
            try
            {
                IntPtr addr = entity.BaseAddress;
                
                float x = (float)EntityPosXBox.Value;
                float y = (float)EntityPosYBox.Value;
                float z = (float)EntityPosZBox.Value;
                float vx = (float)EntityVelXBox.Value;
                float vy = (float)EntityVelYBox.Value;
                float vz = (float)EntityVelZBox.Value;

                MemoryHelper.WriteFloat(_processHandle, addr + _settings.EntityPosXOffset, x);
                MemoryHelper.WriteFloat(_processHandle, addr + _settings.EntityPosYOffset, y);
                MemoryHelper.WriteFloat(_processHandle, addr + _settings.EntityPosZOffset, z);
                MemoryHelper.WriteFloat(_processHandle, addr + _settings.EntityVelXOffset, vx);
                MemoryHelper.WriteFloat(_processHandle, addr + _settings.EntityVelYOffset, vy);
                MemoryHelper.WriteFloat(_processHandle, addr + _settings.EntityVelZOffset, vz);

                entity.Pos.PosX = x;
                entity.Pos.PosY = y;
                entity.Pos.PosZ = z;
                entity.Pos.VelX = vx;
                entity.Pos.VelY = vy;
                entity.Pos.VelZ = vz;

                EntityStatus.Text = $"{LanguageManager.Get("PositionUpdated")} #{entity.EntityId}";
                UpdateEntityUI(entity);
            }
            catch (Exception ex)
            {
                EntityStatus.Text = $"{Error}{ex.Message}";
            }
        }
        private void EntityHealth_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (sender is NumericUpDown num && !num.IsKeyboardFocusWithin) return;
            if (_currentEntity == null || _processHandle == IntPtr.Zero) return;
            MemoryHelper.WriteInt32(_processHandle, _currentEntity.BaseAddress + _settings.EntityHealthOffset, (int)EntityHPBox.Value);
        }
        private void EntityPos_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (_currentEntity == null || _processHandle == IntPtr.Zero) return;
            ApplyEntityPosition(_currentEntity);
        }
        private void DebugProp_Changed(object sender, RoutedEventArgs e)
        {
            ApplyEntityDebug();
        }
        private void ApplyEntityDebug()
        {
            if (_currentEntity == null || _processHandle == IntPtr.Zero || _settings == null) return;

            try
            {
                IntPtr addr = _currentEntity.BaseAddress;

                byte noCollide = (byte)(EntityNoCollide.IsChecked == true ? 1 : 0);
                MemoryHelper.WriteByte(_processHandle, addr + 0x0D, noCollide);
                _currentEntity.NoCollide = noCollide;

                byte invisible = (byte)(EntityInvisible.IsChecked == true ? 1 : 0);
                MemoryHelper.WriteByte(_processHandle, addr + 0x13, invisible);
                _currentEntity.Invisible = invisible;

                byte invincible = (byte)(EntityInvincible.IsChecked == true ? 1 : 0);
                MemoryHelper.WriteByte(_processHandle, addr + 0x27A, invincible);
                _currentEntity.Invincible = invincible;

                byte glow = (byte)(EntityGlow.IsChecked == true ? 1 : 0);
                MemoryHelper.WriteByte(_processHandle, addr + 0x70, glow);
                _currentEntity.Glow = glow;

                float mass = (float)EntityMass.Value;
                MemoryHelper.WriteFloat(_processHandle, addr + 0x58, mass);
                _currentEntity.Mass = mass;

                float friction = (float)EntityFriction.Value;
                MemoryHelper.WriteFloat(_processHandle, addr + 0x5C, friction);
                _currentEntity.Friction = friction;

                int aiState = (int)EntityAIState.Value;
                MemoryHelper.WriteInt32(_processHandle, addr + 0x288, aiState);
                _currentEntity.AIState = aiState;

                int aiWait = (int)EntityAIWait.Value;
                MemoryHelper.WriteInt32(_processHandle, addr + 0x2A8, aiWait);
                _currentEntity.AIWait = aiWait;

                EntityStatus.Text = $"Debug props applied to #{_currentEntity.EntityId}";
            }
            catch (Exception ex)
            {
                EntityStatus.Text = $"Error: {ex.Message}";
            }
        }
        private void DebugNumeric_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ApplyEntityDebug();
        }
        // ===== 应用实体修改 =====
        private bool BothEntityIsVaild()
        {
            if (_processHandle == IntPtr.Zero)
            {
                MessageBox.Show(_statusText);
                return true;
            }else if (_currentEntity == null)
            {
                EntityStatus.Text = EntityNoSourceSelected;
                return true;
            }else if (_targetEntity == null)
            {
                EntityStatus.Text = EntityNoTargetSelected;
                return true;
            }else if (_currentEntity.EntityId == _targetEntity.EntityId)
            {
                EntityStatus.Text = LanguageManager.Get("SameSourceAndTarget");
                return true;
            }
            return false;
        }
        private void TeleportToTarget_Click(object sender, RoutedEventArgs e)
        {
            if (BothEntityIsVaild()) return;
            try
            {
                Position pos = ReadEntityPosition(_targetEntity.BaseAddress);
                if (WriteEntityPosition(_currentEntity.BaseAddress, pos))
                {
                    EntityStatus.Text = LanguageManager.Get("Teleported");
                    RefreshEntity(_currentEntity);
                    ApplyEntityFilter();
                }else
                {
                    EntityStatus.Text = LanguageManager.Get("FailWritePos");
                }
            }
            catch (Exception ex)
            {
                EntityStatus.Text = $"{LanguageManager.Get("TeleportError")}{ex.Message}";
            }
        }
        private void SwapPositions_Click(object sender, RoutedEventArgs e)
        {
            if (BothEntityIsVaild()) return;
            try
            {
                IntPtr srcAddr = _currentEntity.BaseAddress;
                IntPtr dstAddr = _targetEntity.BaseAddress;
                Position srcp = ReadEntityPosition(srcAddr);
                Position dstp = ReadEntityPosition(dstAddr);
                if (WriteEntityPosition(srcAddr, dstp) && WriteEntityPosition(dstAddr, srcp))
                {
                    EntityStatus.Text = LanguageManager.Get("Swapped");
                    RefreshEntity(_currentEntity);
                    RefreshEntity(_targetEntity);
                    ApplyEntityFilter();
                }
                else
                {
                    EntityStatus.Text = LanguageManager.Get("FailWritePos");
                }
            }
            catch (Exception ex)
            {
                EntityStatus.Text = $"{LanguageManager.Get("TeleportError")}{ex.Message}";
            }
        }
        // ===== 扫描武器池 =====
        private string TryGetWeaponName(int WeaponId)
        {
            return WeaponId == 0 ? LanguageManager.Get("Empty") : _weaponManager.GetWeaponName(WeaponId);
        }
        private void ScanWeaponPool_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            int count = _weaponManager.ScanWeapons();
            WeaponCountText.Text = $"{WeaponsTab}: {count}";
            SetStatus($"{count} {WeaponsTab}");
            
            // 武器池扫描完成后，自动刷新当前角色的武器槽
            if (count > 0 && _characterList.Count > 0)
            {
                RefreshWeaponSlots_Click(sender, e);
            }
        }
        private void ApplyCharacterWeaponSlots(CharacterWeapon weapons)
        {
            var slots = _weaponSlotBindings;
            var slotValues = new[]
            {
                new { Slot = slots.Count > 0 ? slots[0] : null, Weapon = weapons.WeaponA, Stack = weapons.StackA, Locked = weapons.NoDropA == 1 },
                new { Slot = slots.Count > 1 ? slots[1] : null, Weapon = weapons.WeaponB, Stack = weapons.StackB, Locked = weapons.NoDropB == 1 },
                new { Slot = slots.Count > 2 ? slots[2] : null, Weapon = weapons.WeaponC, Stack = weapons.StackC, Locked = weapons.NoDropC == 1 }
            };

            foreach (var entry in slotValues)
            {
                if (entry.Slot == null) continue;

                entry.Slot.WeaponId = (int)entry.Weapon;
                entry.Slot.StackCount = (int)entry.Stack;
                entry.Slot.IsLocked = entry.Locked;
                entry.Slot.WeaponName = TryGetWeaponName((int)entry.Weapon);
            }

            WeaponCharInfo.Text = $"{NameBox.Text} (Index: {_currentCharacterIndex})";
        }
        private void ApplyStorageWeaponSlots()
        {
            try
            {
                IntPtr storageAddr = _moduleBase + (int)_settings.StorageWeaponOffset;
                uint slotSize = _settings.StorageWeaponSize;

                for (int i = 0; i < _settings.StorageWeaponSlots && i < _storageWeapons.Count; i++)
                {
                    IntPtr slotAddr = (IntPtr)(storageAddr + i * slotSize);
                    uint weaponId = MemoryHelper.ReadUInt32(_processHandle, slotAddr);
                    uint count = MemoryHelper.ReadUInt32(_processHandle, slotAddr + 4);

                    _storageWeapons[i].WeaponId = (int)weaponId;
                    _storageWeapons[i].Count = (int)count;
                    _storageWeapons[i].WeaponName = TryGetWeaponName((int)weaponId);
                }
            }
            catch (Exception ex)
            {
                SetStatus($"Error reading storage weapons: {ex.Message}");
            }
        }

        // ===== 刷新武器槽 (只刷新左侧，不重新扫描武器池) =====
        private void RefreshWeaponSlots_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }
            if (TryGetCurrentPlayerBase(out IntPtr playerBase))
            {
                var weapons = _weaponManager.ReadCharacterWeapons(playerBase);
                ApplyCharacterWeaponSlots(weapons);
            }

            ApplyStorageWeaponSlots();
            SetStatus(LanguageManager.Get("Refreshed"));
        }
        
        private void WeaponIdTextBox_LostFocus(object sender, RoutedEventArgs e)
        {
            if (sender is TextBox textBox && textBox.DataContext is WeaponSlotBinding slot)
            {
                slot.WeaponName = TryGetWeaponName(slot.WeaponId);
                ApplyAllSlots_Click(sender, e);
            }
        }

        private void StackCount_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ApplyAllSlots_Click(sender, e);
        }

        // ===== 锁定状态变化 =====
        private void LockedState_Changed(object sender, RoutedEventArgs e)
        {
            // 自动应用到游戏
            ApplyAllSlots_Click(sender, e);
        }

        // ===== 应用所有武器槽 =====
        private void ApplyAllSlots_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            if (!TryGetCurrentPlayerBase(out IntPtr playerBase))
            {
                SetStatus(NoPlayerSelectedText);
                return;
            }

            var slots = _weaponSlotBindings;
            var weapons = new CharacterWeapon
            {
                WeaponA = (uint)slots[0].WeaponId,
                StackA = (uint)slots[0].StackCount,
                NoDropA = (uint)(slots[0].IsLocked ? 1 : 0),
                
                WeaponB = (uint)slots[1].WeaponId,
                StackB = (uint)slots[1].StackCount,
                NoDropB = (uint)(slots[1].IsLocked ? 1 : 0),
                
                WeaponC = (uint)slots[2].WeaponId,
                StackC = (uint)slots[2].StackCount,
                NoDropC = (uint)(slots[2].IsLocked ? 1 : 0)
            };

            if (_weaponManager.WriteAllWeaponSlots(playerBase, weapons))
            {
                SetStatus(LanguageManager.Get("WeaponApplySuccess"));
                RefreshWeaponSlots_Click(sender, e);
            }
            else
            {
                SetStatus(LanguageManager.Get("WeaponApplyFailed"));
            }
        }

        // ===== 武器搜索 =====
        private void WeaponSearchBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            string searchText = WeaponSearchBox.Text?.Trim().ToLower() ?? "";
            
            if (string.IsNullOrEmpty(searchText))
            {
                WeaponPoolGrid.ItemsSource = _weaponManager.WeaponList;
                WeaponCountText.Text = $"{WeaponsTab}: {_weaponManager.WeaponList.Count}";
                return;
            }

            var filtered = _weaponManager.WeaponList
                .Where(w => w.Name.ToLower().Contains(searchText) || w.Id.ToString().Contains(searchText))
                .ToList();
            
            WeaponPoolGrid.ItemsSource = filtered;
            WeaponCountText.Text = $"{WeaponsTab}: {filtered.Count} ({LanguageManager.Get("Filtered")})";
        }

        // ===== 读取仓库武器 =====
        private void ReadStorageWeapons_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            try
            {
                ApplyStorageWeaponSlots();
                SetStatus(ReadText);
            }
            catch (Exception ex)
            {
                SetStatus($"Error reading storage weapons: {ex.Message}");
            }
        }

        // ===== 应用仓库武器 =====
        private void ApplyStorageWeapons_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            try
            {
                IntPtr storageAddr = _moduleBase + (int)_settings.StorageWeaponOffset;
                uint slotSize = _settings.StorageWeaponSize;
                
                for (int i = 0; i < _settings.StorageWeaponSlots && i < _storageWeapons.Count; i++)
                {
                    IntPtr slotAddr = (IntPtr)(storageAddr + i * slotSize);
                    MemoryHelper.WriteUInt32(_processHandle, slotAddr, (uint)_storageWeapons[i].WeaponId);
                    MemoryHelper.WriteUInt32(_processHandle, slotAddr + 4, (uint)_storageWeapons[i].Count);
                }
                
                SetStatus(ApplyText);
                ReadStorageWeapons_Click(sender, e); // 刷新显示
            }
            catch (Exception ex)
            {
                SetStatus($"Error applying storage weapons: {ex.Message}");
            }
        }

        // ===== 仓库武器ID失去焦点 =====
        private void StorageWeaponId_LostFocus(object sender, RoutedEventArgs e)
        {
            if (sender is TextBox textBox && textBox.DataContext is StorageWeaponSlot slot)
            {
                slot.WeaponName = TryGetWeaponName(slot.WeaponId);
                ApplyStorageWeapons_Click(sender, e);
            }
        }

        // ===== 仓库数量变化 =====
        private void StorageCount_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ApplyStorageWeapons_Click(sender, e);
        }

        // ===== 全局资源偏移 =====
        private void ReadResourceOff(ref ObservableCollection<ResourceEntry> _resources, IntPtr resAddr)
        {
            for (int i = 0; i < 6; ++i){
                _resources[i].Value = (int)MemoryHelper.ReadUInt32(_processHandle,
                    resAddr + 0x04 * (i + 1));  // food
            }
        }
        private void WriteResourceOff(ref ObservableCollection<ResourceEntry> _resources, IntPtr resAddr)
        {
            for (int i = 0; i < 6; ++i){
                MemoryHelper.WriteUInt32(_processHandle,
                    resAddr + 0x04 * (i + 1),
                    (uint)_resources[i].Value);  // food
            }
        }
        // ===== 读取全局资源 =====
        private void ReadGlobalResources_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            try
            {
                IntPtr storageAddr = _moduleBase + (int)_settings.StorageOffset;
                
                ReadResourceOff(ref _globalResources, storageAddr);
                
                SetStatus("Read global resources");
            }
            catch (Exception ex)
            {
                SetStatus($"Error reading global resources: {ex.Message}");
            }
        }

        // ===== 应用全局资源 =====
        private void ApplyGlobalResources_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            try
            {
                IntPtr storageAddr = _moduleBase + (int)_settings.StorageOffset;
                
                WriteResourceOff(ref _globalResources, storageAddr);
                
                SetStatus("Applied global resources");
            }
            catch (Exception ex)
            {
                SetStatus($"Error applying global resources: {ex.Message}");
            }
        }

        // ===== 读取玩家资源 =====
        private void ReadPlayerResources_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            if (!TryGetCurrentPlayerBase(out IntPtr playerBase))
            {
                SetStatus(NoPlayerSelectedText);
                return;
            }

            try
            {
                IntPtr resourceAddr = playerBase + Settings.ResourceOffset;
                
                ReadResourceOff(ref _playerResources, resourceAddr);
                
                ResourceCharInfo.Text = $"{NameBox.Text} (Index: {_currentCharacterIndex})";
                SetStatus("Read player resources");
            }
            catch (Exception ex)
            {
                SetStatus($"Error reading player resources: {ex.Message}");
            }
        }

        // ===== 应用玩家资源 =====
        private void ApplyPlayerResources_Click(object sender, RoutedEventArgs e)
        {
            if (!HasProcessContext())
            {
                MessageBox.Show(_statusText);
                return;
            }

            if (!TryGetCurrentPlayerBase(out IntPtr playerBase))
            {
                SetStatus(NoPlayerSelectedText);
                return;
            }

            try
            {
                IntPtr resourceAddr = playerBase + Settings.ResourceOffset;
                
                WriteResourceOff(ref _playerResources, resourceAddr);
                
                SetStatus("Applied player resources");
            }
            catch (Exception ex)
            {
                SetStatus($"Error applying player resources: {ex.Message}");
            }
        }
        
        // ===== 高级 =====
        readonly byte[] nopBytes = [0x90, 0x90];
        private void ChangeMax_Click(object sender, RoutedEventArgs e)
        {
            if (_processHandle == IntPtr.Zero)
            {
                MessageBox.Show(_statusText);
                return;
            }
            // if (!int.TryParse(NewMaxBox.Text, out int newMax) || newMax < 0)
            // {
            //     MaxStatus.Text = "Invalid value.";
            //     return;
            // }
            IntPtr jgAddr = _moduleBase + 0x27ee4;
            // 修改保护
            MemoryHelper.VirtualProtectEx(_processHandle, jgAddr, (uint)nopBytes.Length, 0x40, out uint oldProtect);
            MemoryHelper.WriteBytes(_processHandle, jgAddr, nopBytes);
            MemoryHelper.VirtualProtectEx(_processHandle, jgAddr, (uint)nopBytes.Length, oldProtect, out _);

            MessageBox.Show(
                LanguageManager.Get("Junk01"),
                "Attention",
                MessageBoxButton.OK,
                MessageBoxImage.Warning
            );
            MessageBox.Show(
                LanguageManager.Get("Junk02"),
                "Attention",
                MessageBoxButton.OK,
                MessageBoxImage.Question
            );
            MaxStatus.Text = LanguageManager.Get("Applied");
        }

        private void OpenSettings_Click(object sender, RoutedEventArgs e)
        {
            var settingsWin = new SettingsWindow(_settings);
            settingsWin.Owner = this;
            if (settingsWin.ShowDialog() == true)
            {
                _settings = settingsWin.UpdatedSettings;
                _settings.Save();

                if (!string.Equals(LanguageManager.CurrentLanguage, _settings.Language, StringComparison.OrdinalIgnoreCase))
                {
                    MessageBox.Show(
                        "Language changes will take effect after restarting the program.",
                        "Settings",
                        MessageBoxButton.OK,
                        MessageBoxImage.Information);
                }
            }
        }
    }
}