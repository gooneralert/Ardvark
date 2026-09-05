#include "pch.h"
#define NOMINMAX
#include "AvatarLoader.h"

#include <winhttp.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <thread>

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/console/Console.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#pragma comment(lib, "winhttp.lib")

// Same API/service that the standalone "avatar preview" tool in
// C:\Users\blake\Downloads\preview uses: rblxapi.vercel.app generates a
// textured OBJ of any Roblox user's avatar and hands back OBJ/MTL/texture
// URLs. We reuse exactly that flow, plus Ardvark's own memory reader for the
// local player's user id.

namespace Cheat {
namespace Features {
namespace AvatarLoader {
namespace {

const char kApiKey[] = "bW57XpB71S8SlnRPaMVVqxpB4UlW65WE";
const char kApiBase[] = "https://rblxapi.vercel.app";

std::mutex g_mu;
bool g_has_ready = false;       // model waiting to be consumed (under g_mu)
Model g_ready;
bool g_failed = false;
std::atomic<bool> g_available{ false };
std::atomic<bool> g_running{ false };

// ---------------------------------------------------------------------------
// small string / JSON helpers
// ---------------------------------------------------------------------------

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty())
        return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                        nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring out(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path, bool& https)
{
    https = true;
    const char* p = url.c_str();
    if (std::strncmp(p, "https://", 8) == 0)
    {
        p += 8;
        https = true;
    }
    else if (std::strncmp(p, "http://", 7) == 0)
    {
        p += 7;
        https = false;
    }
    else
    {
        return false;
    }

    const char* slash = std::strchr(p, '/');
    if (!slash)
    {
        host.assign(p, p + std::strlen(p));
        path = L"/";
    }
    else
    {
        host.assign(p, slash);
        path.assign(slash, slash + std::strlen(slash));
    }
    return !host.empty();
}

// position right after the colon of the first "\"<key>\":" field at/after `from`
size_t FindJsonField(const std::string& s, const char* key, size_t from = 0)
{
    std::string pat = "\"";
    pat += key;
    pat += "\"";
    size_t pos = from;
    while ((pos = s.find(pat, pos)) != std::string::npos)
    {
        size_t after = pos + pat.size();
        while (after < s.size() && (s[after] == ' ' || s[after] == '\t' ||
                                    s[after] == '\r' || s[after] == '\n'))
            ++after;
        if (after < s.size() && s[after] == ':')
        {
            ++after;
            while (after < s.size() && (s[after] == ' ' || s[after] == '\t' ||
                                        s[after] == '\r' || s[after] == '\n'))
                ++after;
            return after;
        }
        pos = after;
    }
    return std::string::npos;
}

std::string ReadJsonString(const std::string& s, size_t pos)
{
    if (pos >= s.size() || s[pos] != '"')
        return {};
    std::string out;
    ++pos;
    while (pos < s.size())
    {
        const char c = s[pos++];
        if (c == '"')
            break;
        if (c == '\\')
        {
            if (pos >= s.size())
                break;
            const char e = s[pos++];
            switch (e)
            {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case '"': out += '"'; break;
            case 'u':
            {
                unsigned v = 0;
                for (int i = 0; i < 4 && pos < s.size(); ++i)
                {
                    const char h = s[pos++];
                    v <<= 4;
                    if (h >= '0' && h <= '9')       v |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f')  v |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F')  v |= (unsigned)(h - 'A' + 10);
                }
                out += (v < 0x80) ? (char)v : '?';
                break;
            }
            default: out += e; break;
            }
        }
        else
        {
            out += c;
        }
    }
    return out;
}

std::string JsonStr(const std::string& s, const char* key, size_t from = 0)
{
    const size_t pos = FindJsonField(s, key, from);
    if (pos == std::string::npos)
        return {};
    return ReadJsonString(s, pos);
}

struct TexRef {
    std::string id;
    std::string url;
};

bool ParseTextures(const std::string& json, std::vector<TexRef>& out)
{
    out.clear();
    const size_t pos = FindJsonField(json, "textures");
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[')
        return false;

    size_t i = pos;
    while (i < json.size() && out.size() < 64)
    {
        const size_t ob = json.find('{', i);
        if (ob == std::string::npos)
            break;
        const size_t cb = json.find('}', ob);
        if (cb == std::string::npos)
            break;
        const std::string entry = json.substr(ob, cb - ob + 1);
        TexRef t;
        t.id = JsonStr(entry, "id");
        t.url = JsonStr(entry, "url");
        if (!t.id.empty() && !t.url.empty())
            out.push_back(std::move(t));
        i = cb + 1;
    }
    return !out.empty();
}

// 0 = permanent error, 1 = completed, 2 = keep polling
int ApiState(const std::string& resp)
{
    if (FindJsonField(resp, "data") == std::string::npos)
        return 0;
    const std::string state = JsonStr(resp, "state");
    if (state == "Completed")
        return 1;
    return 2;
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

bool HttpRequest(const wchar_t* verb, const std::string& url,
                 const std::vector<std::wstring>& headers,
                 const std::string& body,
                 std::vector<unsigned char>& out, std::wstring* set_cookie)
{
    out.clear();
    std::wstring host, path;
    bool https = true;
    if (!ParseUrl(url, host, path, https))
        return false;

    HINTERNET ses = WinHttpOpen(L"jewsploit/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses)
        return false;

    const INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
    if (!con)
    {
        WinHttpCloseHandle(ses);
        return false;
    }

    HINTERNET req = WinHttpOpenRequest(con, verb, path.c_str(), nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       https ? WINHTTP_FLAG_SECURE : 0);
    if (!req)
    {
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }

    // rbxcdn serves the OBJ / MTL with Content-Encoding: gzip; ask WinHTTP to
    // decode it transparently (same as WinINet did inside the standalone tool).
    DWORD decomp = WINHTTP_DECOMPRESSION_FLAG_ALL;
    WinHttpSetOption(req, WINHTTP_OPTION_DECOMPRESSION, &decomp, sizeof(decomp));

    DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

    std::wstring headerStr;
    for (const auto& h : headers)
        headerStr += h + L"\r\n";

    const BOOL ok = WinHttpSendRequest(
        req,
        headerStr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerStr.c_str(),
        headerStr.empty() ? 0 : (DWORD)-1L,
        (verb[0] == 'P' && !body.empty()) ? (LPVOID)body.data() : WINHTTP_NO_REQUEST_DATA,
        (DWORD)body.size(), (DWORD)body.size(), 0);

    if (!ok || !WinHttpReceiveResponse(req, nullptr))
    {
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }

    // rbxcdn returns HTTP error bodies (e.g. 403 Access Denied XML) that must
    // NOT be treated as a successful download - otherwise a failed texture
    // would be passed to stbi as if it were a PNG and the avatar would render
    // white instead of falling back to the material colour.
    {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                                WINHTTP_NO_HEADER_INDEX))
        {
            if (status >= 400)
            {
                WinHttpCloseHandle(req);
                WinHttpCloseHandle(con);
                WinHttpCloseHandle(ses);
                return false;
            }
        }
    }

    for (;;)
    {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0)
            break;
        const std::size_t old = out.size();
        out.resize(old + avail);
        DWORD read = 0;
        if (!WinHttpReadData(req, out.data() + old, avail, &read))
        {
            out.resize(old);
            break;
        }
        out.resize(old + read);
        if (out.size() > (48u << 20)) // paranoia cap
            break;
    }

    if (set_cookie)
    {
        DWORD idx = 0, sz = 0;
        if (WinHttpQueryHeaders(req, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX,
                                WINHTTP_NO_OUTPUT_BUFFER, &sz, &idx))
        {
            const int n = (int)(sz / sizeof(wchar_t)) + 1;
            std::wstring buf(n, L'\0');
            if (WinHttpQueryHeaders(req, WINHTTP_QUERY_SET_COOKIE, WINHTTP_HEADER_NAME_BY_INDEX,
                                    &buf[0], &sz, &idx))
            {
                buf.resize(wcslen(buf.c_str()));
                const size_t semi = buf.find(L';');
                if (semi != std::wstring::npos)
                    buf.resize(semi);
                *set_cookie = buf;
            }
        }
    }

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return !out.empty();
}

bool HttpGet(const std::string& url, const std::vector<std::wstring>& headers,
             std::string& out)
{
    std::vector<unsigned char> bytes;
    if (!HttpRequest(L"GET", url, headers, "", bytes, nullptr))
        return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool HttpGetBytes(const std::string& url, const std::vector<std::wstring>& headers,
                  std::vector<unsigned char>& out)
{
    return HttpRequest(L"GET", url, headers, "", out, nullptr);
}

// ---------------------------------------------------------------------------
// MTL text: replace "map_Kd <asset id>" with the real URL, drop "map_d"
// (alpha map lines - our shader always uses the mesh's own texture).
// ---------------------------------------------------------------------------

std::string ProcessMtlText(const std::string& mtl, const std::vector<TexRef>& textures)
{
    std::vector<std::pair<std::string, std::string>> id2url;
    id2url.reserve(textures.size());
    for (const auto& t : textures)
        id2url.push_back({ t.id, t.url });

    std::istringstream in(mtl);
    std::ostringstream out;
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("map_d ", 0) == 0 || line.rfind("map_d\t", 0) == 0)
            continue;

        if (line.rfind("map_", 0) == 0)
        {
            const size_t sp = line.find_first_of(" \t", 4);
            if (sp != std::string::npos)
            {
                const size_t beg = line.find_first_not_of(" \t", sp);
                const size_t end = line.find_last_not_of(" \t");
                if (beg != std::string::npos && end > beg)
                {
                    const std::string tok = line.substr(beg, end - beg + 1);
                    for (const auto& kv : id2url)
                    {
                        if (kv.first == tok)
                        {
                            line.replace(beg, end - beg + 1, kv.second);
                            break;
                        }
                    }
                }
            }
        }
        out << line << '\n';
    }
    return out.str();
}

struct MatInfo {
    std::string name;
    float kd[3]{ 1.f, 1.f, 1.f };
    std::string map_kd_url;
};

std::vector<MatInfo> ParseMtl(const std::string& mtl)
{
    std::vector<MatInfo> mats;
    MatInfo cur;
    bool has = false;
    std::istringstream in(mtl);
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::istringstream ls(line);
        std::string cmd;
        if (!(ls >> cmd))
            continue;
        if (cmd == "newmtl")
        {
            if (has)
                mats.push_back(std::move(cur));
            cur = MatInfo{};
            ls >> cur.name;
            has = true;
        }
        else if (cmd == "Kd")
        {
            ls >> cur.kd[0] >> cur.kd[1] >> cur.kd[2];
        }
        else if (cmd == "map_Kd")
        {
            ls >> cur.map_kd_url;
        }
    }
    if (has)
        mats.push_back(std::move(cur));
    return mats;
}

