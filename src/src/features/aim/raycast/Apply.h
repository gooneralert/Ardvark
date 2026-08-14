#pragma once
// write target into remote RaycastState

            void SetActive(bool on, const Vector3& world_target, bool wallbang)
            {
                if (!on)
                {
                    if (g_hook.active && g_hook.state)
                    {
                        std::uint32_t v = 0;
                        w_mem(g_hook.state, &v, sizeof(v));
                        g_hook.active = false;
                    }
                    g_wallbang = false;
                    return;
                }

                if (!g_hook.installed)
                {
                    return;
                }

                float pos[3]{ world_target.x, world_target.y, world_target.z };
                std::uint32_t flags = 0;
                if (wallbang)
                {
                    flags = 1u;
                }
                float scale = 1.15f;
                std::uint32_t one = 1;
                // reserved бит = wallbang, active в конце чтобы stub сразу видел
                w_mem(g_hook.state + offsetof(RaycastState, reserved), &flags, sizeof(flags));
                w_mem(g_hook.state + offsetof(RaycastState, target_x), pos, sizeof(pos));
                w_mem(g_hook.state + offsetof(RaycastState, scale), &scale, sizeof(scale));
                w_mem(g_hook.state + offsetof(RaycastState, active), &one, sizeof(one));
                g_hook.active = true;
                g_wallbang = wallbang;
            }
