#include "pch.h"
#include "PhantomForces.h"

#include "app/Settings.h"
#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"
#include "core/roblox/math/Math.h"
#undef GetClassName

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Games {
namespace PhantomForces {

namespace {

/*
 * PF live (place 292439477) — что нашли через MCP:
 *
 * - Имена сервисов DataModel зашифрованы (Players/Teams и т.д. ищем по ClassName).
 * - Workspace.Players (Folder, имя "Players" стабильно) содержит РОВНО 2 team-folder.
 *   Имена папок ("Bright blue"/"Bright orange") теперь рандом — нельзя матчить по Name.
 * - Внутри team-folder: Model на каждого живого.
 * - Логика как в roblox-ext/cache.cpp (pf_mode):
 *     Head  = Part с BillboardGui (+ TextLabel = имя)
 *     Torso = Part с SpotLight
 *     Limbs = остальные Part; L/R по local_x/local_y относительно Torso
 *   Size у парт часто ~0 → ESP подставляет R6 sizes.
 * - Teamcheck (точный):
 *     BillboardGui/TextLabel.TextColor3 ≈ (255,10,20) = враг (как fragment/PF nametag).
 *     Своя папка = где теги НЕ вражеские; + сверка имён с лидербордом.
 *     Ближайшая к камере — НЕ используем (переворачивает тимчек у стены).
 * - PlaceId 292439477 — автоматически.
 */

struct PartInfo {
    std::uint64_t addr = 0;
    Vector3 pos{};
    Vector3 size{};
    float volume = 0.f;
};

struct TeamFolder {
    std::uint64_t addr = 0;
    TeamSide side = SideUnknown;
    Color3 sample_color{};
    bool has_color = false;
};

bool ModelUnderWorkspacePlayers(std::uint64_t model, std::uint64_t ws_players);
std::uint64_t FindWorkspacePlayersFolder();

std::uint64_t FindServiceByClass(const char* cls)
{
    if (!Globals::InstanceDataModel.address)
        return 0;
    for (const auto& c : Globals::InstanceDataModel.GetChildren()) {
        if (c.GetClassName() == cls)
            return c.address;
    }
    return 0;
}

std::uint64_t LocalPlayerAddr()
{
    auto players = Globals::Players;
    if (!players || !g_Memory.IsValid(players->address)) {
        const std::uint64_t svc = FindServiceByClass("Players");
        if (!g_Memory.IsValid(svc))
            return 0;
        return g_Memory.Read<std::uint64_t>(svc + ::Player::LocalPlayer);
    }
    return g_Memory.Read<std::uint64_t>(
        players->address + ::Player::LocalPlayer);
}

std::string ReadGuiText(std::uint64_t label)
{
    if (!g_Memory.IsValid(label))
        return {};
    return g_Memory.ReadString(label + ::TextLabel::Text);
}

Color3 ReadPartColor(std::uint64_t part)
{
    if (!g_Memory.IsValid(part))
        return {};
    return g_Memory.Read<Color3>(part + ::BasePart::Color3);
}

float ColorDist2(const Color3& a, const Color3& b)
{
    const float dr = a.r - b.r;
    const float dg = a.g - b.g;
    const float db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

bool IsPartClass(const std::string& cls)
{
    return cls == "Part" || cls == "MeshPart" || cls == "UnionOperation" ||
           cls == "WedgePart" || cls == "CornerWedgePart" || cls == "TrussPart";
}

void CollectParts(const Instance& node, std::vector<PartInfo>& out, int depth)
{
    if (depth > 6 || !g_Memory.IsValid(node.address))
        return;

    const std::string cls = node.GetClassName();
    if (IsPartClass(cls)) {
        BasePart bp(node.address);
        PartInfo pi;
        pi.addr = node.address;
        pi.pos = bp.GetPosition();
        pi.size = bp.GetSize();
        pi.volume = std::fabs(pi.size.x * pi.size.y * pi.size.z);
        if (pi.volume > 0.001f &&
            (std::fabs(pi.pos.x) + std::fabs(pi.pos.y) + std::fabs(pi.pos.z)) > 0.01f)
            out.push_back(pi);
    }

    if (cls == "Model" || cls == "Folder" || cls == "Configuration" || depth == 0) {
        for (const auto& c : node.GetChildren())
            CollectParts(c, out, depth + 1);
    }
}

std::shared_ptr<Instance> MakePart(std::uint64_t addr)
{
    return std::make_shared<Instance>(addr);
}

std::uint64_t FindBillboardTextLabel(const Instance& model)
{
    for (const auto& part : model.GetChildren()) {
        if (part.GetClassName() != "Part" && part.GetClassName() != "MeshPart")
            continue;
        for (const auto& c : part.GetChildren()) {
            if (c.GetClassName() != "BillboardGui")
                continue;
            for (const auto& bc : c.GetChildren()) {
                if (bc.GetClassName() == "TextLabel")
                    return bc.address;
            }
        }
    }
    return 0;
}

std::string ReadBillboardName(const Instance& part)
{
    for (const auto& c : part.GetChildren()) {
        if (c.GetClassName() != "BillboardGui")
            continue;
        for (const auto& bc : c.GetChildren()) {
            if (bc.GetClassName() != "TextLabel")
                continue;
            std::string t = ReadGuiText(bc.address);
            if (!t.empty())
                return t;
        }
    }
    return {};
}

std::string ReadModelBillboardName(const Instance& model)
{
    const std::uint64_t label = FindBillboardTextLabel(model);
    if (!g_Memory.IsValid(label))
        return {};
    return ReadGuiText(label);
}

bool ReadModelBillboardColor(const Instance& model, Color3& out)
{
    const std::uint64_t label = FindBillboardTextLabel(model);
    if (!g_Memory.IsValid(label))
        return false;
    out = g_Memory.Read<Color3>(label + ::TextLabel::TextColor3);
    if (!std::isfinite(out.r) || !std::isfinite(out.g) || !std::isfinite(out.b))
        return false;
    if (out.r < 0.f) out.r = 0.f;
    if (out.r > 1.f) out.r = 1.f;
    if (out.g < 0.f) out.g = 0.f;
    if (out.g > 1.f) out.g = 1.f;
    if (out.b < 0.f) out.b = 0.f;
    if (out.b > 1.f) out.b = 1.f;
    return true;
}

// PF enemy nametag ≈ RGB(255, 10, 20); тиммейты — cyan
bool IsEnemyLabelColor(const Color3& c)
{
    const int r = (int)(c.r * 255.f + 0.5f);
    const int g = (int)(c.g * 255.f + 0.5f);
    const int b = (int)(c.b * 255.f + 0.5f);
    return std::abs(r - 255) <= 8 && std::abs(g - 10) <= 12 && std::abs(b - 20) <= 12;
}

// >0 = enemy folder, <0 = friendly, 0 = unknown
int ScoreFolderEnemy(std::uint64_t folder)
{
    int enemy = 0;
    int friendly = 0;
    for (const auto& model : Instance(folder).GetChildren()) {
        if (model.GetClassName() != "Model")
            continue;
        Color3 col{};
        if (!ReadModelBillboardColor(model, col))
            continue;
        if (IsEnemyLabelColor(col))
            ++enemy;
        else
            ++friendly;
    }
    if (enemy + friendly <= 0)
        return 0;
    return enemy - friendly;
}

// как roblox-ext: Head=BillboardGui, Torso=SpotLight, limbs по local x/y
bool PopulateCharacter(std::uint64_t model, PlayerCache& cache)
{
    std::vector<std::uint64_t> all_parts;
    for (const auto& child : Instance(model).GetChildren()) {
        // в roblox-ext только Part (не MeshPart)
        if (child.GetClassName() == "Part")
            all_parts.push_back(child.address);
    }
    if (all_parts.empty())
        return false;

    std::uint64_t head_part = 0;
    std::uint64_t torso_part = 0;
    std::vector<std::uint64_t> limb_parts;
    std::string player_name;

    for (const std::uint64_t part_addr : all_parts) {
        bool is_head = false;
        bool is_torso = false;
        Instance part(part_addr);

        for (const auto& pc : part.GetChildren()) {
            const std::string pccls = pc.GetClassName();
            if (pccls == "BillboardGui") {
                is_head = true;
                if (player_name.empty())
                    player_name = ReadBillboardName(part);
                break;
            }
            if (pccls == "SpotLight") {
                is_torso = true;
                break;
            }
        }

        if (is_head)
            head_part = part_addr;
        else if (is_torso)
            torso_part = part_addr;
        else
            limb_parts.push_back(part_addr);
    }

    if (!head_part && !torso_part)
        return false;

    cache.isR6 = true;
    if (head_part)
        cache.head = MakePart(head_part);
    if (torso_part) {
        cache.upperTorso = MakePart(torso_part);
        // отдельный shared_ptr — ESP is_hrp не должен скипать torso в chams
        cache.humanoidRootPart = MakePart(torso_part);
    }

    if (torso_part && !limb_parts.empty()) {
        BasePart torso_bp(torso_part);
        const Vector3 torso_pos = torso_bp.GetPosition();
        const Matrix4x4 rot = torso_bp.GetRotation();
        // как matrix3 в roblox-ext: right=col0, up=col1
        const float rx = rot.m[0][0], ry_r = rot.m[1][0], rz_r = rot.m[2][0];
        const float ux = rot.m[0][1], uy = rot.m[1][1], uz = rot.m[2][1];

        std::vector<std::pair<float, std::uint64_t>> arms, legs;
        for (const std::uint64_t lp : limb_parts) {
            const Vector3 pos = BasePart(lp).GetPosition();
            const float ox = pos.x - torso_pos.x;
            const float oy = pos.y - torso_pos.y;
            const float oz = pos.z - torso_pos.z;
            const float local_y = ox * ux + oy * uy + oz * uz;
            const float local_x = ox * rx + oy * ry_r + oz * rz_r;
            if (local_y > -0.5f)
                arms.push_back({ local_x, lp });
            else
                legs.push_back({ local_x, lp });
        }

        auto by_x = [](const std::pair<float, std::uint64_t>& a,
                       const std::pair<float, std::uint64_t>& b) {
            return a.first < b.first;
        };
        std::sort(arms.begin(), arms.end(), by_x);
        std::sort(legs.begin(), legs.end(), by_x);

        if (arms.size() >= 2) {
            cache.leftUpperArm = MakePart(arms[0].second);
            cache.rightUpperArm = MakePart(arms[1].second);
        } else if (arms.size() == 1) {
            cache.rightUpperArm = MakePart(arms[0].second);
        }

        if (legs.size() >= 2) {
            cache.leftUpperLeg = MakePart(legs[0].second);
            cache.rightUpperLeg = MakePart(legs[1].second);
        } else if (legs.size() == 1) {
            cache.rightUpperLeg = MakePart(legs[0].second);
        }
    }

    if (!player_name.empty()) {
        cache.name = player_name;
        cache.displayName = player_name;
    } else {
        const std::string model_name = Instance(model).GetName();
        if (!model_name.empty()) {
            cache.name = model_name;
            cache.displayName = model_name;
        }
    }

    return true;
}

Color3 SampleFolderColor(std::uint64_t folder)
{
    Color3 sum{};
    int n = 0;
    Instance f(folder);
    for (const auto& model : f.GetChildren()) {
        if (model.GetClassName() != "Model")
            continue;
        for (const auto& ch : model.GetChildren()) {
            if (!IsPartClass(ch.GetClassName()))
                continue;
            Color3 c = ReadPartColor(ch.address);
            const float lum = (c.r + c.g + c.b) / 3.f;
            if (lum < 0.08f || lum > 0.92f)
                continue;
            const bool blueish = c.b > c.r + 0.05f && c.b > c.g;
            const bool orangeish = c.r > c.b + 0.05f && c.r >= c.g;
            if (!blueish && !orangeish)
                continue;
            sum.r += c.r; sum.g += c.g; sum.b += c.b;
            ++n;
            if (n >= 12)
                break;
        }
        if (n >= 12)
            break;
    }
    if (n <= 0)
        return {};
    return Color3(sum.r / n, sum.g / n, sum.b / n);
}

// мин. дистанция цвета папки до ref (для Team Color при камуфляже)
float FolderColorDist2(std::uint64_t folder, const Color3& ref)
{
    float best = 1e9f;
    if (ref.r + ref.g + ref.b < 0.01f)
        return best;

    for (const auto& model : Instance(folder).GetChildren()) {
        if (model.GetClassName() != "Model")
            continue;
        for (const auto& ch : model.GetChildren()) {
            if (!IsPartClass(ch.GetClassName()))
                continue;
            Color3 c = ReadPartColor(ch.address);
            const float lum = (c.r + c.g + c.b) / 3.f;
            if (lum < 0.05f || lum > 0.95f)
                continue;
            const float d = ColorDist2(c, ref);
            if (d < best)
                best = d;
        }
    }
    return best;
}

TeamSide SideFromColor(const Color3& c)
{
    // Phantoms ≈ bright blue, Ghosts ≈ bright orange
    if (c.b > c.r && c.b > c.g)
        return SidePhantom;
    if (c.r > c.b && c.r >= c.g * 0.8f)
        return SideGhost;
    return SideUnknown;
}

Color3 FindLocalTeamColorPart()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return {};

    auto cam = g_Memory.Read<std::uint64_t>(
        Globals::Workspace->address + ::Workspace::CurrentCamera);
    if (!g_Memory.IsValid(cam))
        return {};

    for (const auto& m : Instance(cam).GetChildren()) {
        if (m.GetClassName() != "Model")
            continue;
        for (const auto& ch : m.GetChildren()) {
            if (ch.GetName() == "Team Color")
                return ReadPartColor(ch.address);
        }
    }
    return {};
}

std::uint64_t FindWorkspacePlayersFolder()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return 0;
    for (const auto& c : Globals::Workspace->GetChildren()) {
        if (c.GetClassName() == "Folder" && c.GetName() == "Players")
            return c.address;
    }
    return 0;
}

std::uint64_t FindPlayerGui(std::uint64_t lp)
{
    if (!g_Memory.IsValid(lp))
        return 0;
    for (const auto& c : Instance(lp).GetChildren()) {
        if (c.GetClassName() == "PlayerGui")
            return c.address;
    }
    return 0;
}

std::uint64_t FindChildByName(std::uint64_t parent, const char* name)
{
    if (!g_Memory.IsValid(parent))
        return 0;
    for (const auto& c : Instance(parent).GetChildren()) {
        if (c.GetName() == name)
            return c.address;
    }
    return 0;
}

void CollectBoardNames(std::uint64_t board, std::vector<std::string>& out)
{
    auto container = FindChildByName(board, "Container");
    if (!g_Memory.IsValid(container))
        return;

    for (const auto& row : Instance(container).GetChildren()) {
        if (row.GetName() != "DisplayPlayerScore")
            continue;
        auto label = FindChildByName(row.address, "TextPlayer");
        std::string name = ReadGuiText(label);
        if (name.empty() || name == "Player")
            continue;
        out.push_back(std::move(name));
    }
}

struct LeaderboardSnap {
    std::vector<std::string> phantoms;
    std::vector<std::string> ghosts;
    TeamSide local_side = SideUnknown;
};

LeaderboardSnap ReadLeaderboard(const std::string& local_name)
{
    LeaderboardSnap snap;
    const std::uint64_t lp = LocalPlayerAddr();
    const std::uint64_t gui = FindPlayerGui(lp);
    if (!g_Memory.IsValid(gui))
        return snap;

    auto lb = FindChildByName(gui, "LeaderboardScreenGui");
    auto score = FindChildByName(lb, "DisplayScoreFrame");
    auto container = FindChildByName(score, "Container");
    if (!g_Memory.IsValid(container))
        return snap;

    auto phantom_board = FindChildByName(container, "DisplayPhantomBoard");
    auto ghost_board = FindChildByName(container, "DisplayGhostBoard");
    CollectBoardNames(phantom_board, snap.phantoms);
    CollectBoardNames(ghost_board, snap.ghosts);

    if (!local_name.empty()) {
        for (const auto& n : snap.phantoms) {
            if (n == local_name) {
                snap.local_side = SidePhantom;
                break;
            }
        }
        if (snap.local_side == SideUnknown) {
            for (const auto& n : snap.ghosts) {
                if (n == local_name) {
                    snap.local_side = SideGhost;
                    break;
                }
            }
        }
    }
    return snap;
}

std::unordered_map<std::uint64_t, std::string> BuildModelToPlayerName()
{
    std::unordered_map<std::uint64_t, std::string> map;
    std::uint64_t players = 0;
    if (Globals::Players && g_Memory.IsValid(Globals::Players->address))
        players = Globals::Players->address;
    else
        players = FindServiceByClass("Players");
    if (!g_Memory.IsValid(players))
        return map;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();

    for (const auto& p : Instance(players).GetChildren()) {
        if (p.GetClassName() != "Player")
            continue;
        const std::uint64_t model = g_Memory.Read<std::uint64_t>(
            p.address + ::Player::ModelInstance);
        if (!g_Memory.IsValid(model))
            continue;
        // только живые под Workspace.Players (не труп в Ignore)
        if (g_Memory.IsValid(ws_players) &&
            !ModelUnderWorkspacePlayers(model, ws_players))
            continue;
        std::string name = p.GetName();
        if (!name.empty())
            map[model] = std::move(name);
    }
    return map;
}

Vector3 CameraPos()
{
    if (!Globals::Workspace || !g_Memory.IsValid(Globals::Workspace->address))
        return {};
    auto cam = g_Memory.Read<std::uint64_t>(
        Globals::Workspace->address + ::Workspace::CurrentCamera);
    if (!g_Memory.IsValid(cam))
        return {};
    return g_Memory.Read<Vector3>(cam + ::Camera::Position);
}

// sticky side (лидерборд / Team Color) + team folder как в roblox-ext
static TeamSide g_sticky_side = SideUnknown;
static std::uint64_t g_sticky_team_folder = 0;
static std::uint64_t g_sticky_t0 = 0;
static std::uint64_t g_sticky_t1 = 0;

bool ModelUnderWorkspacePlayers(std::uint64_t model, std::uint64_t ws_players)
{
    if (!g_Memory.IsValid(model) || !g_Memory.IsValid(ws_players))
        return false;
    auto parent = Instance(model).GetParent();
    if (!parent || parent->GetClassName() != "Folder")
        return false;
    auto grand = parent->GetParent();
    return grand && grand->address == ws_players;
}

std::uint64_t ResolveTeamFolderForSide(TeamSide side, std::uint64_t ws_players)
{
    if (side == SideUnknown || !g_Memory.IsValid(ws_players))
        return 0;

    Color3 local_col = FindLocalTeamColorPart();
    std::uint64_t folders[2]{};
    int n = 0;
    std::uint64_t matched = 0;
    std::uint64_t opposite = 0;
    std::uint64_t closest_local = 0;
    float best_d = 1e9f;

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        if (n < 2)
            folders[n++] = folder.address;

        Color3 sample = SampleFolderColor(folder.address);
        const TeamSide fs = SideFromColor(sample);
        if (fs == side)
            matched = folder.address;
        else if (fs != SideUnknown)
            opposite = folder.address;

        const float d = FolderColorDist2(folder.address, local_col);
        if (d < best_d) {
            best_d = d;
            closest_local = folder.address;
        }
    }

    if (g_Memory.IsValid(matched))
        return matched;

    if (g_Memory.IsValid(opposite) && n == 2)
        return (opposite == folders[0]) ? folders[1] : folders[0];

    // ближе к FP Team Color = наша папка (даже при камуфляже)
    if (n == 2 && g_Memory.IsValid(closest_local) && best_d < 1e8f) {
        const TeamSide local_from_col = SideFromColor(local_col);
        if (local_from_col == side || local_from_col == SideUnknown)
            return closest_local;
        return (closest_local == folders[0]) ? folders[1] : folders[0];
    }

    if (g_Memory.IsValid(closest_local) && best_d < 1e8f)
        return closest_local;

    return 0;
}

// PF shares its place-id offset with other games, but the offset read can be
// stale and report 292439477 in *any* game (e.g. Arsenal). PF is unmistakable by
// structure though: every player lives under a Workspace.Players folder that
// holds team folders. Verify that so non-PF games are never treated as PF.
bool HasPfWorkspaceStructure()
{
    static std::uint64_t s_last_ms = 0;
    static bool s_result = false;

    const std::uint64_t now = GetTickCount64();
    if (s_last_ms != 0 && (now - s_last_ms) < 500)
        return s_result;
    s_last_ms = now;

    s_result = false;
    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    if (g_Memory.IsValid(ws_players))
    {
        for (const auto& child : Instance(ws_players).GetChildren())
        {
            if (!g_Memory.IsValid(child.address))
                continue;
            if (Instance(child.address).GetClassName() == "Folder")
            {
                s_result = true;
                break;
            }
        }
    }
    return s_result;
}
} // namespace

