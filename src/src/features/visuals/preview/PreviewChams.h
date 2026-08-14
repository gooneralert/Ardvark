#pragma once

#include "features/visuals/chams/ShaderChams.h"
#include "features/visuals/esp/EspBox.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace PreviewChams {

inline std::vector<ImVec2> ConvexHull(std::vector<ImVec2> pts)
{
	if (pts.size() < 3) return pts;
	std::sort(pts.begin(), pts.end(), [](const ImVec2& a, const ImVec2& b) {
		return a.x < b.x || (a.x == b.x && a.y < b.y);
	});
	auto cross = [](const ImVec2& o, const ImVec2& a, const ImVec2& b) {
		return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
	};
	std::vector<ImVec2> hull(pts.size() * 2);
	int k = 0;
	for (size_t i = 0; i < pts.size(); ++i) {
		while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) k--;
		hull[k++] = pts[i];
	}
	for (int i = (int)pts.size() - 2, t = k + 1; i >= 0; --i) {
		while (k >= t && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) k--;
		hull[k++] = pts[i];
	}
	hull.resize(k > 0 ? k - 1 : 0);
	return hull;
}

inline bool SegInsidePoly(const ImVec2& a, const ImVec2& b,
	const std::vector<ImVec2>& poly, float& out_t0, float& out_t1)
{
	int n = (int)poly.size();
	if (n < 3)
		return false;

	ImVec2 c(0, 0);
	for (const auto& p : poly)
	{
		c.x += p.x;
		c.y += p.y;
	}
	c.x /= n;
	c.y /= n;

	float t0 = 0.0f, t1 = 1.0f;
	float eps = 0.25f;
	for (int i = 0; i < n; ++i)
	{
		const ImVec2& p = poly[i];
		const ImVec2& q = poly[(i + 1) % n];
		float ex = q.x - p.x, ey = q.y - p.y;
		auto side = [&](const ImVec2& v) { return ex * (v.y - p.y) - ey * (v.x - p.x); };
		float s = -1.0f;
		if (side(c) >= 0.0f)
			s = 1.0f;
		float f0 = s * side(a), f1 = s * side(b), df = f1 - f0;
		if (fabsf(df) < 1e-6f)
		{
			if (f0 < -eps)
				return false;
			continue;
		}

		float tc = (-eps - f0) / df;
		if (df > 0.0f)
		{
			if (tc > t0)
				t0 = tc;
		}

		else
		{
			if (tc < t1)
				t1 = tc;
		}

		if (t0 >= t1)
			return false;
	}

	if (t0 < 0.0f) t0 = 0.0f;
	if (t1 > 1.0f) t1 = 1.0f;
	out_t0 = t0;
	out_t1 = t1;
	return out_t1 > out_t0;
}

