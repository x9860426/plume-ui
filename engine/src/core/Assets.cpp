#include "Assets.h"
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ui {

// ---------------- 资源根定位 ----------------

#ifdef _WIN32
// exe 所在目录（GetModuleFileNameW → 去文件名留目录）
static std::string moduleDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return ".";
    std::string dir;
    int len = WideCharToMultiByte(CP_UTF8, 0, buf, (int)n, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return ".";
    dir.resize((size_t)len);
    WideCharToMultiByte(CP_UTF8, 0, buf, (int)n, &dir[0], len, nullptr, nullptr);
    // 去掉最后一个路径分隔符之后的部分（exe 文件名）
    size_t p = dir.find_last_of("\\/");
    return (p != std::string::npos) ? dir.substr(0, p) : ".";
}
#endif

const std::string& assetsRoot() {
    // 静态局部一次锁定：进程内所有资源路径共用同一根
    static const std::string root =
#ifdef _WIN32
        moduleDir();      // exe 目录（assets/themes 拷贝到输出目录 = exe 旁）
#else
        std::string("."); // 类 Unix：工作目录即根
#endif
    return root;
}

std::string assetPath(const char* rel) {
    if (!rel || !rel[0]) return assetsRoot();
    // 绝对路径直通（Windows 盘符 / 两个平台的开头分隔符）
#ifdef _WIN32
    if ((rel[1] == ':' && (rel[2] == '\\' || rel[2] == '/')) ||
        rel[0] == '\\' || rel[0] == '/') return rel;
#else
    if (rel[0] == '/') return rel;
#endif
    return assetsRoot() + "/" + rel;
}

// ---------------- UTF-8 → 宽字符（Windows 专用） ----------------

#ifdef _WIN32
static std::wstring toWide(const char* utf8) {
    int need = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (need <= 0) return std::wstring();
    std::wstring w((size_t)need, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], need);
    w.resize((size_t)need - 1);   // 去掉结尾宽字符 0（std::wstring 自带）
    return w;
}
#endif

// ---------------- IO 实现 ----------------

FILE* openFileUtf8(const char* path, const char* mode) {
    if (!path || !mode || !path[0]) return nullptr;
#ifdef _WIN32
    // mode 是纯 ASCII（"rb"/"wb"），逐字符转宽即可
    std::wstring wmode;
    for (const char* p = mode; *p; ++p) wmode.push_back((wchar_t)*p);
    return _wfopen(toWide(path).c_str(), wmode.c_str());
#else
    return std::fopen(path, mode);
#endif
}

bool readFileUtf8(const char* path, std::string& out) {
    FILE* f = openFileUtf8(path, "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    bool ok = (std::ferror(f) == 0);
    std::fclose(f);
    return ok;
}

bool writeFileUtf8(const char* path, const std::string& content) {
    FILE* f = openFileUtf8(path, "wb");
    if (!f) return false;
    bool ok = true;
    if (!content.empty()) {
        ok = (std::fwrite(content.data(), 1, content.size(), f) == content.size());
    }
    return (std::fclose(f) == 0) && ok;
}

} // namespace ui