bool IsActivePlace()
{
    if (!Globals::InstanceDataModel.address)
        return false;
    if (!g_Memory.IsValid(Globals::InstanceDataModel.address))
        return false;
    // A single hardcoded place-id read is not enough: if that offset is stale it
    // can report 292439477 in *any* game (e.g. Arsenal). Also require the
    // distinctive PF structure (Workspace.Players folder-of-team-folders), so
    // only real Phantom Forces is ever treated as PF.
    const bool id_match = Globals::InstanceDataModel.GetPlaceId() == kPlaceId;
    const bool on = id_match && HasPfWorkspaceStructure();
    // PF team check overrides the default teamcheck: the universal Player.Team
    // comparison can't work here (PF teams are Workspace.Players folders), so
    // while PF is active the PF team check is always applied — same override
    // model as the silent aim method (SILENT_PHANTOM). Forcing every call, not
    // just on transition, so it can't be turned off while in PF.
    if (on)
        g_Settings.misc.teamcheck = true;
    if (!on) {
        g_sticky_side = SideUnknown;
        g_sticky_team_folder = 0;
        g_sticky_t0 = 0;
        g_sticky_t1 = 0;
    }
    return on;
}

TeamSide LocalSide()
{
    if (!IsActivePlace())
        return SideUnknown;

    const std::uint64_t lp = LocalPlayerAddr();
    std::string local_name;
    if (g_Memory.IsValid(lp))
        local_name = Instance(lp).GetName();

    auto lb = ReadLeaderboard(local_name);
    if (lb.local_side != SideUnknown) {
        g_sticky_side = lb.local_side;
        return g_sticky_side;
    }

    Color3 tc = FindLocalTeamColorPart();
    TeamSide from_col = SideFromColor(tc);
    if (from_col != SideUnknown) {
        g_sticky_side = from_col;
        return g_sticky_side;
    }

    return g_sticky_side;
}

