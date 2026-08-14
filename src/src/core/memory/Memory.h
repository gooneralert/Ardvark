#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <stdexcept>

class Memory
{
public:
    Memory() = default;
    explicit Memory(const wchar_t* processName);
    explicit Memory(DWORD pid);
    ~Memory();

    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    // цепляемся к роблоксу
    bool Attach(const wchar_t* processName);
    bool Attach(DWORD pid);
    void Detach();

    bool IsAttached() const { return m_handle != nullptr; }
    bool IsAlive() const; // жив ли процесс
    bool GetExitCode(DWORD* out_code) const;
    DWORD GetPID() const { return m_pid; }
    HANDLE GetHandle() const { return m_handle; }

    // база модуля, кэшится
    uintptr_t GetModuleBase(const wchar_t* moduleName = nullptr) const;

    template<typename T>
    T Read(uintptr_t address) const
    {
        T val{};
        ReadProcessMemory(m_handle,
            reinterpret_cast<LPCVOID>(address),
            &val, sizeof(T), nullptr);
        return val;
    }

    SIZE_T ReadRaw(uintptr_t address, void* buf, SIZE_T bytes) const;

    template<typename T>
    bool Write(uintptr_t address, const T& value) const
    {
        return WriteProcessMemory(m_handle,
            reinterpret_cast<LPVOID>(address),
            &value, sizeof(T), nullptr);
    }

    SIZE_T WriteRaw(uintptr_t address, const void* buf, SIZE_T bytes) const;

    bool IsValid(uintptr_t address) const;
    bool IsWritable(uintptr_t address, SIZE_T size = 8) const;
    std::string ReadString(std::uint64_t address) const;
    uintptr_t ResolvePointer(uintptr_t base,
        const std::initializer_list<uintptr_t>& offsets) const;

    uintptr_t Alloc(SIZE_T size, DWORD protect = PAGE_EXECUTE_READWRITE) const;
    void Free(uintptr_t address) const;
    bool Protect(uintptr_t address, SIZE_T size, DWORD new_protect,
                 DWORD* old_protect = nullptr) const;

private:
    HANDLE m_handle = nullptr;
    DWORD m_pid = 0;

    mutable DWORD m_cachedBasePid = 0;
    mutable uintptr_t m_cachedMainBase = 0;

    static DWORD FindPID(const wchar_t* processName);
};

inline Memory g_Memory;
