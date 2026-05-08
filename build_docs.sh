#!/bin/bash

# 确保在根目录运行
if [ ! -f "Doxyfile" ]; then
    echo "Error: Doxyfile not found in current directory."
    exit 1
fi

# 1. 清理并创建 docs 目录
rm -rf docs
mkdir -p docs

# 2. 生成英文版 (HTML + LaTeX)
echo "--------------------------------------------------"
echo "[1/3] Generating English Documentation..."
(cat Doxyfile; \
 echo "OUTPUT_LANGUAGE=English"; \
 echo "ENABLED_SECTIONS=ENGLISH"; \
 echo "OUTPUT_DIRECTORY=docs/en") | doxygen - > /dev/null

# 3. 生成中文版 (HTML + LaTeX)
echo "[2/3] Generating Chinese Documentation..."
(cat Doxyfile; \
 echo "OUTPUT_LANGUAGE=Chinese"; \
 echo "ENABLED_SECTIONS=CHINESE"; \
 echo "OUTPUT_DIRECTORY=docs/zh") | doxygen - > /dev/null

# --- 核心修复：注入中文支持与符号映射 ---
echo "[3/3] Patching and Compiling Chinese PDF..."

if [ -d "docs/zh/latex" ]; then
    cd docs/zh/latex
    
    # 在 \begin{document} 之前插入中文包、字体设置和符号映射
    # 使用 sed 匹配并替换，注入 xeCJK 和 Emoji 兼容逻辑
    sed -i '/\\begin{document}/i \
\\usepackage{xeCJK} \
\\setCJKmainfont{WenQuanYi Micro Hei} \
\\setCJKmonofont{WenQuanYi Micro Hei Mono} \
\\usepackage{newunicodechar} \
\\newunicodechar{🌟}{\\textasteriskcentered} \
\\newunicodechar{📁}{[Dir]} \
\\newunicodechar{🛠}{[Build]} \
\\newunicodechar{💻}{[PC]} \
\\newunicodechar{🚀}{[Fast]} \
\\newunicodechar{⚠}{[Warn]} \
\\newunicodechar{✅}{[OK]} \
\\newunicodechar{❌}{[Error]}' refman.tex

    # 使用 xelatex 编译（静默模式）
    echo "  -> Running xelatex (Pass 1)..."
    xelatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
    echo "  -> Running xelatex (Pass 2)..."
    xelatex -interaction=nonstopmode refman.tex > /dev/null 2>&1

    if [ -f "refman.pdf" ]; then
        cp refman.pdf ../../Codroid_SDK_Manual_ZH.pdf
        echo "  -> SUCCESS: docs/Codroid_SDK_Manual_ZH.pdf"
    else
        echo "  -> ERROR: Chinese PDF compilation failed. Check docs/zh/latex/refman.log"
    fi
    cd ../../..
fi

# 4. 编译英文 PDF (可选)
if [ -d "docs/en/latex" ]; then
    echo "Compiling English PDF..."
    cd docs/en/latex
    xelatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
    if [ -f "refman.pdf" ]; then
        cp refman.pdf ../../Codroid_SDK_Manual_EN.pdf
        echo "  -> SUCCESS: docs/Codroid_SDK_Manual_EN.pdf"
    fi
    cd ../../..
fi

echo "=================================================="
echo "Build finished. Check the 'docs/' directory."