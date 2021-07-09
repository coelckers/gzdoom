#include "imgui.h"
#include <cstdio>

namespace ImGui
{
    bool ParseColor(const char* s, ImU32* col, int* skipChars);
    void TextAnsiColored(const ImVec4& col, const char* fmt, ...);
}