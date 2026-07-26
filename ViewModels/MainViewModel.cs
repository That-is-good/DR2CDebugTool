using DR2CDebugTool.Models;
using DR2CDebugTool.Services;
using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Windows;

namespace DR2CDebugTool.ViewModels
{
    public class MainViewModel : BaseViewModel
    {
        public MemoryService Memory { get; }
        public PlayerViewModel Player { get; }
        public EntityViewModel Entity { get; }
        public WeaponViewModel Weapon { get; }
        public ResourceViewModel Resource { get; }
        public AdvancedViewModel Advanced { get; }

        // Process management
        public ObservableCollection<ProcessInfo> AllProcesses { get; } = [];
        public ObservableCollection<ProcessInfo> FilteredProcesses { get; } = [];

        private string _processFilter;
        public string ProcessFilter
        {
            get => _processFilter;
            set { SetProperty(ref _processFilter, value); ApplyProcessFilter(); }
        }

        private ProcessInfo? _selectedProcess;
        public ProcessInfo? SelectedProcess
        {
            get => _selectedProcess;
            set => SetProperty(ref _selectedProcess, value);
        }

        // Status
        private string _statusText = "";
        public string StatusText
        {
            get => _statusText;
            set => SetProperty(ref _statusText, value);
        }

        // ===== 本地化字符串（词元复用） =====
        public string TitleText => LanguageManager.Get("MainWindow_Title");
        public string RefreshText => LanguageManager.Get("Refresh");
        public string AttachText => LanguageManager.Get("Attach");
        public string SettingsText => LanguageManager.Get("Settings");
        public string ReadText => LanguageManager.Get("Read");
        public string WriteText => LanguageManager.Get("Write");
        public string ApplyText => LanguageManager.Get("Apply");
        public string SearchText => LanguageManager.Get("Search");
        public string Error => LanguageManager.Get("Error");
        public string NewMaxText => LanguageManager.Get("NewMaxText");

        // 选项卡
        public string PlayerStatsTab => LanguageManager.Get("PlayerStats");
        public string EntitiesTab => LanguageManager.Get("Entities");
        public string WeaponsTab => LanguageManager.Get("Weapons");
        public string ResourcesTab => LanguageManager.Get("Resources");
        public string AdvancedTab => LanguageManager.Get("Advanced");

        // 角色面板
        public string CharacterText => LanguageManager.Get("Character");
        public string ScanCharaText => LanguageManager.Get("ScanCharacters");
        public string PerkLabel => LanguageManager.Get("Perk");
        public string TraitLabel => LanguageManager.Get("Trait");
        public string EntityNameText => LanguageManager.Get("Name");
        public string EntityHPText => LanguageManager.Get("Health");
        public string NoPlayerSelectedText => LanguageManager.Get("NoPlayerSelected");
        public string CurrentCharacterText => LanguageManager.Get("CurrentCharacter");

        // 属性表头
        public string AttributeHeader => LanguageManager.Get("Attribute");
        public string BaseHeader => LanguageManager.Get("Col_Base");
        public string BonusHeader => LanguageManager.Get("Col_Bonus");
        public string EffectiveHeader => LanguageManager.Get("Col_Effective");
        public string KnownHeader => LanguageManager.Get("Known");

