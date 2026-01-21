#!/bin/bash
# Quick syntax check for Windows headers using clang + xwin
# This catches issues like missing includes without needing full build setup

XWIN_PATH="$HOME/.xwin"
CLANG="/opt/homebrew/opt/llvm/bin/clang++"

if [ ! -d "$XWIN_PATH" ]; then
    echo "xwin not installed. Run: xwin --accept-license splat --output ~/.xwin"
    exit 1
fi

CLANG_FLAGS=(
    -target x86_64-pc-windows-msvc
    -fms-compatibility-version=19.40
    -fms-extensions
    -std=c++20
    -isystem "$XWIN_PATH/crt/include"
    -isystem "$XWIN_PATH/sdk/include/ucrt"
    -isystem "$XWIN_PATH/sdk/include/um"
    -isystem "$XWIN_PATH/sdk/include/shared"
    -isystem "$XWIN_PATH/sdk/include/winrt"
    -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Dinterface=struct
    -fsyntax-only
)

# Test specific headers that caused issues
echo "Testing Windows SDK header availability..."

test_header() {
    echo -n "  $1: "
    echo "#include <$1>" | $CLANG "${CLANG_FLAGS[@]}" -x c++ - 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "OK"
        return 0
    else
        echo "MISSING"
        return 1
    fi
}

FAILED=0
test_header "Windows.h" || FAILED=1
test_header "d3d11.h" || FAILED=1
test_header "dxgi.h" || FAILED=1
test_header "dxgi1_5.h" || FAILED=1
test_header "dwmapi.h" || FAILED=1
test_header "Magnification.h" || FAILED=1
test_header "wrl/client.h" || FAILED=1
test_header "UIAutomationClient.h" || FAILED=1
test_header "sapi.h" || FAILED=1

if [ $FAILED -eq 0 ]; then
    echo "All headers found!"
    exit 0
else
    echo "Some headers missing!"
    exit 1
fi
