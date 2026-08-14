#include "pch.h"
#include "ShaderChams.h"
#include <algorithm>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace ShaderChams {

void Triangulate(const std::vector<ImVec2>& poly,
                 std::vector<std::vector<ImVec2>>& out_tris)
{
	out_tris.clear();
	int n0 = (int)poly.size();
	if (n0 < 3)
		return;

	if (n0 == 3)
	{
		out_tris.push_back(poly);
		return;
	}

	auto area2 = [](const ImVec2& a, const ImVec2& b, const ImVec2& c)
	{
		return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
	};
	auto point_in_tri = [&](const ImVec2& p, const ImVec2& a,
	                        const ImVec2& b, const ImVec2& c)
	{
		float a0 = area2(a, b, p);
		float a1 = area2(b, c, p);
		float a2 = area2(c, a, p);
		bool has_neg = (a0 < 0) || (a1 < 0) || (a2 < 0);
		bool has_pos = (a0 > 0) || (a1 > 0) || (a2 > 0);
		return !(has_neg && has_pos);
	};

	std::vector<int> idx((size_t)n0);
	for (int i = 0; i < n0; ++i)
		idx[i] = i;

	float signed_area = 0.0f;
	for (int i = 0; i < n0; ++i)
	{
		const ImVec2& a = poly[i];
		const ImVec2& b = poly[(i + 1) % n0];
		signed_area += a.x * b.y - b.x * a.y;
	}
	if (signed_area < 0.0f)
		std::reverse(idx.begin(), idx.end());

	out_tris.reserve((size_t)(n0 - 2));
	int guard = n0 * n0;
	while ((int)idx.size() > 3 && guard-- > 0)
	{
		int m = (int)idx.size();
		bool clipped = false;
		for (int i = 0; i < m; ++i)
		{
			int i0 = idx[(i + m - 1) % m];
			int i1 = idx[i];
			int i2 = idx[(i + 1) % m];
			const ImVec2& a = poly[i0];
			const ImVec2& b = poly[i1];
			const ImVec2& c = poly[i2];
			if (area2(a, b, c) <= 0.0f)
				continue;

			bool ear = true;
			for (int j = 0; j < m; ++j)
			{
				int ij = idx[j];
				if (ij == i0 || ij == i1 || ij == i2)
					continue;
				if (point_in_tri(poly[ij], a, b, c))
				{
					ear = false;
					break;
				}
			}
			if (!ear)
				continue;

			out_tris.push_back({ a, b, c });
			idx.erase(idx.begin() + i);
			clipped = true;
			break;
		}
		if (!clipped)
			break;
	}
	if (idx.size() == 3)
		out_tris.push_back({ poly[idx[0]], poly[idx[1]], poly[idx[2]] });
}

}
}
}