        // 实体面板
        public string ScanEntitiesText => LanguageManager.Get("ScanEntity");
        public string TypeText => LanguageManager.Get("Type");
        public string SubTypeText => LanguageManager.Get("SubType");
        public string AreaText => LanguageManager.Get("Area");
        public string PosText => LanguageManager.Get("Pos");
        public string VelText => LanguageManager.Get("Vel");
        public string PositionText => LanguageManager.Get("PositionText");
        public string BasicInfoText => LanguageManager.Get("BasicInfoText");
        public string DebugPropertiesText => LanguageManager.Get("DebugPropertiesText");
        public string AIPropertiesText => LanguageManager.Get("AIPropertiesText");
        public string NoCollideText => LanguageManager.Get("NoCollide");
        public string InvisibleText => LanguageManager.Get("Invisible");
        public string InvincibleText => LanguageManager.Get("Invincible");
        public string GlowText => LanguageManager.Get("Glow");
        public string MassText => LanguageManager.Get("Mass");
        public string FrictionText => LanguageManager.Get("Friction");
        public string AIStateText => LanguageManager.Get("AIState");
        public string AIWaitText => LanguageManager.Get("AIWait");
        public string EntityNoTargetSelected => LanguageManager.Get("NoTargetEntity");
        public string EntityNoSourceSelected => LanguageManager.Get("NoSourceEntity");
        public string EntityAllText => LanguageManager.Get("ENTITY_TYPE_ALL");
        public string EntityHumanText => LanguageManager.Get("ENTITY_TYPE_HUMAN");
        public string EntityZombieText => LanguageManager.Get("ENTITY_TYPE_ZOMBIE");
        public string EntityItemText => LanguageManager.Get("ENTITY_TYPE_ITEM");
        public string EntityProjectileText => LanguageManager.Get("ENTITY_TYPE_PROJECTILE");
        public string EntityFurnitureText => LanguageManager.Get("Furniture");
        public string EntityPickupText => LanguageManager.Get("Pickup");
        public string EntityVehicleText => LanguageManager.Get("Vehicle");
        public string EntityPickupSpecText => LanguageManager.Get("PickupSpec");
        public string TeleportToTargetText => LanguageManager.Get("TeleportToTarget");
        public string SwapPositionsText => LanguageManager.Get("SwapPositions");
        public string SetTargetText => LanguageManager.Get("SetTarget");

        // 实体表格列头（组合词）
        public string ColEntityID => LanguageManager.Get("ID");
        public string ColEntityType => LanguageManager.Get("Type");
        public string ColEntitySubType => LanguageManager.Get("SubType");
        public string ColEntityHP => LanguageManager.Get("Health");
        public string ColEntityArea => LanguageManager.Get("Area");
        public string ColEntityPosX => LanguageManager.Combine("Pos", "X");
        public string ColEntityPosY => LanguageManager.Combine("Pos", "Y");
        public string ColEntityPosZ => LanguageManager.Combine("Pos", "Z");
        public string ColEntityAddress => LanguageManager.Get("Address");

        // 武器面板
        public string ScanWeaponPoolText => LanguageManager.Get("ScanWeaponPool");
        public string RefreshWeaponText => LanguageManager.Get("WeaponRefresh");
        public string WeaponSoltText => LanguageManager.Get("WeaponSlot");
        public string WeaponNameText => LanguageManager.Combine("Weapons", "Name");
        public string StackText => LanguageManager.Get("Stack");
        public string LockedText => LanguageManager.Get("NoDrop");
        public string StorageWeaponText => LanguageManager.Get("StorageWeapon");

        // 武器表格列头
        public string ColSlot => LanguageManager.Get("Slot");
        public string ColWeaponID => LanguageManager.Combine("Weapons", "ID");
        public string ColWeaponName => LanguageManager.Combine("Weapons", "Name");
        public string ColWeaponStack => LanguageManager.Get("Stack");
        public string ColWeaponLocked => LanguageManager.Get("NoDrop");
        public string ColStorageIndex => "#";
        public string ColStorageWeaponID => LanguageManager.Combine("Weapons", "ID");
        public string ColStorageWeaponName => LanguageManager.Combine("Weapons", "Name");
        public string ColStorageStack => LanguageManager.Get("Stack");
        public string ColPoolID => LanguageManager.Get("ID");
        public string ColPoolName => LanguageManager.Get("Name");

        // 资源面板
        public string GlobalResourcesText => LanguageManager.Get("GlobalResources");
        public string PlayerResourcesText => LanguageManager.Get("PlayerResources");
        public string ColResourceName => LanguageManager.Get("Name");
        public string ColResourceValue => LanguageManager.Get("Stack");

        // Window control
        public Window? OwnerWindow { get; set; }

