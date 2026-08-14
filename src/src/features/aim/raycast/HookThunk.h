#pragma once
// asm stub builders, only from RaycastSilent.cpp anon ns

                std::vector<std::uint8_t> make_jmp_thunk(std::uintptr_t orig)
                {
                    std::vector<std::uint8_t> c;
                    c.insert(c.end(), { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });
                    append_u64(c, orig);
                    return c;
                }

                // asm stub, переписывает dir/origin под цель
                std::vector<std::uint8_t> make_hook_thunk(std::uintptr_t state, std::uintptr_t orig)
                {
                    std::vector<std::uint8_t> c;
                    c.reserve(384);
                    std::vector<std::size_t> inactive;

                    auto je_inactive = [&]
                    {
                        c.insert(c.end(), { 0x0F, 0x84 });
                        inactive.push_back(c.size());
                        c.insert(c.end(), { 0, 0, 0, 0 });
                    };
                    auto jbe_inactive = [&]
                    {
                        c.insert(c.end(), { 0x0F, 0x86 });
                        inactive.push_back(c.size());
                        c.insert(c.end(), { 0, 0, 0, 0 });
                    };

                    c.insert(c.end(), { 0x48, 0x83, 0xEC, 0x68 });
                    c.insert(c.end(), { 0x49, 0xBA });
                    append_u64(c, state);
                    c.insert(c.end(), { 0x41, 0x83, 0x3A, 0x00 });
                    je_inactive();
                    c.insert(c.end(), { 0x4D, 0x85, 0xC0 }); je_inactive();
                    c.insert(c.end(), { 0x4D, 0x85, 0xC9 }); je_inactive();

                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x42, 0x08 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x00 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x44, 0x24, 0x40 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x4A, 0x0C });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x48, 0x04 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x4C, 0x24, 0x44 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x52, 0x10 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x5C, 0x50, 0x08 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x54, 0x24, 0x48 });

                    c.insert(c.end(), { 0x0F, 0x28, 0xD8 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xDB });
                    c.insert(c.end(), { 0x0F, 0x28, 0xE1 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xDC });
                    c.insert(c.end(), { 0x0F, 0x28, 0xE2 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xDC });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x51, 0xDB });
                    c.insert(c.end(), { 0x0F, 0x57, 0xED });
                    c.insert(c.end(), { 0x0F, 0x2E, 0xDD });
                    jbe_inactive();

                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x21 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xE4 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x69, 0x04 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xED });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x69, 0x08 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xED });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE5 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x51, 0xE4 });
                    c.insert(c.end(), { 0x0F, 0x57, 0xED });
                    c.insert(c.end(), { 0x0F, 0x2E, 0xE5 });
                    jbe_inactive();

                    c.insert(c.end(), { 0x41, 0x8B, 0x42, 0x04 });
                    c.insert(c.end(), { 0xA8, 0x01 });
                    c.insert(c.end(), { 0x0F, 0x85 });
                    const std::size_t wallbang_jmp = c.size();
                    c.insert(c.end(), { 0, 0, 0, 0 });

                    c.insert(c.end(), { 0x0F, 0x28, 0xEC });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xEB });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xC5 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xCD });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x59, 0xD5 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x01 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x49, 0x04 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x11, 0x51, 0x08 });
                    c.insert(c.end(), { 0x49, 0xFF, 0x42, 0x18 });
                    c.push_back(0xE9);
                    const std::size_t to_call = c.size();
                    c.insert(c.end(), { 0, 0, 0, 0 });

                    const std::size_t wallbang_off = c.size();
                    patch_rel32(c, wallbang_jmp, wallbang_off);

                    c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xC3 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xCB });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x5E, 0xD3 });

                    c.insert(c.end(), { 0x0F, 0x28, 0xE0 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x08 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x50 });

                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x40 });

                    c.insert(c.end(), { 0x0F, 0x28, 0xE1 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x0C });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x54 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x44 });

                    c.insert(c.end(), { 0x0F, 0x28, 0xE2 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x59, 0x62, 0x14 });
                    c.insert(c.end(), { 0xF3, 0x41, 0x0F, 0x10, 0x6A, 0x10 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x5C, 0xEC });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x6C, 0x24, 0x58 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x58, 0xE4 });
                    c.insert(c.end(), { 0xF3, 0x0F, 0x11, 0x64, 0x24, 0x48 });

                    c.insert(c.end(), { 0x4C, 0x8D, 0x44, 0x24, 0x50 });
                    c.insert(c.end(), { 0x4C, 0x8D, 0x4C, 0x24, 0x40 });
                    c.insert(c.end(), { 0x49, 0xFF, 0x42, 0x18 });

                    const std::size_t call_off = c.size();
                    patch_rel32(c, to_call, call_off);
                    const std::size_t inactive_off = c.size();
                    for (auto o : inactive) patch_rel32(c, o, inactive_off);

                    c.insert(c.end(), { 0x48, 0x8B, 0x84, 0x24, 0x90, 0x00, 0x00, 0x00 });
                    c.insert(c.end(), { 0x48, 0x89, 0x44, 0x24, 0x20 });
                    c.insert(c.end(), { 0x48, 0xB8 });
                    append_u64(c, orig);
                    c.insert(c.end(), { 0xFF, 0xD0 });
                    c.insert(c.end(), { 0x48, 0x83, 0xC4, 0x68 });
                    c.push_back(0xC3);
                    return c;
                }

