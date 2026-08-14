#pragma once
#include "features/lua/vm/Reflect.h"
// install / remove / ensure + status

            bool Ready() { return g_hook.installed; }
            bool Aiming() { return g_hook.active; }
            bool WallbangMode() { return g_wallbang; }
            std::uintptr_t OriginalHandler() { return g_hook.originalFunction; }

            bool Install()
            {
                if (g_hook.installed)
                {
                    return true;
                }

                std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
                if (!base)
                {
                    return false;
                }

                // не долбим install каждые 2 мс если упало
                auto now = std::chrono::steady_clock::now();
                if (g_lastFail.time_since_epoch().count() != 0 &&
                    now - g_lastFail < std::chrono::milliseconds(1500))
                {
                    return false;
                }

                std::uintptr_t slot = Reflect::RaycastSlot();
                std::uintptr_t fn   = g_Memory.Read<std::uintptr_t>(slot);

                if (!addr_ok(fn))
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "bad handler");
                    return false;
                }

                if (!g_hook.state)
                {
                    g_hook.state = g_Memory.Alloc(page_sz(), PAGE_READWRITE);
                }

                if (!g_hook.state)
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "state alloc");
                    return false;
                }

                // jmp-only оставлял раньше, щас полный stub
                //auto thunk = make_jmp_thunk(fn);
                auto thunk = make_hook_thunk(g_hook.state, fn);

                if (thunk.size() > 0x200)
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "stub too large");
                    return false;
                }

                bool owned = false;
                std::uintptr_t stub = 0;
                std::uintptr_t ignore_cave = 0;

                // несколько попыток, иногда cave не пишется
                for (int attempt = 0; attempt < 8 && !stub; ++attempt)
                {
                    std::uintptr_t cand = find_exec_cave(0x200, base, ignore_cave);
                    if (!cand)
                    {
                        break;
                    }

                    SetLastError(0);
                    DWORD old_prot = 0;
                    bool prot_ok =
                        protect_remote(cand, thunk.size(), PAGE_EXECUTE_READWRITE, &old_prot);

                    SetLastError(0);
                    if (!write_protected(cand, thunk.data(), thunk.size()))
                    {
                        if (prot_ok)
                        {
                            protect_remote(cand, thunk.size(), old_prot, nullptr);
                        }
                        ignore_cave = cand;
                        continue;
                    }

                    stub = cand;
                    owned = false;
                }

                if (!stub)
                {
                    stub = alloc_exec_page();
                    owned = stub != 0;
                    if (stub)
                    {
                        SetLastError(0);
                        if (!write_protected(stub, thunk.data(), thunk.size()))
                        {
                            g_Memory.Free(stub);
                            stub = 0;
                            owned = false;
                        }
                    }
                }

                if (!stub)
                {
                    g_lastFail = now;
                    report_inject(false, 0, 0, "no host");
                    return false;
                }

                RaycastState empty{};
                SetLastError(0);
                if (!w_mem(g_hook.state, &empty, sizeof(empty)))
                {
                    g_lastFail = now;
                    if (owned)
                    {
                        g_Memory.Free(stub);
                    }
                    report_inject(false, 0, 0, "state write");
                    return false;
                }

                FlushInstructionCache(g_Memory.GetHandle(), (void*)stub, thunk.size());
                mark_cfg(stub);

                DWORD prot = query_protect(stub);
                if (!is_executable_protect(prot))
                {
                    g_lastFail = now;
                    if (owned)
                    {
                        g_Memory.Free(stub);
                    }
                    report_inject(false, 0, 0, "stub not executable");
                    return false;
                }

                protect_remote(slot, 8, PAGE_READWRITE, nullptr);
                if (!write_protected(slot, &stub, sizeof(stub)) ||
                    g_Memory.Read<std::uintptr_t>(slot) != stub)
                {
                    g_lastFail = now;
                    if (owned)
                    {
                        g_Memory.Free(stub);
                    }
                    report_inject(false, 0, 0, "slot write");
                    return false;
                }

                g_hook.module_base      = base;
                g_hook.originalFunction = fn;
                g_hook.thunk            = stub;
                g_hook.thunk_owned      = owned;
                g_hook.installed        = true;
                g_hook.active           = false;
                report_inject(true, fn, stub);
                return true;
            }

            void Remove()
            {
                if (g_hook.installed && addr_ok(g_hook.originalFunction) && g_hook.module_base)
                {
                    std::uintptr_t slot =
                        Reflect::RaycastSlot();
                    write_protected(slot, &g_hook.originalFunction,
                                    sizeof(g_hook.originalFunction));
                }

                if (g_hook.thunk && !g_hook.thunk_owned)
                {
                    std::vector<std::uint8_t> pad(0x200, 0xCC);
                    write_protected(g_hook.thunk, pad.data(), pad.size());
                }

                if (g_hook.thunk && g_hook.thunk_owned)
                {
                    g_Memory.Free(g_hook.thunk);
                }

                if (g_hook.state)
                {
                    g_Memory.Free(g_hook.state);
                }

                g_hook = {};
                g_wallbang = false;
            }

            void Ensure(bool want)
            {
                std::uintptr_t base = g_Memory.GetModuleBase(L"RobloxPlayerBeta.exe");
                static std::uintptr_t s_last_base = 0;

                if (g_hook.installed && base && s_last_base && base != s_last_base)
                {
                    Remove();
                    Console::Clear();
                    Console::DumpWorld();
                    Console::Log(Console::Color::Orange, "Silent rescan  module");
                }

                if (base)
                {
                    s_last_base = base;
                }

                if (want)
                {
                    if (!base)
                    {
                        return;
                    }

                    if (g_hook.installed && g_hook.thunk)
                    {
                        std::uintptr_t slot = Reflect::RaycastSlot();
                        std::uintptr_t cur = g_Memory.Read<std::uintptr_t>(slot);
                        if (cur != g_hook.thunk)
                        {
                            g_hook.installed = false;
                        }
                    }

                    if (!g_hook.installed)
                    {
                        Install();
                    }
                }

                else if (g_hook.installed)
                {
                    Remove();
                    Console::Clear();
                    Console::DumpWorld();
                    Console::Log(Console::Color::Yellow, "Silent removed");
                }
            }