std::uint64_t LocalCharacter()
{
    if (!IsActivePlace())
        return 0;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    const std::uint64_t lp = LocalPlayerAddr();

    // только ModelInstance под Workspace.Players (не труп в Ignore)
    if (g_Memory.IsValid(lp)) {
        const std::uint64_t mi = g_Memory.Read<std::uint64_t>(
            lp + ::Player::ModelInstance);
        if (ModelUnderWorkspacePlayers(mi, ws_players))
            return mi;
    }

    // узкий fallback только чтобы не рисовать себя — НЕ для team folder
    if (!g_Memory.IsValid(ws_players))
        return 0;

    const Vector3 cam = CameraPos();
    float best_d2 = 4.f * 4.f;
    std::uint64_t best = 0;

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        for (const auto& model : folder.GetChildren()) {
            if (model.GetClassName() != "Model")
                continue;
            std::vector<PartInfo> parts;
            CollectParts(model, parts, 0);
            if (parts.empty())
                continue;
            Vector3 c{};
            for (const auto& p : parts) {
                c.x += p.pos.x; c.y += p.pos.y; c.z += p.pos.z;
            }
            const float inv = 1.f / (float)parts.size();
            c.x *= inv; c.y *= inv; c.z *= inv;
            const float dx = c.x - cam.x;
            const float dy = c.y - cam.y;
            const float dz = c.z - cam.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = model.address;
            }
        }
    }
    return best;
}

