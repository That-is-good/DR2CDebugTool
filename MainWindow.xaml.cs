using DR2CDebugTool.ViewModels;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace DR2CDebugTool
{
    public partial class MainWindow : Window
    {
        public MainViewModel ViewModel { get; }

        public MainWindow()
        {
            InitializeComponent();

            ViewModel = new MainViewModel();
            ViewModel.OwnerWindow = this;
            DataContext = ViewModel;

            ViewModel.RefreshProcesses();
        }

        ~MainWindow()
        {
            ViewModel.Memory.Detach();
        }

        // ===== 进程管理 =====
        private void RefreshProcesses_Click(object sender, RoutedEventArgs e) => ViewModel.RefreshProcesses();
        private void ProcessFilterBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (sender is TextBox tb)
                ViewModel.ProcessFilter = tb.Text;
        }
        private void Attach_Click(object sender, RoutedEventArgs e) => ViewModel.AttachProcess();
        private void Window_Loaded(object sender, RoutedEventArgs e) { }

        // ===== 角色 =====
        private void ScanCharacters_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Player.ScanCharacters();
        }

        private void CharacterListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (CharacterListBox.SelectedItem is Models.CharacterInfo selected)
            {
                ViewModel.Player.SelectCharacter(selected.Index);
                ViewModel.Weapon.RefreshWeaponSlots(selected.Index, ViewModel.Player.PlayerName);
            }
        }

        private void NameBox_OnKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
                ViewModel.Player.WritePlayerName();
        }

        private void CharacterHealth_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Player.WritePlayerHealth();
            ViewModel.Player.ReadPlayerInfo();
        }

        private void BaseValue_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Player.WriteBaseStats();
        }

        private void BonusValue_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Player.WriteBonusStats();
        }

        private void ReadPlayerStats_Click(object sender, RoutedEventArgs e) => ViewModel.Player.ReadPlayerStats();

        // ===== 实体 =====
        private void ScanEntities_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Entity.ScanEntities();
        }

        private void RefreshEntityList_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Entity.ScanEntities();
        }

        private void EntityDataGrid_SelectedCellsChanged(object sender, SelectedCellsChangedEventArgs e)
        {
            if (EntityDataGrid.SelectedItem is Models.EntityInfo selected)
                ViewModel.Entity.SelectEntity(selected);
        }

        private void EntityFilter_Changed(object sender, System.EventArgs e)
        {
            // Filtering is handled via binding in the ViewModel
        }

        private void EntityHealth_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Entity.ApplyEntityHealth();
        }

        private void EntityPos_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Entity.ApplyEntityPosition();
        }

        private void DebugProp_Changed(object sender, RoutedEventArgs e)
        {
            // Handled via property setters in EntityViewModel
        }

        private void DebugNumeric_Changed(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            // Handled via property setters in EntityViewModel
        }

        private void SetTarget_Click(object sender, RoutedEventArgs e) => ViewModel.Entity.SetTarget();
        private void TeleportToTarget_Click(object sender, RoutedEventArgs e) => ViewModel.Entity.TeleportToTarget();
        private void SwapPositions_Click(object sender, RoutedEventArgs e) => ViewModel.Entity.SwapPositions();

        // ===== 武器 =====
        private void ScanWeaponPool_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Weapon.ScanWeaponPool();
            if (ViewModel.Player.CharacterList.Count > 0)
                ViewModel.Weapon.RefreshWeaponSlots(ViewModel.Player.CurrentCharacterIndex, ViewModel.Player.PlayerName);
        }

        private void RefreshWeaponSlots_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Weapon.RefreshWeaponSlots(ViewModel.Player.CurrentCharacterIndex, ViewModel.Player.PlayerName);
        }

        private void Known_Changed(object sender, RoutedEventArgs e)
        {
            ViewModel.Player.WriteKnownFlags();
        }

        private void EntityDebugCheckBox_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Entity.ApplyEntityDebug();
        }

        private void WeaponId_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (sender is Controls.NumericUpDown num && num.DataContext is Helpers.WeaponSlotBinding slot)
            {
                slot.WeaponName = slot.WeaponId == 0
                    ? Models.LanguageManager.Get("Empty")
                    : ViewModel.Memory.WeaponManager.GetWeaponName(slot.WeaponId);
                ViewModel.Weapon.ApplyAllSlots(ViewModel.Player.CurrentCharacterIndex);
            }
        }

        private void StorageWeaponId_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (sender is Controls.NumericUpDown num && num.DataContext is Models.StorageWeaponSlot slot)
            {
                slot.WeaponName = slot.WeaponId == 0
                    ? Models.LanguageManager.Get("Empty")
                    : ViewModel.Memory.WeaponManager.GetWeaponName(slot.WeaponId);
                ViewModel.Weapon.ApplyStorageWeapons();
            }
        }

        private void StackCount_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Weapon.ApplyAllSlots(ViewModel.Player.CurrentCharacterIndex);
        }

        private void LockedState_Changed(object sender, RoutedEventArgs e)
        {
            ViewModel.Weapon.ApplyAllSlots(ViewModel.Player.CurrentCharacterIndex);
        }

        private void WeaponSearchBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (sender is TextBox tb)
                ViewModel.Weapon.WeaponSearchText = tb.Text;
        }

        private void ReadStorageWeapons_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Weapon.ApplyStorageWeaponSlots();
        }

        private void ApplyStorageWeapons_Click(object sender, RoutedEventArgs e)
        {
            ViewModel.Weapon.ApplyStorageWeapons();
        }

        private void StorageWeaponId_LostFocus(object sender, RoutedEventArgs e)
        {
            if (sender is TextBox textBox && textBox.DataContext is Models.StorageWeaponSlot slot)
            {
                slot.WeaponName = slot.WeaponId == 0
                    ? Models.LanguageManager.Get("Empty")
                    : ViewModel.Memory.WeaponManager.GetWeaponName(slot.WeaponId);
                ViewModel.Weapon.ApplyStorageWeapons();
            }
        }

        private void StorageCount_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            ViewModel.Weapon.ApplyStorageWeapons();
        }

        // ===== 资源 =====
        private void ReadGlobalResources_Click(object sender, RoutedEventArgs e) => ViewModel.Resource.ReadGlobalResources();
        private void ApplyGlobalResources_Click(object sender, RoutedEventArgs e) => ViewModel.Resource.ApplyGlobalResources();
        private void ReadPlayerResources_Click(object sender, RoutedEventArgs e) => ViewModel.Resource.ReadPlayerResources(ViewModel.Player.CurrentCharacterIndex, ViewModel.Player.PlayerName);
        private void ApplyPlayerResources_Click(object sender, RoutedEventArgs e) => ViewModel.Resource.ApplyPlayerResources(ViewModel.Player.CurrentCharacterIndex);

        // ===== 高级 =====
        private void ChangeMax_Click(object sender, RoutedEventArgs e) => ViewModel.Advanced.ChangeMax();
        private void OpenSettings_Click(object sender, RoutedEventArgs e) => ViewModel.OpenSettings();
    }
}