
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

    vec2 cx   = -center.xz;
    vec2 vx   = vec2(sqrt(dot(cx, cx) - radius * radius), radius);
    vec2 minx = mat2(vx.x, vx.y, -vx.y, vx.x) * cx;
    vec2 maxx = mat2(vx.x, -vx.y, vx.y, vx.x) * cx;

    vec2 cy   = -center.yz;
    vec2 vy   = vec2(sqrt(dot(cy, cy) - radius * radius), radius);
    vec2 miny = mat2(vy.x, vy.y, -vy.y, vy.x) * cy;
    vec2 maxy = mat2(vy.x, -vy.y, vy.y, vy.x) * cy;

    aabb = vec4(minx.x / minx.y * p00, miny.x / miny.y * p11, maxx.x / maxx.y * p00, maxy.x / maxy.y * p11);
    aabb = aabb.xwzy * vec4(0.5f, -0.5f, 0.5f, -0.5f) + vec4(0.5f);  // clip space -> uv space

    return true;
}