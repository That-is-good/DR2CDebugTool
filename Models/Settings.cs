using Newtonsoft.Json;
using System.IO;

namespace DR2CDebugTool.Models
{
    public class Settings
    {
        public string Language { get; set; } = "en-US";
        
        // ===== 玩家角色数组 =====
        public uint PlayerArrayOffset { get; set; } = 0x5E25D8;
        public uint PlayerStructSize { get; set; } = 0x2E0;
        
        // 玩家字段偏移
        public int PlayerHealthOffset { get; set; } = 0x140;
        public int PlayerBaseOffset { get; set; } = 0x1C9;
        public int PlayerBonusOffset { get; set; } = 0x1E3;
        public int PlayerNameOffset { get; set; } = 0x1C;
        public int PlayerPerkOffset { get; set; } = 0x44;
        public int PlayerTraitOffset { get; set; } = 0x6C;
        
        // ===== 实体池 =====
        public uint EntityPoolOffset { get; set; } = 0x5632E0;
        public uint EntitySize { get; set; } = 0x304;
        public uint EntitySlots { get; set; } = 0x262;
        
        // ===== 实体字段偏移 =====
        public int EntityIdOffset { get; set; } = 0x00;          // ushort: ID
        public int EntityTypeOffset { get; set; } = 0x02;        // byte: 0x01=玩家, 0x02=僵尸
        public int EntityHealthOffset { get; set; } = 0x254;     // int: 血量
        public int EntityNameOffset { get; set; } = 0x1C;        // string: 名字
        public int EntityPosXOffset { get; set; } = 0x2C;        // float: X坐标
        public int EntityPosYOffset { get; set; } = 0x30;        // float: Y坐标
        public int EntityPosZOffset { get; set; } = 0x34;        // float: Z坐标
        public int EntityVelXOffset { get; set; } = 0x38;        // float: X速度
        public int EntityVelYOffset { get; set; } = 0x3C;        // float: Y速度
        public int EntityVelZOffset { get; set; } = 0x40;        // float: Z速度
        public int EntitySpeedOffset { get; set; } = 0x44;        // float: 速度值
        public int EntityAreaIdOffset { get; set; } = 0x04;
        
        // ===== 实体高亮相关 =====
        public int EntityLightROffset { get; set; } = 0x1F0;     // float: 光照 R
        public int EntityLightGOffset { get; set; } = 0x1F4;     // float: 光照 G
        public int EntityLightBOffset { get; set; } = 0x1F8;     // float: 光照 B
        public int EntityLightAOffset { get; set; } = 0x1FC;     // float: 光照 A

        // ===== 实体类型常量 =====
        public enum ENTITY_TYPE: byte
        {
            ENTITY_TYPE_HUMAN = 0x01,
            ENTITY_TYPE_ZOMBIE = 0x02,
            ENTITY_TYPE_ITEM = 0x03,
            ENTITY_TYPE_PROJECTILE = 0x04,
        };

        // ===== 武器池 =====
        public uint WeaponPoolOffset { get; set; } = 0x004E0080;
        public uint WeaponSize { get; set; } = 0x1C4;
        public uint MaxWeapons { get; set; } = 0x401;
        public uint StorageWeaponOffset { get; set; } = 0x5E2280;
        public uint StorageWeaponSlots { get; set; } = 15;
        public uint StorageWeaponSize { get; set; } = 8;

        // ===== 仓库资源 =====
        public const int ResourceOffset = 0x288;
        public uint StorageOffset { get; set; } = 0x5E2260;

        private static string ConfigPath = "settings.json";

        public static Settings Load()
        {
            if (File.Exists(ConfigPath))
            {
                var json = File.ReadAllText(ConfigPath);
                return JsonConvert.DeserializeObject<Settings>(json) ?? new Settings();
            }
            return new Settings();
        }

        public void Save()
        {
            var json = JsonConvert.SerializeObject(this, Formatting.Indented);
            File.WriteAllText(ConfigPath, json);
        }
    }
    public static class EntityTypeExtensions
    {
        public static bool Is(this byte entityType, Settings.ENTITY_TYPE type)
        {
            return entityType == (byte)type;
        }
    }
}