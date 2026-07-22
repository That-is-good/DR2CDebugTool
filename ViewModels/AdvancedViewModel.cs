using DR2CDebugTool.Models;
using DR2CDebugTool.Services;
using System;

namespace DR2CDebugTool.ViewModels
{
    public class AdvancedViewModel : BaseViewModel
    {
        private readonly MemoryService _memory;
        private readonly byte[] nopBytes = [0x90, 0x90];

        private string _maxStatus = "";
        public string MaxStatus
        {
            get => _maxStatus;
            set => SetProperty(ref _maxStatus, value);
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

        public AdvancedViewModel(MemoryService memory)
        {
            _memory = memory;
        }

        public void RefreshOffsetInfo() => OnPropertyChanged(nameof(OffsetInfoText));

        public void ChangeMax()
        {
            if (!_memory.IsReady)
            {
                System.Windows.MessageBox.Show("Not attached");
                return;
            }

            IntPtr jgAddr = _memory.ModuleBase + 0x27ee4;
            _memory.VirtualProtect(jgAddr, (uint)nopBytes.Length, 0x40, out uint oldProtect);
            _memory.WriteBytes(jgAddr, nopBytes);
            _memory.VirtualProtect(jgAddr, (uint)nopBytes.Length, oldProtect, out _);

            System.Windows.MessageBox.Show(
                LanguageManager.Get("Junk01"),
                "Attention",
                System.Windows.MessageBoxButton.OK,
                System.Windows.MessageBoxImage.Warning
            );
            System.Windows.MessageBox.Show(
                LanguageManager.Get("Junk02"),
                "Attention",
                System.Windows.MessageBoxButton.OK,
                System.Windows.MessageBoxImage.Question
            );
            MaxStatus = LanguageManager.Get("Applied");
        }
    }
}