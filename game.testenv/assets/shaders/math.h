
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

// A Survey of Efficient Representations for Independent Unit Vectors
vec2 EncodeOct(vec3 v)
{
    vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    vec2 s = vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
    vec2 r = (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * s) : p;
    return r;
}

vec3 DecodeOct(vec2 e)
{
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    vec2 s = vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
    v.xy   = v.z < 0 ? (1.0 - abs(v.yx)) * s : v.xy;
    return normalize(v);
}

vec3 ToSRGB(vec3 c) { return pow(c.xyz, vec3(1.0 / 2.2)); }

vec4 ToSRGB(vec4 c) { return vec4(pow(c.xyz, vec3(1.0 / 2.2)), c.w); }

vec3 FromSRGB(vec3 c) { return pow(c.xyz, vec3(2.2)); }

vec4 FromSRGB(vec4 c) { return vec4(pow(c.xyz, vec3(2.2)), c.w); }

// Gradient noise from Jorge Jimenez's presentation:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
float GradientNoise(vec2 uv) { return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715)))); }