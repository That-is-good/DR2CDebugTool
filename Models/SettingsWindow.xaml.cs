using DR2CDebugTool.Models;
using System.Windows.Controls;
using System.Windows;

namespace DR2CDebugTool
{
    public partial class SettingsWindow : Window
    {
        public Settings UpdatedSettings { get; private set; }

        public SettingsWindow(Settings current)
        {
            InitializeComponent();
            UpdatedSettings = new Settings();
            CopySettings(current, UpdatedSettings);
            LoadUI();
            this.Loaded += SettingsWindow_Loaded;
        }

        private void SettingsWindow_Loaded(object sender, RoutedEventArgs e)
        {
            // 使用 LanguageManager 本地化
            Title = LanguageManager.Get("Settings_Title");
            LanguageLabel.Text = LanguageManager.Get("Settings_Language");
            PlayerArrayOffsetLabel.Text = LanguageManager.Get("Settings_PlayerArrayOffset");
            PlayerStructSizeLabel.Text = LanguageManager.Get("Settings_PlayerStructSize");
            PlayerSlotsLabel.Text = LanguageManager.Get("Settings_PlayerSlots");

            EntityPoolOffsetLabel.Text = LanguageManager.Get("Settings_EntityPoolOffset");
            EntitySizeLabel.Text = LanguageManager.Get("Settings_EntitySize");
            EntitySlotsLabel.Text = LanguageManager.Get("Settings_EntitySlots");

            WeaponPoolOffsetLabel.Text = LanguageManager.Get("Settings_WeaponPoolOffset");
            WeaponSizeLabel.Text = LanguageManager.Get("Settings_WeaponSize");
            MaxWeaponsLabel.Text = LanguageManager.Get("Settings_MaxWeapons");

            StorageWeaponOffsetLabel.Text = LanguageManager.Get("Settings_StorageWeaponOffset");
            StorageWeaponSizeLabel.Text = LanguageManager.Get("Settings_StorageWeaponSize");
            StorageWeaponSlotsLabel.Text = LanguageManager.Get("Settings_StorageWeaponSlots");

            ApplySetting.Content = LanguageManager.Get("Apply");
            CancelSetting.Content = LanguageManager.Get("Cancel");
        }

        private void CopySettings(Settings src, Settings dst)
        {
            dst.Language = src.Language;
            dst.PlayerArrayOffset = src.PlayerArrayOffset;
            dst.PlayerStructSize = src.PlayerStructSize;
            dst.PlayerSlots = src.PlayerSlots;

            dst.EntityPoolOffset = src.EntityPoolOffset;
            dst.EntitySize = src.EntitySize;
            dst.EntitySlots = src.EntitySlots;

            dst.WeaponPoolOffset = src.WeaponPoolOffset;
            dst.WeaponSize = src.WeaponSize;
            dst.MaxWeapons = src.MaxWeapons;

            dst.StorageWeaponOffset = src.StorageWeaponOffset;
            dst.StorageWeaponSize = src.StorageWeaponSize;
            dst.StorageWeaponSlots = src.StorageWeaponSlots;

            dst.StorageOffset = src.StorageOffset;
        }

        private void LoadUI()
        {
            foreach (ComboBoxItem item in LanguageCombo.Items)
            {
                if (item.Tag?.ToString() == UpdatedSettings.Language)
                { LanguageCombo.SelectedItem = item; break; }
            }
            
            PlayerArrayOffsetBox.Text = "0x" + UpdatedSettings.PlayerArrayOffset.ToString("X");
            PlayerStructSizeBox.Text = "0x" + UpdatedSettings.PlayerStructSize.ToString("X");
            PlayerSlotsBox.Text = UpdatedSettings.PlayerSlots.ToString();

            EntityPoolOffsetBox.Text = "0x" + UpdatedSettings.EntityPoolOffset.ToString("X");
            EntitySizeBox.Text = "0x" + UpdatedSettings.EntitySize.ToString("X");
            EntitySlotsBox.Text = UpdatedSettings.EntitySlots.ToString();

            WeaponPoolOffsetBox.Text = "0x" + UpdatedSettings.WeaponPoolOffset.ToString("X");
            WeaponSizeBox.Text = "0x" + UpdatedSettings.WeaponSize.ToString("X");
            MaxWeaponsBox.Text = UpdatedSettings.MaxWeapons.ToString();

            StorageResourceOffsetBox.Text = "0x" + UpdatedSettings.StorageOffset.ToString("X");
            StorageWeaponOffsetBox.Text = "0x" + UpdatedSettings.StorageWeaponOffset.ToString("X");
            StorageWeaponSizeBox.Text = "0x" + UpdatedSettings.StorageWeaponSize.ToString("X");
            StorageWeaponSlotsBox.Text = UpdatedSettings.StorageWeaponSlots.ToString();
        }

