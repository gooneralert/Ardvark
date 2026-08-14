#include "pch.h"
#include "WorldOwn.h"

#include <atomic>

namespace Cheat {
namespace Features {
namespace WorldOwn {
namespace {

std::atomic<std::uint64_t> g_seq{ 1 };
std::atomic<std::uint8_t> g_owner[(int)Field::Count]{};
std::atomic<std::uint64_t> g_gen[(int)Field::Count]{};

}

std::uint64_t Claim(Feat f, const Field* fields, int n)
{
	const std::uint64_t gen = g_seq.fetch_add(1) + 1;
	if (!fields || n <= 0)
		return gen;

	for (int i = 0; i < n; ++i)
	{
		const int idx = (int)fields[i];
		if (idx < 0 || idx >= (int)Field::Count)
			continue;

		g_owner[idx].store((std::uint8_t)f);
		g_gen[idx].store(gen);
	}

	return gen;
}

bool Owns(Feat f, Field field)
{
	const int idx = (int)field;
	if (idx < 0 || idx >= (int)Field::Count)
		return false;

	return g_owner[idx].load() == (std::uint8_t)f;
}

void Release(Feat f, const Field* fields, int n)
{
	if (!fields || n <= 0)
		return;

	for (int i = 0; i < n; ++i)
	{
		const int idx = (int)fields[i];
		if (idx < 0 || idx >= (int)Field::Count)
			continue;

		std::uint8_t expect = (std::uint8_t)f;
		g_owner[idx].compare_exchange_strong(expect, (std::uint8_t)Feat::None);
	}
}

} // namespace WorldOwn
} // namespace Features
} // namespace Cheat
