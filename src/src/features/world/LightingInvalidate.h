#pragma once
#include <cstdint>
#include "core/memory/Memory.h"

namespace Cheat {
	namespace Features {

		class LightingInvalidate
		{
		public:
			static std::uint64_t render_view();
			static void invalidate();

			template <typename T>
			static bool write_if_changed(std::uint64_t addr, T value)
			{
				if (!g_Memory.IsValid(addr))
					return false;
				T cur = g_Memory.Read<T>(addr);
				if (cur == value)
					return false;
				g_Memory.Write<T>(addr, value);
				return true;
			}

			template <typename T>
			static void force_write(std::uint64_t addr, T value)
			{
				if (g_Memory.IsValid(addr))
					g_Memory.Write<T>(addr, value);
			}
		};

	}
}
