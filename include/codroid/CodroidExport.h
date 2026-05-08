/**
 * @file CodroidExport.h
 * @brief 动态库符号可见性：`CODROID_API` 在 Windows 上为 dllimport/dllexport，在 GCC >= 4 上为默认可见属性。
 *
 * 构建本 SDK 动态库时在目标上定义 `CODROID_EXPORTS`，以导出实现符号；客户仅链接头文件与导入库/`.so` 时不要定义该宏。
 */

#ifndef CODROID_EXPORT_H
#define CODROID_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef CODROID_EXPORTS
    #define CODROID_API __declspec(dllexport)
  #else
    #define CODROID_API __declspec(dllimport)
  #endif
#else
  #if __GNUC__ >= 4
    #define CODROID_API __attribute__ ((visibility ("default")))
  #else
    #define CODROID_API
  #endif
#endif

#endif
