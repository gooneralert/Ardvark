#pragma once

namespace Cheat {
namespace Features {

class LuaExecutor {
public:
    enum class LogLevel {
        Info,
        Print,
        Warn,
        Error,
        Success
    };

    static void Initialize();
    static void Shutdown();
    static void Render(float alpha = 1.0f);

    static void Log(LogLevel level, const char* fmt, ...);
    static void ClearOutput();
};

}
}
