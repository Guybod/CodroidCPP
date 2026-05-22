/**
 * @file console_utf8.hpp
 * @brief Windows 控制台 UTF-8 输出初始化（Linux 上为空操作）。
 *
 * 在 Ubuntu 上编写的 UTF-8 源码/字符串，在 Windows cmd 默认代码页（如 GBK）下
 * 直接 std::cout 中文会乱码。在 main() 开头调用一次 InitConsoleUtf8() 即可。
 *
 * 仍建议使用 Windows Terminal / PowerShell 7，或在批处理里 chcp 65001。
 */

#ifndef CODROID_CONSOLE_UTF8_HPP
#define CODROID_CONSOLE_UTF8_HPP

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace Codroid {

/** @brief 将当前进程的控制台输入/输出代码页设为 UTF-8（仅 Windows 生效）。 */
inline void InitConsoleUtf8() {
#ifdef _WIN32
    (void)SetConsoleOutputCP(CP_UTF8);
    (void)SetConsoleCP(CP_UTF8);
#endif
}

} // namespace Codroid

#endif