std::uint64_t ResolveLocalFolderByLeaderboardNames(
    std::uint64_t ws_players, const LeaderboardSnap& lb)
{
    if (lb.local_side == SideUnknown || !g_Memory.IsValid(ws_players))
        return 0;

    auto name_on_side = [&](const std::string& name, TeamSide side) -> bool {
        const auto& list = (side == SidePhantom) ? lb.phantoms : lb.ghosts;
        for (const auto& n : list) {
            if (n == name)
                return true;
        }
        return false;
    };

    std::uint64_t best_folder = 0;
    int best_score = 0;

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        int score = 0;
        for (const auto& model : folder.GetChildren()) {
            if (model.GetClassName() != "Model")
                continue;
            const std::string name = ReadModelBillboardName(model);
            if (name.empty())
                continue;
            if (name_on_side(name, lb.local_side))
                ++score;
            else if (name_on_side(name, lb.local_side == SidePhantom ? SideGhost : SidePhantom))
                --score;
        }
        if (score > best_score) {
            best_score = score;
            best_folder = folder.address;
        }
    }
    return best_score > 0 ? best_folder : 0;
}

std::uint64_t LocalTeamFolder()
{
    if (!IsActivePlace())
        return 0;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    if (!g_Memory.IsValid(ws_players))
        return 0;

    std::uint64_t folders[2]{};
    int n = 0;
    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        if (n < 2)
            folders[n++] = folder.address;
    }

    const std::uint64_t t0 = n > 0 ? folders[0] : 0;
    const std::uint64_t t1 = n > 1 ? folders[1] : 0;
    if (t0 != g_sticky_t0 || t1 != g_sticky_t1) {
        g_sticky_t0 = t0;
        g_sticky_t1 = t1;
        g_sticky_team_folder = 0;
    }

    // 1) TextLabel.TextColor: враг ≈ (255,10,20), своя папка = НЕ вражеская
    if (n == 2) {
        const int s0 = ScoreFolderEnemy(folders[0]);
        const int s1 = ScoreFolderEnemy(folders[1]);
        std::uint64_t local_folder = 0;
        if (s0 > 0 && s1 <= 0)
            local_folder = folders[1]; // 0 = enemy → local = 1
        else if (s1 > 0 && s0 <= 0)
            local_folder = folders[0];
        else if (s0 < 0 && s1 >= 0)
            local_folder = folders[0]; // 0 = friendly
        else if (s1 < 0 && s0 >= 0)
            local_folder = folders[1];

        if (g_Memory.IsValid(local_folder)) {
            g_sticky_team_folder = local_folder;
            return local_folder;
        }
    }

    // 2) лидерборд: имена с билбордов ↔ Phantom/Ghost board
    const std::uint64_t lp = LocalPlayerAddr();
    std::string local_name;
    if (g_Memory.IsValid(lp))
        local_name = Instance(lp).GetName();
    const auto lb = ReadLeaderboard(local_name);
    {
        const std::uint64_t by_lb = ResolveLocalFolderByLeaderboardNames(ws_players, lb);
        if (g_Memory.IsValid(by_lb)) {
            g_sticky_team_folder = by_lb;
            return by_lb;
        }
    }

    // 3) ModelInstance под Workspace.Players
    if (g_Memory.IsValid(lp)) {
        const std::uint64_t mi = g_Memory.Read<std::uint64_t>(
            lp + ::Player::ModelInstance);
        if (ModelUnderWorkspacePlayers(mi, ws_players)) {
            auto parent = Instance(mi).GetParent();
            if (parent && parent->GetClassName() == "Folder") {
                g_sticky_team_folder = parent->address;
                return g_sticky_team_folder;
            }
        }
    }

    // 4) Team Color → папка
    if (lb.local_side != SideUnknown || LocalSide() != SideUnknown) {
        const TeamSide side = (lb.local_side != SideUnknown) ? lb.local_side : LocalSide();
        const std::uint64_t folder = ResolveTeamFolderForSide(side, ws_players);
        if (g_Memory.IsValid(folder)) {
            g_sticky_team_folder = folder;
            return folder;
        }
    }

    // 5) sticky (не nearest-to-cam — он переворачивал тимчек)
    if (g_Memory.IsValid(g_sticky_team_folder)) {
        auto p = Instance(g_sticky_team_folder).GetParent();
        if (p && p->address == ws_players)
            return g_sticky_team_folder;
        g_sticky_team_folder = 0;
    }

    return 0;
}

