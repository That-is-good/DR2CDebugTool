#ifndef DR2C_TRANSLATION_H
#define DR2C_TRANSLATION_H

// ============================================================================
// translation.h
// 界面中英文切换：
//   - 词条以“英文原文”为 key（类似 Qt 的 tr()），未收录时原样返回英文
//   - 语言选择由 UI 下拉框切换，可落盘到 DLL 同目录的 .ini
//   - 中文显示依赖系统 CJK 字体，未加载到字体时禁用“中文”选项
// ============================================================================

enum Dr2cLanguage {
    DR2C_LANGUAGE_ENGLISH = 0,
    DR2C_LANGUAGE_CHINESE = 1
};

void         Dr2cSetLanguage(Dr2cLanguage language);
Dr2cLanguage Dr2cGetLanguage();

// 中文字体是否可用
void Dr2cSetCjkFontAvailable(bool available);
bool Dr2cIsCjkFontAvailable();

// 取当前语言文本；英文模式或未收录的 key 原样返回
const char *Dr2cTr(const char *englishKey);
inline const char *Tr(const char *englishKey) { return Dr2cTr(englishKey); }

// 配置文件中的语言字符串（"en" / "zh"）
const char  *Dr2cLanguageToString(Dr2cLanguage language);
Dr2cLanguage Dr2cParseLanguage(const char *text, Dr2cLanguage fallback);

#endif
