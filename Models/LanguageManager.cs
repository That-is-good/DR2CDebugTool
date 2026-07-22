using System.Collections.Generic;

namespace DR2CDebugTool.Models
{
    public static class LanguageManager
    {
        public static string CurrentLanguage { get; set; } = "en-US";

        // 词元（最小可复用单元）
        private static readonly Dictionary<string, Dictionary<string, string>> _translations = new()
        {
            ["en-US"] = new()
            {
                // 基础词元
                {"MainWindow_Title", "Death Road to Canada Debug Tool"},
                {"Refresh", "Refresh"},
                {"Refreshed", "Refreshed"},
                {"Apply", "Apply"},
                {"Cancel", "Cancel"},
                {"Attach", "Attach"},
                {"Settings", "Settings"},
                {"Read", "Read"},
                {"Write", "Write"},
                {"Search", "Search"},
                {"Name", "Name"},
                {"Error", "Error: "},
                {"Applied", "Applied"},
                {"Found", "Found"},
                {"Filtered", "Filtered"},
                {"Address", "Address"},
                {"Value", "Value"},
                {"Slot", "Slot"},
                {"ID", "ID"},

                // 选项卡标题
                {"PlayerStats", "Player Stats"},
                {"Entities", "Entities"},
                {"Weapons", "Weapon"},
                {"Resources", "Resources"},
                {"Advanced", "Advanced"},

                // === 状态 ===
                {"NotAttached", "Not attached"},
                {"Attached", "Attached to PID"},
                {"NoProcesses", "No processes found"},
                {"FailedOpenProcess", "Failed to open process"},
                {"FailedGetModuleBase", "Failed to get module base address"},
                {"ErrorGettingModule", "Error getting module"},

                // === 进程 ===
                {"NoPlayerSelected", "No Player Selected"},
                {"CurrentCharacter", "Current Character"},
                {"NoSourceEntity", "No source entity selected"},
                {"NoTargetEntity", "No target entity selected"},
                {"SameSourceAndTarget", "Source and target are the same entity!"},
                {"Empty", "Empty"},

                // === 角色面板 ===
                {"ScanCharacters", "Scan Characters"},
                {"Character", "Character"},
                {"Health", "Health"},
                {"Perk", "Perk"},
                {"Trait", "Trait"},
                {"PositionText", "Position"},
                {"BasicInfoText", "Basic Info"},
                {"DebugPropertiesText", "Debug Properties"},
                {"AIPropertiesText", "AI Properties"},
                {"NoCollide", "No Collide"},
                {"Invisible", "Invisible"},
                {"Invincible", "Invincible"},
                {"Glow", "Glow"},
                {"Mass", "Mass"},
                {"Friction", "Friction"},
                {"AIState", "AI State"},
                {"AIWait", "AI Idle"},

                // === 属性面板 ===
                {"Attribute", "Attribute"},
                {"Col_Base", "Base (stat@)"},
                {"Col_Bonus", "Bonus (bonus@)"},
                {"Col_Effective", "Effective (effstat)"},
                {"Known", "Known"},

                // === 属性名 ===
                {"STAT_MORALE", "Morale"},
                {"STAT_ATTITUDE", "Attitude"},
                {"STAT_COMPOSURE", "Composure"},
                {"STAT_CHARM", "Charm"},
                {"STAT_WITS", "Wits"},
                {"STAT_LOYALTY", "Loyalty"},
                {"STAT_MEDICAL", "Medical"},
                {"STAT_MECHANICAL", "Mechanical"},
                {"STAT_SHOOTING", "Shooting"},
                {"STAT_STRENGTH", "Strength"},
                {"STAT_DEXTERITY", "Dexterity"},
                {"STAT_FITNESS", "Fitness"},
                {"STAT_VITALITY", "Vitality"},

                // === 实体 ===
                {"ScanEntity", "Scan Entities"},
                {"Scanning", "Scanning entities..."},
                {"Type", "Type"},
                {"SubType", "SubType"},
                {"Area", "Area"},
                {"Pos", "Pos"},
                {"Vel", "Vel"},
                {"FilterError", "Filter error"},
                {"InvalidEntityCount", "Invalid entity count: "},
                {"ScanError", "Scan Error: "},
                {"SelectedEntity", "Selected entity"},
                {"RefreshedEntity", "Refreshed entity"},
                {"TeleportToTarget", "Teleport To Target"},
                {"SwapPositions", "Swap Positions"},
                {"SetTarget", "Set Target"},
                {"Teleported", "Teleported"},
                {"Swapped", "Swapped positions"},
                {"FailWritePos", "Failed to write position."},
                {"TeleportError", "Teleport error: "},
                {"PositionUpdated", "Position updated"},
                {"AppliedChanges", "Applied changes to entity"},
                {"Total_Entity", "Total Entity: "},

                // 实体类型
                {"ENTITY_TYPE_ALL", "All"},
                {"ENTITY_TYPE_HUMAN", "Human"},
                {"ENTITY_TYPE_ZOMBIE", "Zombie"},
                {"ENTITY_TYPE_ITEM", "Item"},
                {"ENTITY_TYPE_PROJECTILE", "Projectile"},
                {"ENTITY_TYPE_UNKNOWN", "Unknown"},
                {"Furniture", "Furniture"},
                {"Pickup", "Pickup"},
                {"Vehicle", "Vehicle"},
                {"PickupSpec", "Special"},

                // === 武器 ===
                {"ScanWeaponPool", "Scan Weapon Pool"},
                {"WeaponRefresh", "Refresh Weapon Slot"},
                {"WeaponSlot", "Weapon Slot"},
                {"StorageWeapon", "Storage Weapon"},
                {"Stack", "Stack"},              // Count和Stack统一
                {"NoDrop", "Locked"},
                {"WeaponScanError", "Weapon scan error"},
                {"WeaponApplySuccess", "Weapon applied successfully"},
                {"WeaponApplyFailed", "Failed to apply weapon"},

                // 资源
                {"GlobalResources", "Global Resources"},
                {"PlayerResources", "Player Resources"},
                {"Food", "Food"},
                {"Gasoline", "Gasoline"},
                {"Medical", "Medical"},
                {"PistolAmmo", "Pistol Ammo"},
                {"RifleAmmo", "Rifle Ammo"},
                {"ShotgunAmmo", "Shotgun Ammo"},

                // 进阶
                {"NewMaxText", "Cancel Stats Limit"},
                {"Junk01", "Although code has changed, But CHAR_STAT_MAX and CHAR_STAT_MIN need you change by yourself.\nEnter {X to CHAR_STAT_MAX} and {X to CHAR_STAT_MIN} in console, X is what you want."},
                {"Junk02", "This Tool can also change.So What does this function do?"},

                // 设置窗口
                {"Settings_Title", "Settings"},
                {"Settings_Language", "Language:"},
                {"Settings_PlayerArrayOffset", "Player Array Offset (hex):"},
                {"Settings_PlayerStructSize", "Player Struct Size (hex):"},
                {"Settings_PlayerBaseOffset", "Player Base Offset (stat@):"},
                {"Settings_PlayerBonusOffset", "Player Bonus Offset (bonus@):"},
                {"Settings_EntityPoolOffset", "Entity Pool Offset (hex):"},
                {"Settings_EntitySize", "Entity Size (hex):"},
                {"Settings_EntityTypeOffset", "Entity Type Offset (hex):"},
                {"Settings_WeaponPoolOffset", "Weapon Pool Offset (hex):"},
                {"Settings_WeaponSize", "Weapon Size (hex):"},
                {"Settings_MaxWeapons", "Max Weapons (hex):"},
                {"Settings_StorageWeaponOffset", "Storage Weapon Offset (hex):"},
                {"Settings_StorageWeaponSlots", "Storage Weapon Slots:"},
                {"Settings_StorageWeaponSize", "Storage Weapon Size (hex):"},
            },
            ["zh-CN"] = new()
            {
                // 基础词元
                {"MainWindow_Title", "死亡之路加拿大调试工具"},
                {"Refresh", "刷新"},
                {"Refreshed", "已刷新"},
                {"Apply", "应用"},
                {"Cancel", "取消"},
                {"Attach", "附加"},
                {"Settings", "设置"},
                {"Read", "读取"},
                {"Write", "写入"},
                {"Search", "搜索"},
                {"Name", "名字"},
                {"Error", "错误："},
                {"Applied", "已应用"},
                {"Found", "找到"},
                {"Filtered", "已过滤"},
                {"Address", "地址"},
                {"Value", "数值"},
                {"Slot", "槽位"},
                {"ID", "ID"},

                // 选项卡标题
                {"PlayerStats", "玩家属性"},
                {"Entities", "实体"},
                {"Weapons", "武器"},
                {"Resources", "资源"},
                {"Advanced", "高级"},

                // === 状态 ===
                {"NotAttached", "未附加"},
                {"Attached", "已附加到 PID"},
                {"NoProcesses", "找不到进程"},
                {"FailedOpenProcess", "无法打开进程"},
                {"FailedGetModuleBase", "获取模块基址失败"},
                {"ErrorGettingModule", "获取模块错误"},

                // === 进程 ===
                {"NoPlayerSelected", "未选择角色"},
                {"CurrentCharacter", "当前角色"},
                {"NoSourceEntity", "未选中源实体"},
                {"NoTargetEntity", "未选中目标实体"},
                {"SameSourceAndTarget", "源实体和目标实体是同一个！"},
                {"Empty", "空"},

                // === 角色面板 ===
                {"ScanCharacters", "扫描角色"},
                {"Character", "角色"},
                {"Health", "生命值"},
                {"Perk", "特长"},
                {"Trait", "特质"},
                {"PositionText", "位置"},
                {"BasicInfoText", "基础信息"},
                {"DebugPropertiesText", "Debug选项"},
                {"AIPropertiesText", "AI选项"},
                {"NoCollide", "无碰撞"},
                {"Invisible", "不绘制"},
                {"Invincible", "无敌"},
                {"Glow", "发光"},
                {"Mass", "质量"},
                {"Friction", "摩擦力"},
                {"AIState", "AI状态"},
                {"AIWait", "AI发呆"},

                // === 属性面板 ===
                {"Attribute", "属性"},
                {"Col_Base", "基础值 (stat@)"},
                {"Col_Bonus", "加成值 (bonus@)"},
                {"Col_Effective", "有效值 (effstat)"},
                {"Known", "已知"},

                // === 属性名 ===
                {"STAT_MORALE", "士气"},
                {"STAT_ATTITUDE", "态度"},
                {"STAT_COMPOSURE", "冷静"},
                {"STAT_CHARM", "魅力"},
                {"STAT_WITS", "智慧"},
                {"STAT_LOYALTY", "忠诚"},
                {"STAT_MEDICAL", "医疗"},
                {"STAT_MECHANICAL", "机械"},
                {"STAT_SHOOTING", "射击"},
                {"STAT_STRENGTH", "力量"},
                {"STAT_DEXTERITY", "敏捷"},
                {"STAT_FITNESS", "体力"},
                {"STAT_VITALITY", "活力"},

                // === 实体 ===
                {"ScanEntity", "搜索实体"},
                {"Scanning", "搜索实体中..."},
                {"Type", "类型"},
                {"SubType", "子类型"},
                {"Area", "区域"},
                {"Pos", "位置"},
                {"Vel", "速度"},
                {"FilterError", "过滤错误"},
                {"InvalidEntityCount", "错误的实体数量："},
                {"ScanError", "扫描错误："},
                {"SelectedEntity", "已选中实体"},
                {"RefreshedEntity", "已刷新实体"},
                {"TeleportToTarget", "传送至目标"},
                {"SwapPositions", "交换位置"},
                {"SetTarget", "设置目标"},
                {"Teleported", "已传送"},
                {"Swapped", "已交换位置"},
                {"FailWritePos", "写入位置失败"},
                {"TeleportError", "传送错误："},
                {"PositionUpdated", "位置已更新"},
                {"AppliedChanges", "已应用至实体"},
                {"Total_Entity", "总实体数: "},

                // 实体类型
                {"ENTITY_TYPE_ALL", "所有"},
                {"ENTITY_TYPE_HUMAN", "人类"},
                {"ENTITY_TYPE_ZOMBIE", "僵尸"},
                {"ENTITY_TYPE_ITEM", "物品"},
                {"ENTITY_TYPE_PROJECTILE", "投掷物"},
                {"ENTITY_TYPE_UNKNOWN", "未知"},
                {"Furniture", "家具"},
                {"Pickup", "可捡起"},
                {"Vehicle", "载具"},
                {"PickupSpec", "特殊"},

                // === 武器 ===
                {"ScanWeaponPool", "扫描武器池"},
                {"WeaponRefresh", "刷新武器槽"},
                {"WeaponSlot", "武器槽"},
                {"StorageWeapon", "仓库武器"},
                {"Stack", "数量"},              // Count和Stack统一
                {"NoDrop", "上锁"},
                {"WeaponScanError", "武器扫描错误"},
                {"WeaponApplySuccess", "武器应用成功"},
                {"WeaponApplyFailed", "武器应用失败"},

                // 资源
                {"GlobalResources", "全局资源"},
                {"PlayerResources", "玩家资源"},
                {"Food", "食物"},
                {"Gasoline", "汽油"},
                {"Medical", "医疗"},
                {"PistolAmmo", "手枪弹药"},
                {"RifleAmmo", "步枪弹药"},
                {"ShotgunAmmo", "霰弹弹药"},

                // 进阶
                {"NewMaxText", "取消属性上限"},
                {"Junk01", "尽管修改了代码, 但 CHAR_STAT_MAX和CHAR_STAT_MIN 还要你自己修改.\n在控制台里输入 {X to CHAR_STAT_MAX}和{X to CHAR_STAT_MIN}, X 为你想要的值."},
                {"Junk02", "但是这个工具也能做到，所以何意味？"},

                // 设置窗口
                {"Settings_Title", "设置"},
                {"Settings_Language", "语言:"},
                {"Settings_PlayerArrayOffset", "玩家数组偏移 (十六进制):"},
                {"Settings_PlayerStructSize", "玩家结构体大小 (十六进制):"},
                {"Settings_PlayerBaseOffset", "玩家基础属性偏移 (stat@):"},
                {"Settings_PlayerBonusOffset", "玩家加成属性偏移 (bonus@):"},
                {"Settings_EntityPoolOffset", "实体池偏移 (十六进制):"},
                {"Settings_EntitySize", "实体大小 (十六进制):"},
                {"Settings_EntityTypeOffset", "实体类型偏移 (十六进制):"},
                {"Settings_WeaponPoolOffset", "武器池偏移 (十六进制):"},
                {"Settings_WeaponSize", "武器结构体大小 (十六进制):"},
                {"Settings_MaxWeapons", "武器数量 (十六进制):"},
                {"Settings_StorageWeaponOffset", "仓库武器偏移 (十六进制):"},
                {"Settings_StorageWeaponSlots", "仓库武器数量:"},
                {"Settings_StorageWeaponSize", "仓库武器结构体大小 (十六进制):"},
            }
        };

        public static string Get(string key)
        {
            if (_translations.TryGetValue(CurrentLanguage, out var dict) && dict.TryGetValue(key, out var value))
                return value;
            return key;
        }

        /// <summary>
        /// 组合词：读取实体 = Read("Read") + Read("Entities")
        /// </summary>
        public static string Combine(string key1, string key2, string separator = " ")
        {
            return Get(key1) + separator + Get(key2);
        }

        /// <summary>
        /// 三词组合
        /// </summary>
        public static string Combine3(string k1, string k2, string k3, string sep = " ")
        {
            return Get(k1) + sep + Get(k2) + sep + Get(k3);
        }
    }
}