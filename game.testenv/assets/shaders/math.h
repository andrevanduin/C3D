
/* Perform (backface) cone culling. */
bool ConeCull(vec3 center, float radius, vec3 coneAxis, float coneCutoff, vec3 cameraPosition)
{
    return dot(center - cameraPosition, coneAxis) >= coneCutoff * length(center - cameraPosition) + radius;
}

/* Rotates a vec3 by a quaternion (represented by a vec4 here since GLSL does not have native quaternions). */
vec3 RotateVecByQuat(vec3 v, vec4 q) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }

/* 2D Polyhedral Bounds of a Clipped, Perspective-Projected 3D Sphere by Michael Mara and Morgan McGuire 2013. */
bool ProjectSphere(vec3 center, float radius, float zNear, float p00, float p11, out vec4 aabb)
{
    if (center.z < radius + zNear)
    {
        return false;
    }

    vec3 cr    = center * radius;
    float czr2 = center.z * center.z * radius;

    float vx   = sqrt(center.x * center.x + czr2);
    float minx = (vx * center.x - cr.z) / (vx * center.z + cr.x);
    float maxx = (vx * center.x + cr.z) / (vx * center.z - cr.x);

    float vy   = sqrt(center.y * center.y + czr2);
    float miny = (vy * center.y - cr.z) / (vy * center.z + cr.y);
    float maxy = (vy * center.y + cr.z) / (vy * center.z - cr.y);

    aabb = vec4(minx * p00, miny * p11, maxx * p00, maxy * p11);
    aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f);  // clip space -> uv space

    return true;
}