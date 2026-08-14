#pragma once

// поиск по дереву — после ExplorerTree.h (g_root_addr)

struct SearchResult {
    std::uint64_t address = 0;
    std::string name;
    std::string cls;
    std::string path;
};

std::mutex g_search_mtx;
std::vector<SearchResult> g_search_results;
std::atomic<std::uint32_t> g_search_gen{ 0 };
std::atomic<bool> g_searching{ false };
bool g_search_on = false;

std::string ToLower(std::string s)
{
	for (char& ch : s)
		ch = (char)std::tolower((unsigned char)ch);
	return s;
}

void RunSearch(std::string query, std::uint64_t root_addr, std::uint32_t gen)
{
	std::string q = ToLower(query);
	std::vector<SearchResult> local;

	struct Frame
	{
		std::uint64_t addr;
		std::string path;
	};
	std::vector<Frame> stack;
	stack.push_back({ root_addr, "game" });

	// 400k нод / 1k результатов а то вечность
	int budget = 400000;
	while (!stack.empty())
	{
		if (g_search_gen.load() != gen)
			return;

		if (budget-- <= 0)
			break;

		Frame f = std::move(stack.back());
		stack.pop_back();

		Cheat::Instance inst(f.addr);
		for (const auto& child : inst.GetChildren())
		{
			if (g_search_gen.load() != gen)
				return;

			std::string name = child.GetName();
			std::string cls = child.GetClassName();

			if (ToLower(name).find(q) != std::string::npos ||
				ToLower(cls).find(q) != std::string::npos)
			{
				if ((int)local.size() < 1000)
				{
					SearchResult r;
					r.address = child.address;
					r.name = name;
					r.cls = cls;
					r.path = f.path + "." + name;
					local.push_back(std::move(r));
				}
			}

			if ((int)stack.size() < 400000)
				stack.push_back({ child.address, f.path + "." + name });
		}
	}

	if (g_search_gen.load() != gen)
		return;

	{
		std::lock_guard<std::mutex> lk(g_search_mtx);
		g_search_results = std::move(local);
	}
	g_searching.store(false);
}

void StartSearch(const char* query)
{
	std::uint32_t gen = ++g_search_gen;
	g_searching.store(true);
	g_search_on = true;
	{
		std::lock_guard<std::mutex> lk(g_search_mtx);
		g_search_results.clear();
	}

	if (!g_root_addr)
	{
		g_searching.store(false);
		return;
	}

	std::thread(RunSearch, std::string(query), g_root_addr, gen).detach();
}

void StopSearch()
{
	++g_search_gen;
	g_searching.store(false);
	g_search_on = false;
	std::lock_guard<std::mutex> lk(g_search_mtx);
	g_search_results.clear();
}
