using DR2CDebugTool.Models;
using DR2CDebugTool.Services;
using System;
using System.Collections.ObjectModel;

namespace DR2CDebugTool.ViewModels
{
    public class ResourceViewModel : BaseViewModel
    {
        private readonly MemoryService _memory;

        public ObservableCollection<ResourceEntry> GlobalResources { get; } = [];
        public ObservableCollection<ResourceEntry> PlayerResources { get; } = [];

        private string _resourceCharInfo = LanguageManager.Get("NoPlayerSelected");
        public string ResourceCharInfo
        {
            get => _resourceCharInfo;
            set => SetProperty(ref _resourceCharInfo, value);
        }

        private string _statusText = "";
        public string StatusText
        {
            get => _statusText;
            set => SetProperty(ref _statusText, value);
        }

        public ResourceViewModel(MemoryService memory)
        {
            _memory = memory;
            InitializeResources();
        }

        private void InitializeResources()
        {
            Dictionary<string, string> res = new()
            {
                {"food", LanguageManager.Get("Food")},
                {"gas", LanguageManager.Get("Gasoline")},
                {"medical", LanguageManager.Get("Medical")},
                {"bullet", LanguageManager.Get("PistolAmmo")},
                {"rifle", LanguageManager.Get("RifleAmmo")},
                {"shell", LanguageManager.Get("ShotgunAmmo")},
            };

            GlobalResources.Clear();
            PlayerResources.Clear();
            foreach (var e in res)
            {
                GlobalResources.Add(new ResourceEntry { Key = e.Key, Name = e.Value });
                PlayerResources.Add(new ResourceEntry { Key = e.Key, Name = e.Value });
            }
        }

        private void ReadResourceOff(ObservableCollection<ResourceEntry> resources, IntPtr resAddr)
        {
            for (int i = 0; i < 6; ++i)
            {
                resources[i].Value = (int)_memory.ReadUInt32(resAddr + 0x04 * (i + 1));
            }
        }

        private void WriteResourceOff(ObservableCollection<ResourceEntry> resources, IntPtr resAddr)
        {
            for (int i = 0; i < 6; ++i)
            {
                _memory.WriteUInt32(resAddr + 0x04 * (i + 1), (uint)resources[i].Value);
            }
        }

        public void ReadGlobalResources()
        {
            if (!_memory.IsReady) return;
            try
            {
                IntPtr storageAddr = _memory.ModuleBase + (int)_memory.Settings.StorageOffset;
                ReadResourceOff(GlobalResources, storageAddr);
                StatusText = "Read global resources";
            }
            catch (Exception ex)
            {
                StatusText = $"Error reading global resources: {ex.Message}";
            }
        }

        public void ApplyGlobalResources()
        {
            if (!_memory.IsReady) return;
            try
            {
                IntPtr storageAddr = _memory.ModuleBase + (int)_memory.Settings.StorageOffset;
                WriteResourceOff(GlobalResources, storageAddr);
                StatusText = "Applied global resources";
            }
            catch (Exception ex)
            {
                StatusText = $"Error applying global resources: {ex.Message}";
            }
        }

        public void ReadPlayerResources(int characterIndex, string? characterName)
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(characterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                IntPtr resourceAddr = playerBase + Settings.ResourceOffset;
                ReadResourceOff(PlayerResources, resourceAddr);
                ResourceCharInfo = $"{(characterName ?? "Unknown")} (Index: {characterIndex})";
                StatusText = "Read player resources";
            }
            catch (Exception ex)
            {
                StatusText = $"Error reading player resources: {ex.Message}";
            }
        }

        public void ApplyPlayerResources(int characterIndex)
        {
            if (!_memory.IsReady) return;
            IntPtr playerBase = _memory.GetPlayerBase(characterIndex);
            if (playerBase == IntPtr.Zero) return;

            try
            {
                IntPtr resourceAddr = playerBase + Settings.ResourceOffset;
                WriteResourceOff(PlayerResources, resourceAddr);
                StatusText = "Applied player resources";
            }
            catch (Exception ex)
            {
                StatusText = $"Error applying player resources: {ex.Message}";
            }
        }
    }
}