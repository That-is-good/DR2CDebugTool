#include "translation.h"

#include <cstring>

namespace {

struct TranslationEntry {
    const char *en;   // key：必须与 UI 中的英文字面量完全一致
    const char *zh;   // 中文译文（UTF-8）
};

// ============================================================================
// 词条表
//   - key 用英文原文，未收录的 key 原样显示英文（可增量补词条）
//   - 同一个英文 key 只出现一次；多处共用同一 key 时译文相同
//   - 带格式符（%d / %u / %s 等）的 key，译文必须保留同样的格式符
// ============================================================================
const TranslationEntry kTranslations[] = {
    // ---------------- 调试面板 ----------------
    { "DR2C Debug",                u8"DR2C 调试" },
    { "Drag entity",               u8"拖动实体" },
    { "Log to file",               u8"日志写入文件" },
    { "Pick radius (world)",       u8"拾取半径（世界）" },
    { "Game FrameRate",            u8"游戏帧率" },
    { "Current Map",               u8"当前地图" },
    { "Right-click entity to edit",u8"右键点击实体以编辑" },
    { "Language",                  u8"语言" },
    { "(CJK font not found)",      u8"（未找到中文字体）" },

    // ---------------- 实体编辑弹窗 ----------------
    { "Entity Edit",               u8"实体编辑" },
    { "Slot %d   ID %u   Type %u / %u", u8"槽位 %d   ID %u   类型 %u / %u" },
    { "(entity no longer exists)", u8"（实体已不存在）" },
    { "Clone",                     u8"复制" },
    { "Destroy",                   u8"销毁" },
    { "Close",                     u8"关闭" },
    { "Entity",                    u8"实体" },
    { "Position",                  u8"位置" },
    { "Map",                       u8"地图" },
    { "Velocity",                  u8"速度" },
    { "Physics",                   u8"物理" },
    { "Hitpoints",                 u8"生命值" },
    { "Sprite",                    u8"精灵图" },
    { "AI state",                  u8"AI 状态" },
    { "Mass",                      u8"质量" },
    { "Friction",                  u8"阻力" },
    { "Bounce",                    u8"弹跳阻力" },
    { "No collision",              u8"无碰撞" },
    { "No pickup",                 u8"不可拾取" },
    { "Unseen",                    u8"不渲染" },
    { "Invisible",                 u8"不绘制" },
    { "No hit",                    u8"不可被击中" },
    { "Glow",                      u8"发光" },
    { "Character",                 u8"角色" },
    { "Item",                      u8"物品" },
    { "Vehicle",                   u8"载具" },

    // ---------------- 角色 ----------------
    { "(no character attached)",   u8"（未绑定角色）" },
    { "Name",                      u8"名称" },
    { "Health",                    u8"生命" },
    { "Speed Bonus",               u8"速度加成" },
    { "Control Human",             u8"控制该角色" },
    { "Stats",                     u8"属性" },
    { "         Base  Bonus  Total", u8"         基础   加成   总计" },
    { "Resources",                 u8"资源" },
    { "Weapons",                   u8"武器" },
    { "Slot %d",                   u8"槽位 %d" },
    { "(empty)",                   u8"（空）" },
    { "Stack",                     u8"数量" },
    { "Lock",                      u8"锁定" },
    // ---------------- 属性 / 资源 ----------------
    { "Morale",                    u8"士气" },
    { "Attitude",                  u8"态度" },
    { "Composure",                 u8"镇定" },
    { "Charm",                     u8"魅力" },
    { "Wits",                      u8"智慧" },
    { "Loyalty",                   u8"忠诚" },
    { "Medical",                   u8"医疗" },
    { "Mechanical",                u8"机械" },
    { "Shooting",                  u8"射击" },
    { "Strength",                  u8"力量" },
    { "Dexterity",                 u8"敏捷" },
    { "Fitness",                   u8"体质" },
    { "Vitality",                  u8"活力" },
    { "None",                      u8"无" },
    { "Food",                      u8"食物" },
    { "Gas",                       u8"汽油" },
    { "Bullet",                    u8"子弹" },
    { "Rifle",                     u8"步枪弹" },
    { "Shell",                     u8"霰弹" },
    { "Junk",                      u8"废料" },

    // ---------------- 物品 / 载具 ----------------
    { "Amount",                    u8"数量" },
    { "Loot",                      u8"战利品" },
    { "(invalid)",                 u8"（无效）" },
    { "Chassis",                   u8"底盘" },
    { "Chassis Max",               u8"底盘上限" },
    { "Engine",                    u8"引擎" },
    { "Engine Max",                u8"引擎上限" },
    { "Armour",                    u8"装甲" },
    { "Armour Max",                u8"装甲上限" },
    { "Speed",                     u8"速度" },
    { "Speed Max",                 u8"速度上限" },
    { "Repair",                    u8"维修" },
    { "MPG",                       u8"油耗 (MPG)" },
};

Dr2cLanguage g_language        = DR2C_LANGUAGE_ENGLISH;
bool         g_cjkFontAvailable = false;

} // namespace

void Dr2cSetLanguage(Dr2cLanguage language)
{
    if (language != DR2C_LANGUAGE_ENGLISH && language != DR2C_LANGUAGE_CHINESE)
        return;
    if (language == DR2C_LANGUAGE_CHINESE && !g_cjkFontAvailable)
        return;   // 没有中文字体时不允许切到中文，避免满屏方框
    g_language = language;
}

Dr2cLanguage Dr2cGetLanguage()
{
    return g_language;
}

void Dr2cSetCjkFontAvailable(bool available)
{
    g_cjkFontAvailable = available;
    if (!available && g_language == DR2C_LANGUAGE_CHINESE)
        g_language = DR2C_LANGUAGE_ENGLISH;
}

bool Dr2cIsCjkFontAvailable()
{
    return g_cjkFontAvailable;
}

const char *Dr2cTr(const char *englishKey)
{
    if (englishKey == nullptr || g_language == DR2C_LANGUAGE_ENGLISH)
        return englishKey;

    for (const TranslationEntry &entry : kTranslations) {
        // 先比指针（字面量可直接命中），再比内容
        if (entry.en == englishKey || std::strcmp(entry.en, englishKey) == 0)
            return entry.zh;
    }
    return englishKey;   // 未收录：回退英文
}

const char *Dr2cLanguageToString(Dr2cLanguage language)
{
    return (language == DR2C_LANGUAGE_CHINESE) ? "zh" : "en";
}

Dr2cLanguage Dr2cParseLanguage(const char *text, Dr2cLanguage fallback)
{
    if (text == nullptr)
        return fallback;
    if (std::strcmp(text, "zh") == 0)
        return DR2C_LANGUAGE_CHINESE;
    if (std::strcmp(text, "en") == 0)
        return DR2C_LANGUAGE_ENGLISH;
    return fallback;
}
