#include "pch.h"
#include "Memory.h"
#include <vector>

Memory::Memory(const wchar_t* processName) { Attach(processName); }
Memory::Memory(DWORD pid) { Attach(pid); }

Memory::~Memory() { Detach(); }

bool Memory::Attach(const wchar_t* processName)
{
    DWORD pid = FindPID(processName);
    if (!pid)
        return false;
    return Attach(pid);
}

// открываем хендл на пид
bool Memory::Attach(DWORD pid)
{
    Detach();

    m_handle = OpenProcess(
        PROCESS_VM_READ | PROCESS_VM_WRITE |
        PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION | SYNCHRONIZE |
        PROCESS_CREATE_THREAD,
        FALSE, pid);

    if (!m_handle)
        return false;

    m_pid = pid;
    return true;
}

void Memory::Detach()
{
    if (m_handle)
    {
        CloseHandle(m_handle);
        m_handle = nullptr;
    }
    m_pid = 0;
    m_cachedBasePid = 0;
    m_cachedMainBase = 0;
}

bool Memory::IsAlive() const
{
    if (!m_handle)
        return false;
    // WAIT_TIMEOUT = ещё крутится
    return WaitForSingleObject(m_handle, 0) == WAIT_TIMEOUT;
}

bool Memory::GetExitCode(DWORD* out_code) const
{
    if (!out_code || !m_handle)
        return false;
    return GetExitCodeProcess(m_handle, out_code) != 0;
}

bool Memory::IsValid(uintptr_t address) const
{
    // нулевой указатель сразу мимо, остальное rpm разберётся
    if (!address)
        return false;
    return true;
}

bool Memory::IsWritable(uintptr_t address, SIZE_T size) const
{
    if (!address || !m_handle)
        return false;

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQueryEx(m_handle, reinterpret_cast<LPCVOID>(address),
                       &mbi, sizeof(mbi)) != sizeof(mbi))
        return false;

    if (mbi.State != MEM_COMMIT)
        return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
        return false;

    DWORD writable =
        PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & writable))
        return false;

    uintptr_t region = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    return address + size <= region + mbi.RegionSize;
}

std::string Memory::ReadString(std::uint64_t address) const
{
    if (!IsValid(address))
        return "Unknown";

    // roblox string: длина на +0x10, SSO если < 16
    std::int32_t len = Read<std::int32_t>(address + 0x10);
    if (len <= 0 || len > 511)
        return "Unknown";

    std::uint64_t str_addr = address;
    if (len >= 16)
        str_addr = Read<std::uint64_t>(address);
    if (!IsValid(str_addr))
        return "Unknown";

    std::vector<char> buf(len + 1, 0);
    ReadRaw(str_addr, buf.data(), buf.size());
    return std::string(buf.data(), len);
}

SIZE_T Memory::ReadRaw(uintptr_t address, void* buf, SIZE_T bytes) const
{
    SIZE_T got = 0;
    ReadProcessMemory(m_handle,
        reinterpret_cast<LPCVOID>(address),
        buf, bytes, &got);
    return got;
}

SIZE_T Memory::WriteRaw(uintptr_t address, const void* buf, SIZE_T bytes) const
{
    SIZE_T wrote = 0;
    WriteProcessMemory(m_handle,
        reinterpret_cast<LPVOID>(address),
        buf, bytes, &wrote);
    return wrote;
}

uintptr_t Memory::Alloc(SIZE_T size, DWORD protect) const
{
    if (!m_handle || size == 0)
        return 0;
    LPVOID p = VirtualAllocEx(m_handle, nullptr, size,
        MEM_COMMIT | MEM_RESERVE, protect);
    return reinterpret_cast<uintptr_t>(p);
}

void Memory::Free(uintptr_t address) const
{
    if (!m_handle || !address)
        return;
    VirtualFreeEx(m_handle, reinterpret_cast<LPVOID>(address), 0, MEM_RELEASE);
}

bool Memory::Protect(uintptr_t address, SIZE_T size, DWORD new_protect,
    DWORD* old_protect) const
{
    if (!m_handle || !address || size == 0)
        return false;

    DWORD prev = 0;
    bool ok = VirtualProtectEx(m_handle,
        reinterpret_cast<LPVOID>(address), size, new_protect, &prev) != 0;
    if (old_protect)
        *old_protect = prev;
    return ok;
}

uintptr_t Memory::ResolvePointer(uintptr_t base,
    const std::initializer_list<uintptr_t>& offsets) const
{
    uintptr_t addr = Read<uintptr_t>(base);

    for (auto it = offsets.begin(); it != offsets.end(); ++it)
    {
        // последний оффсет, просто прибавляем
        if (std::next(it) == offsets.end())
            return addr + *it;

        addr = Read<uintptr_t>(addr + *it);
    }

    return addr;
}

// база RobloxPlayerBeta, отсюда все оффсеты
uintptr_t Memory::GetModuleBase(const wchar_t* moduleName) const
{
    if (!m_pid)
        return 0;

    bool want_main = !moduleName ||
        _wcsicmp(moduleName, L"RobloxPlayerBeta.exe") == 0;
    if (want_main && m_cachedMainBase && m_cachedBasePid == m_pid)
        return m_cachedMainBase;

    HANDLE snap = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_pid);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32W me{ sizeof(MODULEENTRY32W) };
    uintptr_t base = 0;

    if (Module32FirstW(snap, &me))
    {
        do
        {
            if (!moduleName || _wcsicmp(me.szModule, moduleName) == 0)
            {
                base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &me));
    }

    CloseHandle(snap);

    if (want_main && base)
    {
        m_cachedMainBase = base;
        m_cachedBasePid = m_pid;
    }
    return base;
}

DWORD Memory::FindPID(const wchar_t* processName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe{ sizeof(PROCESSENTRY32W) };
    DWORD pid = 0;

    if (Process32FirstW(snap, &pe))
    {
        do
        {
            if (_wcsicmp(pe.szExeFile, processName) == 0)
            {
                pid = pe.th32ProcessID;
                break;
            }
        }
        while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return pid;
}
