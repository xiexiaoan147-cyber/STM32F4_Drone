#!/bin/sh
# 主机端 control.c 回归测试 (无需 arm 工具链)
cd "$(dirname "$0")"
gcc -I stubs -I ../../Core/Inc \
    test_control.c ../../Core/Src/control.c \
    -lm -o ctrltest && ./ctrltest
