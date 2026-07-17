using System;
using System.Runtime.InteropServices;

namespace DR2CDebugTool.Helpers
{
    public static class MemoryHelper
    {
        [DllImport("kernel32.dll")]
        public static extern IntPtr OpenProcess(int dwDesiredAccess, bool bInheritHandle, int dwProcessId);

        [DllImport("kernel32.dll")]
        public static extern bool ReadProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int dwSize, out IntPtr lpNumberOfBytesRead);

        [DllImport("kernel32.dll")]
        public static extern bool WriteProcessMemory(IntPtr hProcess, IntPtr lpBaseAddress, byte[] lpBuffer, int dwSize, out IntPtr lpNumberOfBytesWritten);

        [DllImport("kernel32.dll")]
        public static extern bool VirtualProtectEx(IntPtr hProcess, IntPtr lpAddress, uint dwSize, uint flNewProtect, out uint lpflOldProtect);

        public const uint PAGE_EXECUTE_READWRITE = 0x40;
        public const uint PAGE_EXECUTE_READ = 0x20;
        [DllImport("kernel32.dll")]
        public static extern bool CloseHandle(IntPtr hObject);

        public const int PROCESS_VM_READ = 0x0010;
        public const int PROCESS_VM_WRITE = 0x0020;
        public const int PROCESS_VM_OPERATION = 0x0008;

        public static IntPtr OpenProcessForMemory(int pid)
        {
            return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, false, pid);
        }

        public static byte[] ReadBytes(IntPtr hProcess, IntPtr address, int size)
        {
            byte[] buffer = new byte[size];
            ReadProcessMemory(hProcess, address, buffer, size, out _);
            return buffer;
        }

        public static void WriteBytes(IntPtr hProcess, IntPtr address, byte[] data)
        {
            WriteProcessMemory(hProcess, address, data, data.Length, out _);
        }

        public static int ReadInt32(IntPtr hProcess, IntPtr address)
        {
            return BitConverter.ToInt32(ReadBytes(hProcess, address, 4), 0);
        }

        public static void WriteInt32(IntPtr hProcess, IntPtr address, int value)
        {
            WriteBytes(hProcess, address, BitConverter.GetBytes(value));
        }

        public static byte ReadByte(IntPtr hProcess, IntPtr address)
        {
            return ReadBytes(hProcess, address, 1)[0];
        }

        public static void WriteByte(IntPtr hProcess, IntPtr address, byte value)
        {
            WriteBytes(hProcess, address, new byte[] { value });
        }

        // 新增：读取 ushort (2字节)
        public static ushort ReadUInt16(IntPtr hProcess, IntPtr address)
        {
            return BitConverter.ToUInt16(ReadBytes(hProcess, address, 2), 0);
        }

        // 新增：写入 ushort (2字节)
        public static void WriteUInt16(IntPtr hProcess, IntPtr address, ushort value)
        {
            WriteBytes(hProcess, address, BitConverter.GetBytes(value));
        }

        public static uint ReadUInt32(IntPtr hProcess, IntPtr address)
        {
            return BitConverter.ToUInt32(ReadBytes(hProcess, address, 4), 0);
        }

        // 新增：写入 uint (4字节)
        public static void WriteUInt32(IntPtr hProcess, IntPtr address, uint value)
        {
            WriteBytes(hProcess, address, BitConverter.GetBytes(value));
        }

        public static float ReadFloat(IntPtr hProcess, IntPtr address)
        {
            return BitConverter.ToSingle(ReadBytes(hProcess, address, 4), 0);
        }

        public static void WriteFloat(IntPtr hProcess, IntPtr address, float value)
        {
            WriteBytes(hProcess, address, BitConverter.GetBytes(value));
        }
    }
}