using DR2CDebugTool.Helpers;
using DR2CDebugTool.Models;
using System;
using System.Diagnostics;
using System.Text;

namespace DR2CDebugTool.Services
{
    public class MemoryService
    {
        public IntPtr ProcessHandle { get; private set; } = IntPtr.Zero;
        public IntPtr ModuleBase { get; private set; } = IntPtr.Zero;
        public Settings Settings { get; private set; } = null!;
        public WeaponManager WeaponManager { get; } = new();
        public int ProcessId { get; private set; }

        public bool IsReady => ProcessHandle != IntPtr.Zero && ModuleBase != IntPtr.Zero && Settings != null;

        public event Action? ConnectionChanged;

        public void LoadSettings(Settings settings)
        {
            Settings = settings;
        }

        public bool Attach(int pid)
        {
            if (ProcessHandle != IntPtr.Zero)
            {
                MemoryHelper.CloseHandle(ProcessHandle);
                ProcessHandle = IntPtr.Zero;
            }

            ProcessHandle = MemoryHelper.OpenProcessForMemory(pid);
            if (ProcessHandle == IntPtr.Zero)
                return false;

            try
            {
                var proc = Process.GetProcessById(pid);
                ModuleBase = proc.MainModule?.BaseAddress ?? IntPtr.Zero;
                if (ModuleBase == IntPtr.Zero)
                {
                    MemoryHelper.CloseHandle(ProcessHandle);
                    ProcessHandle = IntPtr.Zero;
                    return false;
                }

                ProcessId = pid;
                WeaponManager.Initialize(ProcessHandle, ModuleBase, Settings);
                ConnectionChanged?.Invoke();
                return true;
            }
            catch
            {
                MemoryHelper.CloseHandle(ProcessHandle);
                ProcessHandle = IntPtr.Zero;
                return false;
            }
        }

        public void Detach()
        {
            if (ProcessHandle != IntPtr.Zero)
            {
                MemoryHelper.CloseHandle(ProcessHandle);
                ProcessHandle = IntPtr.Zero;
                ModuleBase = IntPtr.Zero;
                ConnectionChanged?.Invoke();
            }
        }

        ~MemoryService()
        {
            if (ProcessHandle != IntPtr.Zero)
                MemoryHelper.CloseHandle(ProcessHandle);
        }

        // ===== 通用内存读写 =====
        public int ReadInt32(IntPtr address) => MemoryHelper.ReadInt32(ProcessHandle, address);
        public void WriteInt32(IntPtr address, int value) => MemoryHelper.WriteInt32(ProcessHandle, address, value);
        public uint ReadUInt32(IntPtr address) => MemoryHelper.ReadUInt32(ProcessHandle, address);
        public void WriteUInt32(IntPtr address, uint value) => MemoryHelper.WriteUInt32(ProcessHandle, address, value);
        public ushort ReadUInt16(IntPtr address) => MemoryHelper.ReadUInt16(ProcessHandle, address);
        public byte ReadByte(IntPtr address) => MemoryHelper.ReadByte(ProcessHandle, address);
        public void WriteByte(IntPtr address, byte value) => MemoryHelper.WriteByte(ProcessHandle, address, value);
        public float ReadFloat(IntPtr address) => MemoryHelper.ReadFloat(ProcessHandle, address);
        public void WriteFloat(IntPtr address, float value) => MemoryHelper.WriteFloat(ProcessHandle, address, value);
        public byte[] ReadBytes(IntPtr address, int size) => MemoryHelper.ReadBytes(ProcessHandle, address, size);
        public void WriteBytes(IntPtr address, byte[] data) => MemoryHelper.WriteBytes(ProcessHandle, address, data);

        public string ReadString(IntPtr address, int maxLen)
        {
            if (address == IntPtr.Zero) return "";
            try
            {
                byte[] buffer = MemoryHelper.ReadBytes(ProcessHandle, address, maxLen);
                int len = 0;
                while (len < buffer.Length && buffer[len] != 0) len++;
                return Encoding.UTF8.GetString(buffer, 0, len);
            }
            catch
            {
                return "";
            }
        }

