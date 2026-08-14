#include "animation.h"

#include <math.h>

float anim::approach(float cur, float tgt, float speed, float dt)
{
	float d = tgt - cur;
	if (d > -0.05f && d < 0.05f)
	{
		return tgt;
	}

	float t = 1.f - expf(-speed * dt);
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;
	return cur + d * t;
}

ImVec2 anim::approach(const ImVec2& cur, const ImVec2& tgt, float speed, float dt)
{
	return ImVec2(
		approach(cur.x, tgt.x, speed, dt),
		approach(cur.y, tgt.y, speed, dt)
	);
}

float anim::ease_out_cubic(float t)
{
	if (t < 0.f) t = 0.f;
	if (t > 1.f) t = 1.f;
	float u = 1.f - t;
	return 1.f - u * u * u;
}
