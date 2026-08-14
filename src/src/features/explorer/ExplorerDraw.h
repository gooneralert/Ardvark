#pragma once

// icons / chrome / detail — тянется из Explorer.cpp в anonymous namespace

struct IconTex {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 16, h = 16;
};

std::unordered_map<std::string, IconTex> g_icons;
bool g_icons_ok = false;

ID3D11ShaderResourceView* DecodePng(const unsigned char* png, int len, int& out_w, int& out_h)
{
	if (!Cheat::Core::g_Device)
		return nullptr;

	int ch = 0;
	unsigned char* pixels = stbi_load_from_memory(png, len, &out_w, &out_h, &ch, 4);
	if (!pixels)
		return nullptr;

	D3D11_TEXTURE2D_DESC td{};
	td.Width = out_w;
	td.Height = out_h;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = pixels;
	sd.SysMemPitch = out_w * 4;

	ID3D11Texture2D* tex = nullptr;
	HRESULT hr = Cheat::Core::g_Device->CreateTexture2D(&td, &sd, &tex);
	stbi_image_free(pixels);
	if (FAILED(hr) || !tex)
		return nullptr;

	ID3D11ShaderResourceView* srv = nullptr;
	hr = Cheat::Core::g_Device->CreateShaderResourceView(tex, nullptr, &srv);
	tex->Release();

	if (FAILED(hr))
		return nullptr;

	return srv;
}

void RegisterIcon(const char* key, const unsigned char* png, int len)
{
	IconTex it;
	it.srv = DecodePng(png, len, it.w, it.h);
	if (it.srv)
		g_icons[key] = it;
}

const char* IconKeyForClass(const std::string& cls)
{
	if (cls == "LocalScript" || cls == "Script" || cls == "ModuleScript")
		return "localscript";

	if (cls == "Part" || cls == "TrussPart" || cls == "WedgePart" ||
		cls == "CornerWedgePart" || cls == "Seat" || cls == "VehicleSeat" ||
		cls == "SpawnLocation" || cls == "UnionOperation" ||
		cls == "NegateOperation" || cls == "IntersectOperation")
	{
		return "part";
	}

	if (cls == "MeshPart")
		return "meshpart";
	if (cls == "Terrain")
		return "terrain";

	if (cls == "Workspace")
		return "workspace";
	if (cls == "Folder" || cls == "Configuration")
		return "folder";
	if (cls == "Model" || cls == "Actor" || cls == "WorldModel")
		return "model";

	if (cls == "Camera")
		return "camera";
	if (cls == "Lighting")
		return "lightning";
	if (cls == "Players")
		return "players";
	if (cls == "Player")
		return "player";
	if (cls == "Humanoid")
		return "humanoid";
	if (cls == "Backpack")
		return "backpack";
	if (cls == "StarterGear")
		return "startergear";
	if (cls == "Stats")
		return "stats";
	if (cls == "StatsItem")
		return "statsitem";
	if (cls == "GuiService")
		return "guiservice";
	if (cls == "RunService")
		return "runservice";
	if (cls == "LogService")
		return "logservice";
	if (cls == "SoundService" || cls == "Sound")
		return "soundservice";
	if (cls == "MarketplaceService")
		return "marketplaceservice";
	if (cls == "ContentProvider")
		return "contentprovider";
	if (cls == "VideoCaptureService")
		return "videocapture";

	if (cls == "PlayerGui" || cls == "StarterGui" || cls == "ScreenGui" ||
		cls == "CoreGui")
	{
		return "playergui";
	}

	if (cls == "Frame" || cls == "TextLabel" || cls == "TextButton" ||
		cls == "ImageLabel" || cls == "ImageButton" || cls == "TextBox")
	{
		return "frame";
	}

	if (cls == "BoolValue")
		return "boolvalue";
	if (cls == "IntValue")
		return "intvalue";
	if (cls == "NumberValue" || cls == "DoubleConstrainedValue")
		return "doubletype";

	return "typeshit";
}

ID3D11ShaderResourceView* IconSrv(const std::string& cls, int& w, int& h)
{
	const char* key = IconKeyForClass(cls);
	auto it = g_icons.find(key);
	if (it == g_icons.end())
		it = g_icons.find("typeshit");

	if (it == g_icons.end())
	{
		w = 16;
		h = 16;
		return nullptr;
	}

	w = it->second.w;
	h = it->second.h;
	return it->second.srv;
}

bool IsScriptClass(const std::string& cls)
{
	return cls == "LocalScript" || cls == "Script" || cls == "ModuleScript";
}

struct Target {
    std::uint64_t address = 0;
    std::string name;
    std::string cls;
    std::string path;
    bool valid = false;
};

Target g_ctx;
Target g_detail;
bool g_detail_open = false;
bool g_open_ctx = false;

std::vector<unsigned char> g_bc_bytes;
std::uint64_t g_bc_for = 0;
std::string g_bc_status;
std::string g_decompiled;
bool g_show_decompiled = false;

void DumpBytecode(const Target& t)
{
	g_bc_bytes.clear();
	g_bc_for = t.address;
	g_bc_status.clear();

	uintptr_t bc_off = ::LocalScript::ByteCode;
	if (t.cls == "ModuleScript")
		bc_off = ::ModuleScript::ByteCode;

	std::uint64_t bc_obj = g_Memory.Read<std::uint64_t>(t.address + bc_off);
	if (!g_Memory.IsValid(bc_obj))
	{
		g_bc_status = "no bytecode object";
		return;
	}

	std::uint64_t ptr = g_Memory.Read<std::uint64_t>(bc_obj + ::ByteCode::Pointer);
	std::uint64_t size = g_Memory.Read<std::uint64_t>(bc_obj + ::ByteCode::Size);

	// 16mb потолок, иначе хз что это
	if (!g_Memory.IsValid(ptr) || size == 0 || size > (16u * 1024u * 1024u))
	{
		g_bc_status = "empty / unreadable bytecode";
		return;
	}

	g_bc_bytes.resize((size_t)size);
	SIZE_T got = g_Memory.ReadRaw(ptr, g_bc_bytes.data(), (SIZE_T)size);
	g_bc_bytes.resize(got);

	char buf[96];
	std::snprintf(buf, sizeof(buf), "%zu bytes (compressed Luau bytecode)", (size_t)got);
	g_bc_status = buf;
}

void SaveBytecodeToFile(const Target& t)
{
	if (g_bc_bytes.empty())
		return;

	std::string safe;
	for (char c : t.name)
	{
		if (std::isalnum((unsigned char)c))
			safe += c;

		else
			safe += '_';
	}

	if (safe.empty())
		safe = "script";

	std::string fname = safe + ".luauc";
	std::ofstream f(fname, std::ios::binary);
	if (f)
	{
		f.write((const char*)g_bc_bytes.data(), (std::streamsize)g_bc_bytes.size());
		g_bc_status = "saved: " + fname;
	}

	else
	{
		g_bc_status = "failed to write file";
	}
}

