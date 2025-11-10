module sdf;

// Signed Distance Function module for basic shapes & operations

public float sphere(float3 pos, float radius) {
    return length(pos) - radius;
}

public float repeatingSphere(float3 pos, float radius, float3 period) {
    float3 p = pos - period * round(pos / period);
    return length(p) - radius;
    //float3 p = pos - period * floor(pos / period + 0.5);
    //return length(p) - radius;
}

public float cube(float3 pos, float size) {
    return length(max(abs(pos) - size, 0.0));
}

public float cylinder(float3 pos, float radius, float height)
{
    float2 d = abs(float2(length(pos.xy), pos.z)) - float2(radius, height);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

/*public float cube(float3 pos, float size) {
    float3 d = abs(pos) - size;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}*/

public float union(float d1, float d2) {
    return smin(d1, d2, 0.25);
}

public float subtract(float d1, float d2) {
    return max(-d1, d2);
}

public float smoothSubtract(float d1, float d2, float k) {
    return smax(-d1, d2, k);
}

float smin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

float smax(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return max(a, b) + h * h * k * 0.25;
}