bool IsTeammate(const PlayerCache& cache, std::uint64_t local_team_folder)
{
    if (!IsActivePlace())
        return false;
    if (!local_team_folder || !cache.team_folder)
        return false;
    return cache.team_folder == local_team_folder;
}

void MergePlayers(std::unordered_map<std::uint64_t, PlayerCache>& target)
{
    if (!IsActivePlace())
        return;

    const std::uint64_t ws_players = FindWorkspacePlayersFolder();
    if (!g_Memory.IsValid(ws_players))
        return;

    const std::uint64_t lp = LocalPlayerAddr();
    std::string local_name;
    if (g_Memory.IsValid(lp))
        local_name = Instance(lp).GetName();

    const auto lb = ReadLeaderboard(local_name);
    const auto model_to_name = BuildModelToPlayerName();
    const std::uint64_t local_char = LocalCharacter();
    const std::uint64_t local_team = LocalTeamFolder();

    // имена с доски по стороне, ещё не занятые ModelInstance
    std::unordered_set<std::string> used_names;
    for (const auto& [m, n] : model_to_name)
        used_names.insert(n);

    auto take_lb_name = [&](TeamSide side) -> std::string {
        const auto& list = (side == SidePhantom) ? lb.phantoms : lb.ghosts;
        for (const auto& n : list) {
            if (n == local_name)
                continue;
            if (used_names.count(n))
                continue;
            used_names.insert(n);
            return n;
        }
        return {};
    };

    // разметить folder → side
    std::unordered_map<std::uint64_t, TeamSide> folder_side;
    Color3 local_col = FindLocalTeamColorPart();
    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;
        Color3 sample = SampleFolderColor(folder.address);
        TeamSide side = SideFromColor(sample);
        if (side == SideUnknown && local_team == folder.address)
            side = lb.local_side;
        if (side == SideUnknown && local_col.r + local_col.g + local_col.b > 0.01f) {
            // если цвет папки ближе к локальному Team Color — наша сторона
            // иначе противоположная
            // (второй папке назначим позже)
            side = SideFromColor(local_col);
            if (folder.address != local_team && side != SideUnknown)
                side = (side == SidePhantom) ? SideGhost : SidePhantom;
        }
        folder_side[folder.address] = side;
    }

    // если обе unknown — по local_team + lb
    if (g_Memory.IsValid(local_team) && lb.local_side != SideUnknown) {
        folder_side[local_team] = lb.local_side;
        for (auto& [fa, s] : folder_side) {
            if (fa != local_team)
                s = (lb.local_side == SidePhantom) ? SideGhost : SidePhantom;
        }
    }

    for (const auto& folder : Instance(ws_players).GetChildren()) {
        if (folder.GetClassName() != "Folder")
            continue;

        const TeamSide side = folder_side.count(folder.address)
            ? folder_side[folder.address] : SideUnknown;

        for (const auto& model : folder.GetChildren()) {
            if (model.GetClassName() != "Model")
                continue;
            if (model.address == local_char)
                continue;

            PlayerCache cache(model.address);
            cache.character = model.address;
            cache.team_folder = folder.address;
            cache.is_player = true;
            cache.isR6 = true;

            if (!PopulateCharacter(model.address, cache))
                continue;

            // имя: Billboard TextLabel → ModelInstance → лидерборд
            if (cache.name.empty() || cache.name == Instance(model.address).GetName()) {
                auto it = model_to_name.find(model.address);
                if (it != model_to_name.end()) {
                    cache.name = it->second;
                    cache.displayName = it->second;
                } else {
                    std::string n = take_lb_name(side);
                    if (!n.empty()) {
                        cache.name = n;
                        cache.displayName = n;
                    }
                }
            }
            if (cache.name.empty())
                continue;
            if (cache.name == local_name)
                continue;
            used_names.insert(cache.name);

            // ключ = адрес модели (у PF нет нормального Player→Character)
            target[model.address] = std::move(cache);
        }
    }
}

} // namespace PhantomForces
} // namespace Games
} // namespace Cheat
