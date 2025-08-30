bool ConeCull(vec3 center, float radius, vec3 coneAxis, float coneCutoff, vec3 cameraPosition)
{
    return dot(center - cameraPosition, coneAxis) > coneCutoff * length(center - cameraPosition) + radius;
}

vec3 RotateVecByQuat(vec3 v, vec4 q) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }