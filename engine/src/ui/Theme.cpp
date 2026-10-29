#include "Theme.h"
#include "../core/Assets.h"
#include <nlohmann/json.hpp>
#include <cstdio>

namespace ui {

using json = nlohmann::json;

ThemeData g_themes[THEME_COUNT];
ThemeColors g_defaultThemeColors[THEME_COUNT];
int g_currentTheme = 0;

static void InitSimpleStyle() {
    ThemeData& theme = g_themes[0];
    theme.name = "简单风格";
    ThemeColors& c = theme.colors;
    c.bg1 = Color::rgb(0xf5, 0xf5, 0xf5);
    c.bg2 = Color::rgb(0xf5, 0xf5, 0xf5);
    c.bg3 = Color::rgb(0xf5, 0xf5, 0xf5);
    c.bg4 = Color::rgb(0xf5, 0xf5, 0xf5);
    c.useGradient = false;
    c.antialias = true;
    c.text = Color::rgb(0x33, 0x33, 0x33);
    c.title = Color::rgb(0x33, 0x33, 0x33);
    c.accent = Color::rgb(0x4a, 0x90, 0xd9);
    c.accent2 = Color::rgb(0x3a, 0x7b, 0xc8);
    c.btnBg1 = Color::rgb(0xe0, 0xe0, 0xe0);
    c.btnBg2 = Color::rgb(0xe0, 0xe0, 0xe0);
    c.btnHover1 = Color::rgb(0xd5, 0xd5, 0xd5);
    c.btnHover2 = Color::rgb(0xd5, 0xd5, 0xd5);
    c.btnPressed = Color::rgb(0xc0, 0xc0, 0xc0);
    c.btnText = Color::rgb(0x33, 0x33, 0x33);
    c.btnBorder = Color::rgb(0xcc, 0xcc, 0xcc);
    c.btnHoverBorder = Color::rgb(0xaa, 0xaa, 0xaa);
    c.btnRadius = 4.0f;
    c.inputBg = Color::rgb(0xff, 0xff, 0xff);
    c.inputBorder = Color::rgb(0xcc, 0xcc, 0xcc);
    c.inputText = Color::rgb(0x33, 0x33, 0x33);
    c.inputRadius = 4.0f;
    c.toggleOffBg = Color::rgb(0xcc, 0xcc, 0xcc);
    c.toggleOnBg = Color::rgb(0x4a, 0x90, 0xd9);
    c.toggleOffBorder = Color::rgb(0xbb, 0xbb, 0xbb);
    c.toggleOnBorder = Color::rgb(0x3a, 0x7b, 0xc8);
    c.toggleOffSlider = Color::rgb(0xff, 0xff, 0xff);
    c.toggleOnSlider = Color::rgb(0xff, 0xff, 0xff);
    c.toggleW = 50.0f;
    c.toggleH = 28.0f;
    c.toggleRadius = 14.0f;
    c.toggleStyle = 0;
    c.sliderGroove = Color::rgb(0xdd, 0xdd, 0xdd);
    c.sliderHandle = Color::rgb(0x4a, 0x90, 0xd9);
    c.sliderHandleBorder = Color::rgb(0x4a, 0x90, 0xd9);
    c.checkBorder = Color::rgb(0xaa, 0xaa, 0xaa);
    c.checkBg = Color::rgb(0xff, 0xff, 0xff);
    c.checkCheckedBg = Color::rgb(0x4a, 0x90, 0xd9);
    c.checkCheckedBorder = Color::rgb(0x3a, 0x7b, 0xc8);
    c.checkRadius = 3.0f;
    c.tabBg = Color::rgb(0xe8, 0xe8, 0xe8);
    c.tabSelBg = Color::rgb(0xff, 0xff, 0xff);
    c.tabText = Color::rgb(0x55, 0x55, 0x55);
    c.tabSelText = Color::rgb(0x33, 0x33, 0x33);
    c.tabRadius = 4.0f;
    c.menuBg = Color::rgb(0xf0, 0xf0, 0xf0);
    c.menuText = Color::rgb(0x33, 0x33, 0x33);
    c.menuSelBg = Color::rgb(0xe0, 0xe0, 0xe0);
    c.menuSelText = Color::rgb(0x33, 0x33, 0x33);
    c.toolbarBg = Color::rgb(0xf8, 0xf8, 0xf8);
    c.toolbarHoverBg = Color::rgb(0xe0, 0xe0, 0xe0);
    c.statusBg = Color::rgb(0xf0, 0xf0, 0xf0);
    c.statusText = Color::rgb(0x55, 0x55, 0x55);
    c.groupBorder = Color::rgb(0xcc, 0xcc, 0xcc);
    c.groupTitleBg = Color::rgb(0xff, 0xff, 0xff);
    c.groupTitle = Color::rgb(0x44, 0x44, 0x44);
    c.groupRadius = 4.0f;
    c.cardBg1 = Color::rgb(0xe0, 0xe0, 0xe0);
    c.cardBg2 = Color::rgb(0xe0, 0xe0, 0xe0);
    c.cardHoverBg1 = Color::rgb(0xd5, 0xd5, 0xd5);
    c.cardHoverBg2 = Color::rgb(0xd5, 0xd5, 0xd5);
    c.cardRadius = 4.0f;
    c.listBg = Color::rgb(0xff, 0xff, 0xff);
    c.listBorder = Color::rgb(0xcc, 0xcc, 0xcc);
    c.listText = Color::rgb(0x33, 0x33, 0x33);
    c.listSelBg = Color::rgb(0x4a, 0x90, 0xd9);

    ThemeTexts& t = theme.texts;
    t.windowTitle = "简单风格 UI 模板";
    t.titleText = "简单 UI 模板集合";
    t.statusMsg = "就绪";
    t.cardTexts[0] = "登录窗口";
    t.cardTexts[1] = "设置窗口";
    t.cardTexts[2] = "关于窗口";
    t.cardTexts[3] = "消息盒子";
    t.demoGroupTitle = "交互控件演示";
    t.hSliderLabel = "水平滑动条：";
    t.vSliderLabel = "垂直滑动条：";
    t.toggleLabel = "自定义开关：";
    t.check1Text = "选项 1";
    t.check2Text = "选项 2";
    t.comboLabel = "下拉选择：";
    t.comboItems[0] = "选项 A";
    t.comboItems[1] = "选项 B";
    t.comboItems[2] = "选项 C";
    t.comboItems[3] = "选项 D";
    t.tab1Title = "概览";
    t.tab2Title = "列表";
    t.tab1Content = "这里可以放置任何自定义内容，所有控件自动应用极简主题。";
    t.listItems[0] = "项目 1";
    t.listItems[1] = "项目 2";
    t.listItems[2] = "项目 3";
    t.listItems[3] = "项目 4";
    t.refreshBtnText = "刷新样式";
    t.fontName = "Segoe UI";
    t.menuLabels[0] = "文件";
    t.menuLabels[1] = "视图";
    t.menuLabels[2] = "帮助";
    t.loginTitle = "欢迎回来";
    t.loginIcon = "[USER]";
    t.settingsTitle = "设置";
    t.aboutTitle = "关于";

    theme.bgType = 0;
}

static json colorToJson(const Color& c) {
    return json::array({c.r, c.g, c.b});
}

static Color jsonToColor(const json& j) {
    if (j.is_array() && j.size() >= 3) {
        return Color::rgb(j[0].get<int>(), j[1].get<int>(), j[2].get<int>());
    }
    return Color::rgb(0, 0, 0);
}

static json themeToJson(const ThemeData& theme) {
    json j;
    j["name"] = theme.name;
    j["bgType"] = theme.bgType;

    const ThemeColors& c = theme.colors;
    json& colors = j["colors"];
    colors["bg1"] = colorToJson(c.bg1);
    colors["bg2"] = colorToJson(c.bg2);
    colors["bg3"] = colorToJson(c.bg3);
    colors["bg4"] = colorToJson(c.bg4);
    colors["useGradient"] = c.useGradient;
    colors["antialias"] = c.antialias;
    colors["text"] = colorToJson(c.text);
    colors["title"] = colorToJson(c.title);
    colors["accent"] = colorToJson(c.accent);
    colors["accent2"] = colorToJson(c.accent2);
    colors["btnBg1"] = colorToJson(c.btnBg1);
    colors["btnBg2"] = colorToJson(c.btnBg2);
    colors["btnHover1"] = colorToJson(c.btnHover1);
    colors["btnHover2"] = colorToJson(c.btnHover2);
    colors["btnPressed"] = colorToJson(c.btnPressed);
    colors["btnText"] = colorToJson(c.btnText);
    colors["btnBorder"] = colorToJson(c.btnBorder);
    colors["btnHoverBorder"] = colorToJson(c.btnHoverBorder);
    colors["btnRadius"] = c.btnRadius;
    colors["inputBg"] = colorToJson(c.inputBg);
    colors["inputBorder"] = colorToJson(c.inputBorder);
    colors["inputText"] = colorToJson(c.inputText);
    colors["inputRadius"] = c.inputRadius;
    colors["toggleOffBg"] = colorToJson(c.toggleOffBg);
    colors["toggleOnBg"] = colorToJson(c.toggleOnBg);
    colors["toggleOffBorder"] = colorToJson(c.toggleOffBorder);
    colors["toggleOnBorder"] = colorToJson(c.toggleOnBorder);
    colors["toggleOffSlider"] = colorToJson(c.toggleOffSlider);
    colors["toggleOnSlider"] = colorToJson(c.toggleOnSlider);
    colors["toggleW"] = c.toggleW;
    colors["toggleH"] = c.toggleH;
    colors["toggleRadius"] = c.toggleRadius;
    colors["toggleStyle"] = c.toggleStyle;
    colors["sliderGroove"] = colorToJson(c.sliderGroove);
    colors["sliderHandle"] = colorToJson(c.sliderHandle);
    colors["sliderHandleBorder"] = colorToJson(c.sliderHandleBorder);
    colors["checkBorder"] = colorToJson(c.checkBorder);
    colors["checkBg"] = colorToJson(c.checkBg);
    colors["checkCheckedBg"] = colorToJson(c.checkCheckedBg);
    colors["checkCheckedBorder"] = colorToJson(c.checkCheckedBorder);
    colors["checkRadius"] = c.checkRadius;
    colors["tabBg"] = colorToJson(c.tabBg);
    colors["tabSelBg"] = colorToJson(c.tabSelBg);
    colors["tabText"] = colorToJson(c.tabText);
    colors["tabSelText"] = colorToJson(c.tabSelText);
    colors["tabRadius"] = c.tabRadius;
    colors["menuBg"] = colorToJson(c.menuBg);
    colors["menuText"] = colorToJson(c.menuText);
    colors["menuSelBg"] = colorToJson(c.menuSelBg);
    colors["menuSelText"] = colorToJson(c.menuSelText);
    colors["toolbarBg"] = colorToJson(c.toolbarBg);
    colors["toolbarHoverBg"] = colorToJson(c.toolbarHoverBg);
    colors["statusBg"] = colorToJson(c.statusBg);
    colors["statusText"] = colorToJson(c.statusText);
    colors["groupBorder"] = colorToJson(c.groupBorder);
    colors["groupTitleBg"] = colorToJson(c.groupTitleBg);
    colors["groupTitle"] = colorToJson(c.groupTitle);
    colors["groupRadius"] = c.groupRadius;
    colors["cardBg1"] = colorToJson(c.cardBg1);
    colors["cardBg2"] = colorToJson(c.cardBg2);
    colors["cardHoverBg1"] = colorToJson(c.cardHoverBg1);
    colors["cardHoverBg2"] = colorToJson(c.cardHoverBg2);
    colors["cardRadius"] = c.cardRadius;
    colors["listBg"] = colorToJson(c.listBg);
    colors["listBorder"] = colorToJson(c.listBorder);
    colors["listText"] = colorToJson(c.listText);
    colors["listSelBg"] = colorToJson(c.listSelBg);

    const ThemeTexts& t = theme.texts;
    json& texts = j["texts"];
    texts["windowTitle"] = t.windowTitle;
    texts["titleText"] = t.titleText;
    texts["statusMsg"] = t.statusMsg;
    texts["cardTexts"] = json::array({t.cardTexts[0], t.cardTexts[1], t.cardTexts[2], t.cardTexts[3]});
    texts["demoGroupTitle"] = t.demoGroupTitle;
    texts["hSliderLabel"] = t.hSliderLabel;
    texts["vSliderLabel"] = t.vSliderLabel;
    texts["toggleLabel"] = t.toggleLabel;
    texts["check1Text"] = t.check1Text;
    texts["check2Text"] = t.check2Text;
    texts["comboLabel"] = t.comboLabel;
    texts["comboItems"] = json::array({t.comboItems[0], t.comboItems[1], t.comboItems[2], t.comboItems[3]});
    texts["tab1Title"] = t.tab1Title;
    texts["tab2Title"] = t.tab2Title;
    texts["tab1Content"] = t.tab1Content;
    texts["listItems"] = json::array({t.listItems[0], t.listItems[1], t.listItems[2], t.listItems[3]});
    texts["refreshBtnText"] = t.refreshBtnText;
    texts["fontName"] = t.fontName;
    texts["menuLabels"] = json::array({t.menuLabels[0], t.menuLabels[1], t.menuLabels[2]});
    texts["loginTitle"] = t.loginTitle;
    texts["loginIcon"] = t.loginIcon;
    texts["settingsTitle"] = t.settingsTitle;
    texts["aboutTitle"] = t.aboutTitle;

    return j;
}

static void jsonToTheme(const json& j, ThemeData& theme) {
    if (j.contains("name")) theme.name = j["name"].get<std::string>();
    if (j.contains("bgType")) theme.bgType = j["bgType"].get<int>();

    if (j.contains("colors")) {
        const json& c = j["colors"];
        ThemeColors& col = theme.colors;
        if (c.contains("bg1")) col.bg1 = jsonToColor(c["bg1"]);
        if (c.contains("bg2")) col.bg2 = jsonToColor(c["bg2"]);
        if (c.contains("bg3")) col.bg3 = jsonToColor(c["bg3"]);
        if (c.contains("bg4")) col.bg4 = jsonToColor(c["bg4"]);
        if (c.contains("useGradient")) col.useGradient = c["useGradient"].get<bool>();
        if (c.contains("antialias")) col.antialias = c["antialias"].get<bool>();
        if (c.contains("text")) col.text = jsonToColor(c["text"]);
        if (c.contains("title")) col.title = jsonToColor(c["title"]);
        if (c.contains("accent")) col.accent = jsonToColor(c["accent"]);
        if (c.contains("accent2")) col.accent2 = jsonToColor(c["accent2"]);
        if (c.contains("btnBg1")) col.btnBg1 = jsonToColor(c["btnBg1"]);
        if (c.contains("btnBg2")) col.btnBg2 = jsonToColor(c["btnBg2"]);
        if (c.contains("btnHover1")) col.btnHover1 = jsonToColor(c["btnHover1"]);
        if (c.contains("btnHover2")) col.btnHover2 = jsonToColor(c["btnHover2"]);
        if (c.contains("btnPressed")) col.btnPressed = jsonToColor(c["btnPressed"]);
        if (c.contains("btnText")) col.btnText = jsonToColor(c["btnText"]);
        if (c.contains("btnBorder")) col.btnBorder = jsonToColor(c["btnBorder"]);
        if (c.contains("btnHoverBorder")) col.btnHoverBorder = jsonToColor(c["btnHoverBorder"]);
        if (c.contains("btnRadius")) col.btnRadius = c["btnRadius"].get<float>();
        if (c.contains("inputBg")) col.inputBg = jsonToColor(c["inputBg"]);
        if (c.contains("inputBorder")) col.inputBorder = jsonToColor(c["inputBorder"]);
        if (c.contains("inputText")) col.inputText = jsonToColor(c["inputText"]);
        if (c.contains("inputRadius")) col.inputRadius = c["inputRadius"].get<float>();
        if (c.contains("toggleOffBg")) col.toggleOffBg = jsonToColor(c["toggleOffBg"]);
        if (c.contains("toggleOnBg")) col.toggleOnBg = jsonToColor(c["toggleOnBg"]);
        if (c.contains("toggleOffBorder")) col.toggleOffBorder = jsonToColor(c["toggleOffBorder"]);
        if (c.contains("toggleOnBorder")) col.toggleOnBorder = jsonToColor(c["toggleOnBorder"]);
        if (c.contains("toggleOffSlider")) col.toggleOffSlider = jsonToColor(c["toggleOffSlider"]);
        if (c.contains("toggleOnSlider")) col.toggleOnSlider = jsonToColor(c["toggleOnSlider"]);
        if (c.contains("toggleW")) col.toggleW = c["toggleW"].get<float>();
        if (c.contains("toggleH")) col.toggleH = c["toggleH"].get<float>();
        if (c.contains("toggleRadius")) col.toggleRadius = c["toggleRadius"].get<float>();
        if (c.contains("toggleStyle")) col.toggleStyle = c["toggleStyle"].get<int>();
        if (c.contains("sliderGroove")) col.sliderGroove = jsonToColor(c["sliderGroove"]);
        if (c.contains("sliderHandle")) col.sliderHandle = jsonToColor(c["sliderHandle"]);
        if (c.contains("sliderHandleBorder")) col.sliderHandleBorder = jsonToColor(c["sliderHandleBorder"]);
        if (c.contains("checkBorder")) col.checkBorder = jsonToColor(c["checkBorder"]);
        if (c.contains("checkBg")) col.checkBg = jsonToColor(c["checkBg"]);
        if (c.contains("checkCheckedBg")) col.checkCheckedBg = jsonToColor(c["checkCheckedBg"]);
        if (c.contains("checkCheckedBorder")) col.checkCheckedBorder = jsonToColor(c["checkCheckedBorder"]);
        if (c.contains("checkRadius")) col.checkRadius = c["checkRadius"].get<float>();
        if (c.contains("tabBg")) col.tabBg = jsonToColor(c["tabBg"]);
        if (c.contains("tabSelBg")) col.tabSelBg = jsonToColor(c["tabSelBg"]);
        if (c.contains("tabText")) col.tabText = jsonToColor(c["tabText"]);
        if (c.contains("tabSelText")) col.tabSelText = jsonToColor(c["tabSelText"]);
        if (c.contains("tabRadius")) col.tabRadius = c["tabRadius"].get<float>();
        if (c.contains("menuBg")) col.menuBg = jsonToColor(c["menuBg"]);
        if (c.contains("menuText")) col.menuText = jsonToColor(c["menuText"]);
        if (c.contains("menuSelBg")) col.menuSelBg = jsonToColor(c["menuSelBg"]);
        if (c.contains("menuSelText")) col.menuSelText = jsonToColor(c["menuSelText"]);
        if (c.contains("toolbarBg")) col.toolbarBg = jsonToColor(c["toolbarBg"]);
        if (c.contains("toolbarHoverBg")) col.toolbarHoverBg = jsonToColor(c["toolbarHoverBg"]);
        if (c.contains("statusBg")) col.statusBg = jsonToColor(c["statusBg"]);
        if (c.contains("statusText")) col.statusText = jsonToColor(c["statusText"]);
        if (c.contains("groupBorder")) col.groupBorder = jsonToColor(c["groupBorder"]);
        if (c.contains("groupTitleBg")) col.groupTitleBg = jsonToColor(c["groupTitleBg"]);
        if (c.contains("groupTitle")) col.groupTitle = jsonToColor(c["groupTitle"]);
        if (c.contains("groupRadius")) col.groupRadius = c["groupRadius"].get<float>();
        if (c.contains("cardBg1")) col.cardBg1 = jsonToColor(c["cardBg1"]);
        if (c.contains("cardBg2")) col.cardBg2 = jsonToColor(c["cardBg2"]);
        if (c.contains("cardHoverBg1")) col.cardHoverBg1 = jsonToColor(c["cardHoverBg1"]);
        if (c.contains("cardHoverBg2")) col.cardHoverBg2 = jsonToColor(c["cardHoverBg2"]);
        if (c.contains("cardRadius")) col.cardRadius = c["cardRadius"].get<float>();
        if (c.contains("listBg")) col.listBg = jsonToColor(c["listBg"]);
        if (c.contains("listBorder")) col.listBorder = jsonToColor(c["listBorder"]);
        if (c.contains("listText")) col.listText = jsonToColor(c["listText"]);
        if (c.contains("listSelBg")) col.listSelBg = jsonToColor(c["listSelBg"]);
    }

    if (j.contains("texts")) {
        const json& t = j["texts"];
        ThemeTexts& txt = theme.texts;
        if (t.contains("windowTitle")) txt.windowTitle = t["windowTitle"].get<std::string>();
        if (t.contains("titleText")) txt.titleText = t["titleText"].get<std::string>();
        if (t.contains("statusMsg")) txt.statusMsg = t["statusMsg"].get<std::string>();
        if (t.contains("cardTexts") && t["cardTexts"].is_array()) {
            for (int i = 0; i < 4 && i < (int)t["cardTexts"].size(); i++)
                txt.cardTexts[i] = t["cardTexts"][i].get<std::string>();
        }
        if (t.contains("demoGroupTitle")) txt.demoGroupTitle = t["demoGroupTitle"].get<std::string>();
        if (t.contains("hSliderLabel")) txt.hSliderLabel = t["hSliderLabel"].get<std::string>();
        if (t.contains("vSliderLabel")) txt.vSliderLabel = t["vSliderLabel"].get<std::string>();
        if (t.contains("toggleLabel")) txt.toggleLabel = t["toggleLabel"].get<std::string>();
        if (t.contains("check1Text")) txt.check1Text = t["check1Text"].get<std::string>();
        if (t.contains("check2Text")) txt.check2Text = t["check2Text"].get<std::string>();
        if (t.contains("comboLabel")) txt.comboLabel = t["comboLabel"].get<std::string>();
        if (t.contains("comboItems") && t["comboItems"].is_array()) {
            for (int i = 0; i < 4 && i < (int)t["comboItems"].size(); i++)
                txt.comboItems[i] = t["comboItems"][i].get<std::string>();
        }
        if (t.contains("tab1Title")) txt.tab1Title = t["tab1Title"].get<std::string>();
        if (t.contains("tab2Title")) txt.tab2Title = t["tab2Title"].get<std::string>();
        if (t.contains("tab1Content")) txt.tab1Content = t["tab1Content"].get<std::string>();
        if (t.contains("listItems") && t["listItems"].is_array()) {
            for (int i = 0; i < 4 && i < (int)t["listItems"].size(); i++)
                txt.listItems[i] = t["listItems"][i].get<std::string>();
        }
        if (t.contains("refreshBtnText")) txt.refreshBtnText = t["refreshBtnText"].get<std::string>();
        if (t.contains("fontName")) txt.fontName = t["fontName"].get<std::string>();
        if (t.contains("menuLabels") && t["menuLabels"].is_array()) {
            for (int i = 0; i < 3 && i < (int)t["menuLabels"].size(); i++)
                txt.menuLabels[i] = t["menuLabels"][i].get<std::string>();
        }
        if (t.contains("loginTitle")) txt.loginTitle = t["loginTitle"].get<std::string>();
        if (t.contains("loginIcon")) txt.loginIcon = t["loginIcon"].get<std::string>();
        if (t.contains("settingsTitle")) txt.settingsTitle = t["settingsTitle"].get<std::string>();
        if (t.contains("aboutTitle")) txt.aboutTitle = t["aboutTitle"].get<std::string>();
    }
}

static const char* g_themeNames[THEME_COUNT] = {
    "简单风格", "时间之主", "电玩国度", "萌趣乐园", "深空矩阵",
    "像素街机", "吸血鬼古堡", "天空风格", "深空星舰", "暗黑",
    "赛博朋克", "国风", "塔罗牌", "复古船", "夏天",
    "天使与恶魔", "幽灵", "拼图", "星空", "春天",
    "曙光", "月亮", "木头", "机器人", "死神",
    "河流", "海洋", "火焰", "狼人", "王国",
    "电脑", "使命", "冬天", "轮回", "魔法风格"
};

void InitThemes() {
    InitSimpleStyle();
    for (int i = 1; i < THEME_COUNT; i++) {
        g_themes[i].name = g_themeNames[i];
    }
    LoadThemesFromJSON();
    for (int i = 0; i < THEME_COUNT; i++) {
        g_defaultThemeColors[i] = g_themes[i].colors;
    }
}

void LoadThemesFromJSON() {
    for (int i = 1; i < THEME_COUNT; i++) {
        char path[256];
        std::snprintf(path, sizeof(path), "themes/theme_%02d.json", i);
        std::string text;
        if (readFileUtf8(path, text)) {
            try {
                json j = json::parse(text);
                jsonToTheme(j, g_themes[i]);
            } catch (...) {
            }
        }
    }
}

void SaveThemeToJSON(int index, const std::string& dir) {
    if (index < 0 || index >= THEME_COUNT) return;
    char path[256];
    std::snprintf(path, sizeof(path), "%s/theme_%02d.json", dir.c_str(), index);
    json j = themeToJson(g_themes[index]);
    writeFileUtf8(path, j.dump(2));
}

bool ExportThemeToFile(int index, const std::string& filePath) {
    if (index < 0 || index >= THEME_COUNT || filePath.empty()) return false;
    json j = themeToJson(g_themes[index]);
    return writeFileUtf8(filePath.c_str(), j.dump(2));

}

bool ImportThemeFromFile(int index, const std::string& filePath) {
    if (index < 0 || index >= THEME_COUNT || filePath.empty()) return false;
    std::string text;
    if (!readFileUtf8(filePath.c_str(), text)) return false;
    try {
        json j = json::parse(text);
        jsonToTheme(j, g_themes[index]);   // 名称/背景类型/颜色/文本整体应用
        return true;
    } catch (...) {
        return false;   // JSON 损坏/格式错误：保持原主题不变
    }
}

} // namespace ui