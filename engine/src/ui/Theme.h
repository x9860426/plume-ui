#pragma once
#include "../core/Math.h"
#include <cstdint>
#include <string>

namespace ui {

#define THEME_COUNT 35

struct ThemeColors {
    Color bg1, bg2, bg3, bg4;
    bool useGradient = false;
    bool antialias = true;

    Color text, title, accent, accent2;
    // 危险色（关闭按钮 hover/警告标题等），主题未指定时用统一默认红
    Color danger = Color::rgb(0xd9, 0x45, 0x4a);

    Color btnBg1, btnBg2, btnHover1, btnHover2, btnPressed, btnText;
    Color btnBorder, btnHoverBorder;
    float btnRadius = 4.0f;

    Color inputBg, inputBorder, inputText;
    float inputRadius = 4.0f;

    Color toggleOffBg, toggleOnBg, toggleOffBorder, toggleOnBorder;
    Color toggleOffSlider, toggleOnSlider;
    float toggleW = 50.0f, toggleH = 28.0f, toggleRadius = 14.0f;
    int toggleStyle = 0;

    Color sliderGroove, sliderHandle, sliderHandleBorder;

    Color checkBorder, checkBg, checkCheckedBg, checkCheckedBorder;
    float checkRadius = 3.0f;

    Color tabBg, tabSelBg, tabText, tabSelText;
    float tabRadius = 4.0f;

    Color menuBg, menuText, menuSelBg, menuSelText;
    Color toolbarBg, toolbarHoverBg;
    Color statusBg, statusText;
    Color groupBorder, groupTitleBg, groupTitle;
    float groupRadius = 4.0f;

    Color cardBg1, cardBg2, cardHoverBg1, cardHoverBg2;
    float cardRadius = 4.0f;

    Color listBg, listBorder, listText, listSelBg;
};

struct ThemeTexts {
    std::string windowTitle = "Simple UI";
    std::string titleText = "Simple UI Template";
    std::string statusMsg = "Ready";
    std::string cardTexts[4] = {"Login", "Settings", "About", "Message"};
    std::string demoGroupTitle = "Interactive Controls";
    std::string hSliderLabel = "Horizontal Slider:";
    std::string vSliderLabel = "Vertical Slider:";
    std::string toggleLabel = "Custom Toggle:";
    std::string check1Text = "Option 1";
    std::string check2Text = "Option 2";
    std::string comboLabel = "Dropdown:";
    std::string comboItems[4] = {"Item A", "Item B", "Item C", "Item D"};
    std::string tab1Title = "Overview";
    std::string tab2Title = "List";
    std::string tab1Content = "Custom content area with simple theme applied.";
    std::string listItems[4] = {"Item 1", "Item 2", "Item 3", "Item 4"};
    std::string refreshBtnText = "Refresh";
    std::string fontName = "Segoe UI";
    std::string menuLabels[3] = {"File", "View", "Help"};
    std::string loginTitle = "Welcome";
    std::string loginIcon = "[USER]";
    std::string settingsTitle = "Settings";
    std::string aboutTitle = "About";
};

struct ThemeData {
    ThemeColors colors;
    ThemeTexts texts;
    int bgType = 0;
    std::string name;
};

void InitThemes();
void LoadThemesFromJSON();
void SaveThemeToJSON(int index, const std::string& dir);

// 主题工作室导入/导出（任意路径 JSON，含名称/背景类型/颜色/文本完整数据）
bool ExportThemeToFile(int index, const std::string& filePath);
bool ImportThemeFromFile(int index, const std::string& filePath);

extern ThemeData g_themes[THEME_COUNT];
extern ThemeColors g_defaultThemeColors[THEME_COUNT];
extern int g_currentTheme;

inline ThemeData& currentTheme() { return g_themes[g_currentTheme]; }
inline ThemeColors& currentColors() { return g_themes[g_currentTheme].colors; }
inline ThemeTexts& currentTexts() { return g_themes[g_currentTheme].texts; }

} // namespace ui