        private bool ParseHex(string text, out uint value)
        {
            if (text.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                text = text.Substring(2);
            return uint.TryParse(text, System.Globalization.NumberStyles.HexNumber, null, out value);
        }

        // private bool ParseHexInt(string text, out int value)
        // {
        //     if (text.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
        //         text = text[2..];
        //     return int.TryParse(text, System.Globalization.NumberStyles.HexNumber, null, out value);
        // }

        private void OK_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (LanguageCombo.SelectedItem is ComboBoxItem langItem && langItem.Tag != null)
                    UpdatedSettings.Language = langItem.Tag.ToString() ?? "en-US";

                //==================Player
                if (!ParseHex(PlayerArrayOffsetBox.Text, out uint playerArrayOff))
                    throw new Exception("Invalid Player Array Offset");
                UpdatedSettings.PlayerArrayOffset = playerArrayOff;

                if (!ParseHex(PlayerStructSizeBox.Text, out uint playerSize))
                    throw new Exception("Invalid Player Struct Size");
                UpdatedSettings.PlayerStructSize = playerSize;

                if (!uint.TryParse(PlayerSlotsBox.Text, out uint playerSlots))
                    throw new Exception("Invalid Player Slots");
                UpdatedSettings.PlayerSlots = playerSlots;
                
                //==================Entity
                if (!ParseHex(EntityPoolOffsetBox.Text, out uint entityPool))
                    throw new Exception("Invalid Entity Pool Offset");
                UpdatedSettings.EntityPoolOffset = entityPool;

                if (!ParseHex(EntitySizeBox.Text, out uint entitySize))
                    throw new Exception("Invalid Entity Size");
                UpdatedSettings.EntitySize = entitySize;

                if (!uint.TryParse(EntitySlotsBox.Text, out uint entitySlots))
                    throw new Exception("Invalid Entity Slots");
                UpdatedSettings.EntitySlots = entitySlots;
                
                //==================Weapon
                if (!ParseHex(WeaponPoolOffsetBox.Text, out uint weaponPool))
                    throw new Exception("Invalid Weapon Pool Offset");
                UpdatedSettings.WeaponPoolOffset = weaponPool;

                if (!ParseHex(WeaponSizeBox.Text, out uint weaponSize))
                    throw new Exception("Invalid Weapon Size");
                UpdatedSettings.WeaponSize = weaponSize;

                if (!uint.TryParse(MaxWeaponsBox.Text, out uint maxWeapons))
                    throw new Exception("Invalid Max Weapons");
                UpdatedSettings.MaxWeapons = maxWeapons;

                //==================StorageWeapon
                if (!ParseHex(StorageResourceOffsetBox.Text, out uint StorageOffset))
                    throw new Exception("Invalid Storage Resource Offset");
                UpdatedSettings.StorageOffset = StorageOffset;

                if (!ParseHex(StorageWeaponOffsetBox.Text, out uint storageWeaponOffset))
                    throw new Exception("Invalid Storage Weapon Offset");
                UpdatedSettings.StorageWeaponOffset = storageWeaponOffset;

                if (!ParseHex(StorageWeaponSizeBox.Text, out uint storageWeaponSize))
                    throw new Exception("Invalid Storage Weapon Size");
                UpdatedSettings.StorageWeaponSize = storageWeaponSize;

                if (!uint.TryParse(StorageWeaponSlotsBox.Text, out uint storageWeaponSlots))
                    throw new Exception("Invalid Storage Weapon Slots");
                UpdatedSettings.StorageWeaponSlots = storageWeaponSlots;

                DialogResult = true;
                Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Invalid input: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void Cancel_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = false;
            Close();
        }
    }
}