// ---------------------------------------------------------------------------
// local player
// ---------------------------------------------------------------------------

std::int64_t ResolveLocalUserId()
{
    if (!g_Memory.IsAttached())
        return 0;
    const auto players = Cheat::Globals::Players;
    if (!players || !g_Memory.IsValid(players->address))
        return 0;

    const std::uint64_t local = g_Memory.Read<std::uint64_t>(
        players->address + ::Players::LocalPlayer);
    if (!g_Memory.IsValid(local))
        return 0;

    return Cheat::Player(local).GetUserId();
}

// ---------------------------------------------------------------------------
// worker
// ---------------------------------------------------------------------------

void Worker()
{
    // 1. wait for the cache thread to resolve Globals::Players (max ~30 s)
    std::int64_t uid = 0;
    for (int i = 0; i < 60 && uid <= 0; ++i)
    {
        uid = ResolveLocalUserId();
        if (uid <= 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    if (uid <= 0)
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_failed = true;
        g_running.store(false);
        return;
    }

    const std::wstring keyHeader = L"x-api-key: " + Utf8ToWide(kApiKey);

    // 2. non-fatal login (the x-api-key header alone often suffices)
    std::wstring cookie;
    {
        std::string loginBody = "{\"password\":\"";
        loginBody += kApiKey;
        loginBody += "\"}";
        std::vector<std::wstring> headers{
            keyHeader,
            L"Content-Type: application/json"
        };
        std::vector<unsigned char> dummy;
        HttpRequest(L"POST", std::string(kApiBase) + "/api/auth/login",
                    headers, loginBody, dummy, &cookie);
    }

    std::vector<std::wstring> apiHeaders{ keyHeader };
    if (!cookie.empty())
        apiHeaders.push_back(L"Cookie: " + cookie);

    const std::string queryUrl =
        std::string(kApiBase) + "/api/avatar?userId=" + std::to_string((long long)uid);

    // 3. poll until the model render completes
    std::string resp;
    bool completed = false;
    for (int attempt = 0; attempt < 180; ++attempt)
    {
        std::string r;
        if (HttpGet(queryUrl, apiHeaders, r) && !r.empty())
        {
            const int st = ApiState(r);
            if (st == 0)
                break; // permanent error
            if (st == 1)
            {
                resp = std::move(r);
                completed = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (!completed)
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_failed = true;
        g_running.store(false);
        return;
    }

    std::vector<TexRef> textures;
    ParseTextures(resp, textures);
    const std::string objUrl = JsonStr(resp, "objUrl");
    const std::string mtlUrl = JsonStr(resp, "mtlUrl");
    if (objUrl.empty() || mtlUrl.empty())
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_failed = true;
        g_running.store(false);
        return;
    }

    // 4. download OBJ + MTL (WinHTTP transparently gunzips the rbxcdn files).
    //    The CDN is occasionally flaky (intermittent 403), so retry a few times.
    std::string objText;
    std::string mtlText;
    for (int attempt = 0; attempt < 3 && (objText.empty() || mtlText.empty()); ++attempt)
    {
        if (objText.empty())
            HttpGet(objUrl, {}, objText);
        if (mtlText.empty())
            HttpGet(mtlUrl, {}, mtlText);
        if (objText.empty() || mtlText.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(500 * (attempt + 1)));
    }
    if (objText.empty() || mtlText.empty())
    {
        Cheat::Console::Log(Cheat::Console::Color::Red,
            "avatar: failed to download OBJ/MTL");
        std::lock_guard<std::mutex> lk(g_mu);
        g_failed = true;
        g_running.store(false);
        return;
    }

    // 5. MTL: swap asset ids for real URLs, then parse materials
    const std::string processedMtl = ProcessMtlText(mtlText, textures);
    std::vector<MatInfo> mats = ParseMtl(processedMtl);

    // 6. download each referenced texture once (with retries)
    std::vector<std::pair<std::string, std::vector<unsigned char>>> texData;
    for (const auto& m : mats)
    {
        if (m.map_kd_url.empty())
            continue;
        bool already = false;
        for (const auto& kv : texData)
        {
            if (kv.first == m.map_kd_url)
            {
                already = true;
                break;
            }
        }
        if (already)
            continue;

        std::vector<unsigned char> bytes;
        for (int attempt = 0; attempt < 3 && bytes.empty(); ++attempt)
        {
            if (HttpGetBytes(m.map_kd_url, {}, bytes))
                break;
            bytes.clear();
            std::this_thread::sleep_for(std::chrono::milliseconds(400 * (attempt + 1)));
        }
        if (!bytes.empty())
            texData.emplace_back(m.map_kd_url, std::move(bytes));
    }
    Cheat::Console::Log(Cheat::Console::Color::Cyan,
        "avatar: %zu materials, %zu/%zu textures downloaded",
        mats.size(), texData.size(), mats.size());

    // 7. hand the assembled model to the render thread
    Model model;
    model.ok = true;
    model.obj = std::move(objText);
    model.materials.reserve(mats.size());
    for (auto& m : mats)
    {
        Material mm;
        mm.name = std::move(m.name);
        mm.kd[0] = m.kd[0];
        mm.kd[1] = m.kd[1];
        mm.kd[2] = m.kd[2];
        // materials may share one texture URL (body parts -> single atlas),
        // so copy instead of move - moving would empty it for the rest
        for (const auto& kv : texData)
        {
            if (kv.first == m.map_kd_url)
            {
                mm.png = kv.second;
                break;
            }
        }
        model.materials.push_back(std::move(mm));
    }

    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_ready = std::move(model);
        g_has_ready = true;
        g_failed = false;
    }
    g_available.store(true);
    g_running.store(false);
}

} // namespace

void Start()
{
    if (g_running.exchange(true))
        return;
    g_available.store(false);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_has_ready = false;
        g_failed = false;
        g_ready = Model{};
    }
    std::thread(Worker).detach();
}

bool ConsumeReady(Model& out)
{
    if (!g_available.load(std::memory_order_acquire))
        return false;
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_has_ready)
        return false;
    out = std::move(g_ready);
    g_has_ready = false;
    g_available.store(false, std::memory_order_release);
    return true;
}

bool IsFailed()
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_failed;
}

void Reset()
{
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_has_ready = false;
        g_failed = false;
        g_ready = Model{};
    }
    g_available.store(false);
    g_running.store(false);
}

} } }