inline std::vector<ImVec2> ClipHalfPlane(const std::vector<ImVec2>& poly,
	const ImVec2& p, const ImVec2& q, float s)
{
	std::vector<ImVec2> out;
	int n = (int)poly.size();
	if (n < 3)
		return out;
	out.reserve(n + 2);
	auto f = [&](const ImVec2& v) {
		return s * ((q.x - p.x) * (v.y - p.y) - (q.y - p.y) * (v.x - p.x));
	};
	for (int i = 0; i < n; ++i)
	{
		const ImVec2& a = poly[i];
		const ImVec2& b = poly[(i + 1) % n];
		float fa = f(a), fb = f(b);
		if (fa >= 0.0f)
			out.push_back(a);
		if ((fa < 0.0f) != (fb < 0.0f))
		{
			float t = fa / (fa - fb);
			out.push_back(ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
		}
	}
	if (out.size() < 3)
		out.clear();
	return out;
}

inline void SubtractPoly(std::vector<ImVec2> piece, const std::vector<ImVec2>& B,
	std::vector<std::vector<ImVec2>>& out)
{
	int n = (int)B.size();
	if (n < 3)
	{
		if (piece.size() >= 3)
			out.push_back(std::move(piece));
		return;
	}

	ImVec2 c(0, 0);
	for (const auto& v : B)
	{
		c.x += v.x;
		c.y += v.y;
	}
	c.x /= n;
	c.y /= n;

	for (int i = 0; i < n && piece.size() >= 3; ++i)
	{
		const ImVec2& p = B[i];
		const ImVec2& q = B[(i + 1) % n];
		float cs = (q.x - p.x) * (c.y - p.y) - (q.y - p.y) * (c.x - p.x);
		float s = -1.0f;
		if (cs >= 0.0f)
			s = 1.0f;
		auto outside = ClipHalfPlane(piece, p, q, -s);
		if (!outside.empty())
			out.push_back(std::move(outside));
		piece = ClipHalfPlane(piece, p, q, s);
	}
}

inline void DrawSegmentOutsideUnion(ImDrawList* dl, const ImVec2& a, const ImVec2& b,
	const std::vector<std::vector<ImVec2>>& polys,
	int skip, ImU32 color)
{
	std::vector<std::pair<float, float>> covered;
	for (int i = 0; i < (int)polys.size(); ++i) {
		if (i == skip) continue;
		float t0, t1;
		if (SegInsidePoly(a, b, polys[i], t0, t1))
			covered.emplace_back(t0, t1);
	}
	auto lerp_pt = [&](float t) {
		return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
	};
	if (covered.empty()) {
		dl->AddLine(a, b, color, 1.0f);
		return;
	}
	std::sort(covered.begin(), covered.end());
	std::vector<std::pair<float, float>> merged;
	for (auto& seg : covered) {
		if (merged.empty() || seg.first > merged.back().second)
			merged.push_back(seg);

		else
			merged.back().second = (std::max)(merged.back().second, seg.second);
	}
	float cur = 0.0f;
	for (auto& m : merged) {
		if (m.first > cur)
			dl->AddLine(lerp_pt(cur), lerp_pt(m.first), color, 1.0f);
		cur = (std::max)(cur, m.second);
	}
	if (cur < 1.0f)
		dl->AddLine(lerp_pt(cur), b, color, 1.0f);
}

// превью chams на projected part boxes
inline void Draw(ImDrawList* dl,
	const std::vector<std::array<ImVec2, 8>>& parts,
	int mode, int shader,
	ImU32 outline_col, ImU32 fill_col)
{
	if (parts.empty())
		return;

	if (mode == 0)
	{
		for (const auto& pc : parts)
			for (const auto& e : EspBox::k_box_edges)
				dl->AddLine(pc[e[0]], pc[e[1]], outline_col, 1.0f);
		return;
	}

	if (mode == 1)
	{
		for (const auto& pc : parts)
		{
			std::vector<ImVec2> pts(pc.begin(), pc.end());
			auto hull = ConvexHull(std::move(pts));
			if (hull.size() >= 3)
				dl->AddConvexPolyFilled(hull.data(), (int)hull.size(), fill_col);
		}
		for (const auto& pc : parts)
			for (const auto& e : EspBox::k_box_edges)
				dl->AddLine(pc[e[0]], pc[e[1]], outline_col, 1.0f);
		return;
	}

	std::vector<std::vector<ImVec2>> hulls;
	hulls.reserve(parts.size());
	for (const auto& pc : parts)
	{
		std::vector<ImVec2> pts(pc.begin(), pc.end());
		auto h = ConvexHull(std::move(pts));
		if (h.size() >= 3)
			hulls.push_back(std::move(h));
	}

	std::vector<std::vector<ImVec2>> clipped;
	clipped.reserve(hulls.size() * 2);
	for (int i = 0; i < (int)hulls.size(); ++i)
	{
		std::vector<std::vector<ImVec2>> pieces{ hulls[i] };
		for (int j = 0; j < i && !pieces.empty(); ++j)
		{
			std::vector<std::vector<ImVec2>> next;
			for (auto& piece : pieces)
				SubtractPoly(std::move(piece), hulls[j], next);
			pieces = std::move(next);
		}
		for (auto& piece : pieces)
		{
			if (piece.size() >= 3)
				clipped.push_back(std::move(piece));
		}
	}

	if (mode == 3)
	{
		ShaderChams::DrawFill(dl, clipped, (float)ImGui::GetTime(),
			shader, false, false);
		ImU32 shader_outline = ShaderChams::OutlineColor(shader, false);
		for (int i = 0; i < (int)hulls.size(); ++i)
		{
			const auto& hull = hulls[i];
			int n = (int)hull.size();
			for (int e = 0; e < n; ++e)
				DrawSegmentOutsideUnion(dl, hull[e], hull[(e + 1) % n], hulls, i, shader_outline);
		}
	}

	else
	{
		ImDrawListFlags backup = dl->Flags;
		dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
		for (const auto& piece : clipped)
			dl->AddConvexPolyFilled(piece.data(), (int)piece.size(), fill_col);
		dl->Flags = backup;
		for (int i = 0; i < (int)hulls.size(); ++i)
		{
			const auto& hull = hulls[i];
			int n = (int)hull.size();
			for (int e = 0; e < n; ++e)
				DrawSegmentOutsideUnion(dl, hull[e], hull[(e + 1) % n], hulls, i, outline_col);
		}
	}
}

} // namespace PreviewChams
} // namespace Visuals
} // namespace Cheat

namespace PreviewChams = Cheat::Visuals::PreviewChams;
