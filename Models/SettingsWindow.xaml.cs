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
            PlayerBaseOffsetLabel.Text = LanguageManager.Get("Settings_PlayerBaseOffset");
            PlayerBonusOffsetLabel.Text = LanguageManager.Get("Settings_PlayerBonusOffset");
            EntityPoolOffsetLabel.Text = LanguageManager.Get("Settings_EntityPoolOffset");
            EntitySizeLabel.Text = LanguageManager.Get("Settings_EntitySize");
            WeaponPoolOffsetLabel.Text = LanguageManager.Get("Settings_WeaponPoolOffset");
            WeaponSizeLabel.Text = LanguageManager.Get("Settings_WeaponSize");
            MaxWeaponsLabel.Text = LanguageManager.Get("Settings_MaxWeapons");
            StorageWeaponOffsetLabel.Text = LanguageManager.Get("Settings_StorageWeaponOffset");
            StorageWeaponSlotsLabel.Text = LanguageManager.Get("Settings_StorageWeaponSlots");
            StorageWeaponSizeLabel.Text = LanguageManager.Get("Settings_StorageWeaponSize");
            ApplySetting.Content = LanguageManager.Get("Apply");
            CancelSetting.Content = LanguageManager.Get("Cancel");
        }

        private void CopySettings(Settings src, Settings dst)
        {
            dst.Language = src.Language;
            dst.PlayerArrayOffset = src.PlayerArrayOffset;
            dst.PlayerStructSize = src.PlayerStructSize;
            dst.PlayerBaseOffset = src.PlayerBaseOffset;
            dst.PlayerBonusOffset = src.PlayerBonusOffset;
            dst.PlayerHealthOffset = src.PlayerHealthOffset;
            dst.PlayerNameOffset = src.PlayerNameOffset;
            dst.PlayerPerkOffset = src.PlayerPerkOffset;
            dst.PlayerTraitOffset = src.PlayerTraitOffset;

            dst.EntityPoolOffset = src.EntityPoolOffset;
            dst.EntitySize = src.EntitySize;
            dst.EntityIdOffset = src.EntityIdOffset;
            dst.EntityTypeOffset = src.EntityTypeOffset;
            dst.EntityHealthOffset = src.EntityHealthOffset;
            dst.EntityNameOffset = src.EntityNameOffset;
            dst.EntityPosXOffset = src.EntityPosXOffset;
            dst.EntityPosYOffset = src.EntityPosYOffset;
            dst.EntityLightROffset = src.EntityLightROffset;
            dst.EntityLightGOffset = src.EntityLightGOffset;
            dst.EntityLightBOffset = src.EntityLightBOffset;
            dst.EntityLightAOffset = src.EntityLightAOffset;

            dst.WeaponPoolOffset = src.WeaponPoolOffset;
            dst.WeaponSize = src.WeaponSize;
            dst.MaxWeapons = src.MaxWeapons;
            dst.StorageWeaponOffset = src.StorageWeaponOffset;
            dst.StorageWeaponSlots = src.StorageWeaponSlots;
            dst.StorageWeaponSize = src.StorageWeaponSize;

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
            PlayerBaseOffsetBox.Text = "0x" + UpdatedSettings.PlayerBaseOffset.ToString("X");
            PlayerBonusOffsetBox.Text = "0x" + UpdatedSettings.PlayerBonusOffset.ToString("X");
            EntityPoolOffsetBox.Text = "0x" + UpdatedSettings.EntityPoolOffset.ToString("X");
            EntitySizeBox.Text = "0x" + UpdatedSettings.EntitySize.ToString("X");
            WeaponPoolOffsetBox.Text = "0x" + UpdatedSettings.WeaponPoolOffset.ToString("X");
            WeaponSizeBox.Text = "0x" + UpdatedSettings.WeaponSize.ToString("X");
            MaxWeaponsBox.Text = "0x" + UpdatedSettings.MaxWeapons.ToString("X");
            StorageWeaponOffsetBox.Text = "0x" + UpdatedSettings.StorageWeaponOffset.ToString("X");
            StorageWeaponSlotsBox.Text = UpdatedSettings.StorageWeaponSlots.ToString();
            StorageWeaponSizeBox.Text = "0x" + UpdatedSettings.StorageWeaponSize.ToString("X");
            StorageOffsetBox.Text = "0x" + UpdatedSettings.StorageOffset.ToString("X");
        }

        private bool ParseHex(string text, out uint value)
        {
            if (text.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                text = text.Substring(2);
            return uint.TryParse(text, System.Globalization.NumberStyles.HexNumber, null, out value);
        }

        private bool ParseHexInt(string text, out int value)
        {
            if (text.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                text = text[2..];
            return int.TryParse(text, System.Globalization.NumberStyles.HexNumber, null, out value);
        }

        private void OK_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                if (LanguageCombo.SelectedItem is ComboBoxItem langItem && langItem.Tag != null)
                    UpdatedSettings.Language = langItem.Tag.ToString() ?? "en-US";

                if (!ParseHex(PlayerArrayOffsetBox.Text, out uint playerArrayOff))
                    throw new Exception("Invalid Player Array Offset");
                UpdatedSettings.PlayerArrayOffset = playerArrayOff;

                if (!ParseHex(PlayerStructSizeBox.Text, out uint playerSize))
                    throw new Exception("Invalid Player Struct Size");
                UpdatedSettings.PlayerStructSize = playerSize;

                if (!ParseHexInt(PlayerBaseOffsetBox.Text, out int baseOff))
                    throw new Exception("Invalid Player Base Offset");
                UpdatedSettings.PlayerBaseOffset = baseOff;

                if (!ParseHexInt(PlayerBonusOffsetBox.Text, out int bonusOff))
                    throw new Exception("Invalid Player Bonus Offset");
                UpdatedSettings.PlayerBonusOffset = bonusOff;

                if (!ParseHex(EntityPoolOffsetBox.Text, out uint entityPool))
                    throw new Exception("Invalid Entity Pool Offset");
                UpdatedSettings.EntityPoolOffset = entityPool;

                if (!ParseHex(EntitySizeBox.Text, out uint entitySize))
                    throw new Exception("Invalid Entity Size");
                UpdatedSettings.EntitySize = entitySize;

                if (!ParseHex(WeaponPoolOffsetBox.Text, out uint weaponPool))
                    throw new Exception("Invalid Weapon Pool Offset");
                UpdatedSettings.WeaponPoolOffset = weaponPool;

                if (!ParseHex(WeaponSizeBox.Text, out uint weaponSize))
                    throw new Exception("Invalid Weapon Size");
                UpdatedSettings.WeaponSize = weaponSize;

                if (!ParseHex(MaxWeaponsBox.Text, out uint maxWeapons))
                    throw new Exception("Invalid Max Weapons");
                UpdatedSettings.MaxWeapons = maxWeapons;
                
                if (!ParseHex(StorageOffsetBox.Text, out uint storageOffset))
                    throw new Exception("Invalid Storage Offset");
                UpdatedSettings.StorageOffset = storageOffset;

                if (!ParseHex(StorageWeaponOffsetBox.Text, out uint storageWeaponOffset))
                    throw new Exception("Invalid Storage Weapon Offset");
                UpdatedSettings.StorageWeaponOffset = storageWeaponOffset;

                if (!uint.TryParse(StorageWeaponSlotsBox.Text, out uint storageWeaponSlots))
                    throw new Exception("Invalid Storage Weapon Slots");
                UpdatedSettings.StorageWeaponSlots = storageWeaponSlots;

                if (!ParseHex(StorageWeaponSizeBox.Text, out uint storageWeaponSize))
                    throw new Exception("Invalid Storage Weapon Size");
                UpdatedSettings.StorageWeaponSize = storageWeaponSize;

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