        public MainViewModel()
        {
            // 必须先加载设置，确定语言，再创建子ViewModel（InitializeStats需要正确的语言）
            var settings = Settings.Load();
            LanguageManager.CurrentLanguage = settings.Language;
            Memory = new MemoryService();
            Memory.LoadSettings(settings);

            Player = new PlayerViewModel(Memory);
            Entity = new EntityViewModel(Memory);
            Weapon = new WeaponViewModel(Memory);
            Resource = new ResourceViewModel(Memory);
            Advanced = new AdvancedViewModel(Memory);

            //ProcessFilter = "prog";
            StatusText = LanguageManager.Get("NotAttached");
            int storageSlots = Memory.Settings != null && Memory.Settings.StorageWeaponSlots > 0 ? (int)Memory.Settings.StorageWeaponSlots : 15;
            Weapon.InitializeStorage(storageSlots);

            Memory.ConnectionChanged += OnConnectionChanged;
        }

        public void LoadSettings()
        {
            var settings = Settings.Load();
            Memory.LoadSettings(settings);
            LanguageManager.CurrentLanguage = settings.Language;
        }

        private void OnConnectionChanged()
        {
            if (Memory.IsReady)
            {
                StatusText = $"{LanguageManager.Get("Attached")} {Memory.ProcessId} - Base: {Memory.ModuleBase.ToInt64():X}";
                Player.RefreshOffsetInfo();
                Advanced.RefreshOffsetInfo();

                Player.ScanCharacters();
                if (Player.CharacterList.Count > 0){
                    Player.SelectCharacter(0);
                }

                Entity.ScanEntities();
            }
            else
            {
                StatusText = LanguageManager.Get("NotAttached");
            }
        }

        public void NotifyLanguageChanged()
        {
            // 通知所有本地化属性更新
            foreach (var prop in typeof(MainViewModel).GetProperties())
            {
                if (prop.CanRead && prop.Name.EndsWith("Text") || prop.Name.EndsWith("Header") || prop.Name.StartsWith("Col") || prop.Name.EndsWith("Tab"))
                {
                    OnPropertyChanged(prop.Name);
                }
            }
            // 额外通知
            OnPropertyChanged(nameof(TitleText));
            OnPropertyChanged(nameof(Error));
            OnPropertyChanged(nameof(NewMaxText));
            OnPropertyChanged(nameof(NoPlayerSelectedText));
            OnPropertyChanged(nameof(CurrentCharacterText));
            OnPropertyChanged(nameof(EntityNoTargetSelected));
            OnPropertyChanged(nameof(EntityNoSourceSelected));
            OnPropertyChanged(nameof(EntityAllText));
            OnPropertyChanged(nameof(EntityHumanText));
            OnPropertyChanged(nameof(EntityZombieText));
            OnPropertyChanged(nameof(EntityItemText));
            OnPropertyChanged(nameof(EntityProjectileText));
            OnPropertyChanged(nameof(ColStorageIndex));
        }

        public void RefreshProcesses()
        {
            AllProcesses.Clear();
            var allProcs = Process.GetProcesses();
            foreach (var p in allProcs.OrderBy(p => p.ProcessName))
            {
                try { AllProcesses.Add(new ProcessInfo { Id = p.Id, Name = p.ProcessName }); }
                catch { }
            }
            ApplyProcessFilter();
        }

        private void ApplyProcessFilter()
        {
            string filter = ProcessFilter?.Trim().ToLower() ?? "";
            FilteredProcesses.Clear();
            foreach (var p in AllProcesses)
            {
                if (string.IsNullOrEmpty(filter) || p.Name?.ToLower().Contains(filter) == true)
                    FilteredProcesses.Add(p);
            }
            if (FilteredProcesses.Count > 0)
                SelectedProcess = FilteredProcesses[0];
        }

        public void AttachProcess()
        {
            if (SelectedProcess == null) return;
            if (Memory.Attach(SelectedProcess.Id)) { }
            else
            {
                StatusText = LanguageManager.Get("FailedOpenProcess");
            }
        }

        public void OpenSettings()
        {
            var settingsWin = new SettingsWindow(Memory.Settings ?? new Settings())
            {
                Owner = OwnerWindow
            };
            if (settingsWin.ShowDialog() == true)
            {
                Memory.LoadSettings(settingsWin.UpdatedSettings);
                settingsWin.UpdatedSettings.Save();

                if (Memory.Settings != null && !string.Equals(LanguageManager.CurrentLanguage, Memory.Settings.Language, StringComparison.OrdinalIgnoreCase))
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