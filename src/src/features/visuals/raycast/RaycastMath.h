// pulled into RaycastEngine.cpp anon ns — dont compile alone

double now_sec() {
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

Vector3 multiply_transpose(const float3x3& m, const Vector3& v) {
    return Vector3(
        m._11 * v.x + m._12 * v.y + m._13 * v.z,
        m._21 * v.x + m._22 * v.y + m._23 * v.z,
        m._31 * v.x + m._32 * v.y + m._33 * v.z);
}

float component(const Vector3& v, int axis)
{
    if (axis == 0)
        return v.x;

    if (axis == 1)
        return v.y;

    return v.z;
}

float3x3 identity_rotation()
{
    float3x3 r{};
    r._11 = r._22 = r._33 = 1.0f;
    return r;
}

// луч vs obb (сначала сфера, дешёвый отсев)
bool ray_intersects_obb(const Vector3& origin, const Vector3& dir,
                        const occluder_part& part, float max_distance, float* out_hit)
{
    Vector3 to_part = part.position - origin;
    float radius_sq = part.radius_sq;
    if (radius_sq < 0.f)
        radius_sq = 0.f;
    float sphere_radius = std::sqrt(radius_sq);
    float dist_sq = to_part.LengthSquared();
    float combined = sphere_radius + max_distance;
    if (dist_sq > combined * combined)
        return false;

    Vector3 local_origin = multiply_transpose(part.rotation, origin - part.position);
    Vector3 local_dir = multiply_transpose(part.rotation, dir);
    Vector3 half = part.half_size;

    float tmin = -(std::numeric_limits<float>::max)();
    float tmax = (std::numeric_limits<float>::max)();

    for (int axis = 0; axis < 3; ++axis)
    {
        float o = component(local_origin, axis);
        float d = component(local_dir, axis);
        float mn = -component(half, axis);
        float mx = component(half, axis);

        if (std::fabs(d) < 1e-6f)
        {
            if (o < mn || o > mx)
                return false;

            continue;
        }

        float t1 = (mn - o) / d;
        float t2 = (mx - o) / d;
        if (t1 > t2)
            std::swap(t1, t2);

        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax || tmax < 0.0f || tmin > max_distance)
            return false;
    }

    float hit = tmax;
    if (tmin >= 0.0f)
        hit = tmin;

    if (hit < 0.0f || hit > max_distance || tmin > tmax)
        return false;

    if (out_hit)
        *out_hit = hit;

    return true;
}
