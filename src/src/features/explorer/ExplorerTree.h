#pragma once

struct Node {
    std::uint64_t address = 0;
    std::string name;
    std::string cls;
    std::vector<Node> children;
    bool loaded = false;
    bool open = false;
    bool has_kids = false;
};

Node g_root;
std::uint64_t g_root_addr = 0;

static int ServiceRank(const std::string& cls)
{
	static const char* order[] = {
		"Workspace",
		"Players",
		"Lighting",
		"MaterialService",
		"ReplicatedFirst",
		"ReplicatedStorage",
		"ServerStorage",
		"ServerScriptService",
		"StarterGui",
		"StarterPack",
		"StarterPlayer",
		"SoundService",
		"Chat",
		"TextChatService",
		"Teams",
		"TeleportService",
		"TweenService",
		"RunService",
		"UserInputService",
		"GuiService",
		"HttpService",
		"MarketplaceService",
		"InsertService",
		"Debris",
	};
	for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); ++i)
	{
		if (cls == order[i])
			return i;
	}
	return 1000;
}

static bool ServiceLess(const Node& a, const Node& b)
{
	int ra = ServiceRank(a.cls);
	int rb = ServiceRank(b.cls);
	if (ra != rb)
		return ra < rb;
	return a.name < b.name;
}

void SortChildren(Node& n)
{
	if (n.cls != "DataModel")
		return;

	auto& v = n.children;
	for (size_t i = 1; i < v.size(); ++i)
	{
		Node key = std::move(v[i]);
		size_t j = i;
		while (j > 0 && ServiceLess(key, v[j - 1]))
		{
			v[j] = std::move(v[j - 1]);
			--j;
		}
		v[j] = std::move(key);
	}
}

void LoadChildren(Node& n)
{
	if (n.loaded)
		return;

	n.loaded = true;
	n.children.clear();

	Cheat::Instance inst(n.address);
	for (const auto& c : inst.GetChildren())
	{
		Node cn;
		cn.address = c.address;
		cn.name = c.GetName();
		cn.cls = c.GetClassName();
		cn.has_kids = c.HasChildren();
		n.children.push_back(std::move(cn));
	}
	n.has_kids = !n.children.empty();

	SortChildren(n);

	if (n.cls == "DataModel")
	{
		for (auto& c : n.children)
		{
			if (c.cls == "Workspace")
			{
				c.open = true;
				LoadChildren(c);
				break;
			}
		}
	}
}

void EnsureRoot()
{
	std::uint64_t dm = Cheat::Globals::InstanceDataModel.address;
	if (dm && dm != g_root_addr)
	{
		g_root_addr = dm;
		g_root = Node{};
		g_root.address = dm;
		g_root.cls = "DataModel";
		g_root.name = "game";
		g_root.open = true;
	}
	else if (!dm)
	{
		g_root_addr = 0;
		g_root = Node{};
	}
}

std::string BuildPath(std::uint64_t address)
{
	std::vector<std::string> parts;
	std::uint64_t cur = address;
	int guard = 64;

	while (cur && guard-- > 0)
	{
		Cheat::Instance node(cur);
		std::string nm = node.GetName();
		if (nm.empty())
			nm = "?";
		parts.push_back(nm);

		if (cur == g_root_addr)
			break;

		auto parent = node.GetParent();
		if (!parent)
			break;

		cur = parent->address;
	}

	std::string out;
	for (auto it = parts.rbegin(); it != parts.rend(); ++it)
	{
		if (!out.empty())
			out += ".";
		out += *it;
	}

	return out;
}
