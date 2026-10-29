#pragma once
// ============================================================
// UTF-8 资源 IO 单源（全引擎唯一文件读写入口）
//
// 存在的根因：MinGW CRT 的 fopen/ifstream 走 ANSI 代码页，
// Windows 中文用户名/中文目录下 UTF-8 路径直接打开失败
// （"no regular font" 的根源）。本模块统一提供：
//   assetsRoot()   资源根目录（exe 所在目录；非 Windows = CWD）
//   assetPath()    相对路径 → 资源根下完整路径（绝对路径直通）
//   openFileUtf8() UTF-8 路径 FILE* 打开（内部转宽字符 _wfopen）
//   readFileUtf8() 整文件读入字符串（JSON/文本统一入口）
//   writeFileUtf8() 字符串整文件写出
//
// 使用规则：任何资源/配置读写禁止直接 fopen/ifstream——
// 一律走本模块（中文路径全平台可用，一处修全局修）。
// ============================================================
#include <cstdio>
#include <string>

namespace ui {

// 资源根（首次调用锁定：Windows = exe 所在目录，其他平台 = 当前目录）。
// 修复 CWD 非资源根（IDE/脚本启动）时相对路径全部失效的问题。
const std::string& assetsRoot();

// 相对路径拼接：rel 相对资源根；绝对路径原样返回；空串返回根
std::string assetPath(const char* rel);

// UTF-8 路径打开（mode 语义同 fopen："rb"/"wb" 等）；
// Windows 内部 UTF-8→UTF-16 走 _wfopen，其他平台直通 fopen
FILE* openFileUtf8(const char* path, const char* mode);

// 整文件读入 out（二进制安全）；成功返回 true（空文件也成功）
bool readFileUtf8(const char* path, std::string& out);

// 字符串整体写出（二进制安全）；成功返回 true
bool writeFileUtf8(const char* path, const std::string& content);

} // namespace ui