        public void WriteString(IntPtr address, string str, int maxLen)
        {
            if (address == IntPtr.Zero) return;
            byte[] strBytes = Encoding.UTF8.GetBytes(str);
            if (strBytes.Length >= maxLen)
                Array.Resize(ref strBytes, maxLen - 1);
            byte[] buffer = new byte[maxLen];
            Array.Copy(strBytes, buffer, strBytes.Length);
            buffer[strBytes.Length] = 0;
            MemoryHelper.WriteBytes(ProcessHandle, address, buffer);
        }

        // ===== 玩家相关 =====
        public IntPtr GetPlayerBase(int index)
        {
            if (ModuleBase == IntPtr.Zero || Settings == null) return IntPtr.Zero;
            return ModuleBase + (int)Settings.PlayerArrayOffset + index * (int)Settings.PlayerStructSize;
        }

        public int ReadHealth(IntPtr playerBase) => ReadInt32(playerBase + Settings.PlayerHealthOffset);
        public void WriteHealth(IntPtr playerBase, int value) => WriteInt32(playerBase + Settings.PlayerHealthOffset, value);
        public string ReadPlayerName(IntPtr playerBase) => ReadString(playerBase + Settings.PlayerNameOffset, 40);
        public void WritePlayerName(IntPtr playerBase, string name) => WriteString(playerBase + Settings.PlayerNameOffset, name, 40);
        public string ReadPerk(IntPtr playerBase) => ReadString(playerBase + Settings.PlayerPerkOffset, 40);
        public string ReadTrait(IntPtr playerBase) => ReadString(playerBase + Settings.PlayerTraitOffset, 40);

        public int ReadStatBase(IntPtr playerBase, int index) => ReadByte(playerBase + Settings.PlayerBaseOffset + index);
        public void WriteStatBase(IntPtr playerBase, int index, byte value) => WriteByte(playerBase + Settings.PlayerBaseOffset + index, value);
        public int ReadStatBonus(IntPtr playerBase, int index) => (sbyte)ReadByte(playerBase + Settings.PlayerBonusOffset + index);
        public void WriteStatBonus(IntPtr playerBase, int index, sbyte value) => WriteByte(playerBase + Settings.PlayerBonusOffset + index, (byte)value);

        // ===== 实体相关 =====
        public Position ReadEntityPosition(IntPtr entityAddr)
        {
            if (!IsReady || entityAddr == IntPtr.Zero) return new();
            try
            {
                return new Position
                {
                    PosX = ReadFloat(entityAddr + Settings.EntityPosXOffset),
                    PosY = ReadFloat(entityAddr + Settings.EntityPosYOffset),
                    PosZ = ReadFloat(entityAddr + Settings.EntityPosZOffset),
                    VelX = ReadFloat(entityAddr + Settings.EntityVelXOffset),
                    VelY = ReadFloat(entityAddr + Settings.EntityVelYOffset),
                    VelZ = ReadFloat(entityAddr + Settings.EntityVelZOffset),
                    AreaId = ReadByte(entityAddr + Settings.EntityAreaIdOffset),
                };
            }
            catch { return new(); }
        }

        public bool WriteEntityPosition(IntPtr entityAddr, Position pos)
        {
            if (!IsReady || entityAddr == IntPtr.Zero) return false;
            try
            {
                WriteFloat(entityAddr + Settings.EntityPosXOffset, pos.PosX);
                WriteFloat(entityAddr + Settings.EntityPosYOffset, pos.PosY);
                WriteFloat(entityAddr + Settings.EntityPosZOffset, pos.PosZ);
                WriteFloat(entityAddr + Settings.EntityVelXOffset, pos.VelX);
                WriteFloat(entityAddr + Settings.EntityVelYOffset, pos.VelY);
                WriteFloat(entityAddr + Settings.EntityVelZOffset, pos.VelZ);
                WriteInt32(entityAddr + Settings.EntityAreaIdOffset, pos.AreaId);
                return true;
            }
            catch { return false; }
        }

        public void VirtualProtect(IntPtr address, uint size, uint newProtect, out uint oldProtect)
        {
            MemoryHelper.VirtualProtectEx(ProcessHandle, address, size, newProtect, out oldProtect);
        }
    }
}