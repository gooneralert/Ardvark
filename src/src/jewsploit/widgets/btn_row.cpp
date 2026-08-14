#include "btn_row.h"
#include "btn.h"

#include <imgui.h>

int ng::btn_row(const char* id, const char* const labels[], int n, float h)
{
	if (!labels || n <= 0)
	{
		return -1;
	}

	ImGui::PushID(id);

	float pad_r = 12.f;
	float gap = 6.f;
	float avail = ImGui::GetContentRegionAvail().x;
	float total = avail - pad_r;
	if (total < 40.f)
	{
		total = 40.f;
	}

	float bw = (total - gap * (float)(n - 1)) / (float)n;
	if (bw < 40.f)
	{
		bw = 40.f;
	}

	if (h <= 0.f)
	{
		h = 30.f;
	}

	int hit = -1;

	for (int i = 0; i < n; i++)
	{
		if (i > 0)
		{
			ImGui::SameLine(0.f, gap);
		}

		char bid[24]{};
		// короткий id
		bid[0] = '#'; bid[1] = '#'; bid[2] = 'b';
		bid[3] = (char)('0' + (i % 10));
		bid[4] = 0;

		if (ng::btn(bid, labels[i], bw, h))
		{
			hit = i;
		}
	}

	ImGui::PopID();
	return hit;
}