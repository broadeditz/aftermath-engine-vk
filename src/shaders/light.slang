module light;

public float2x2 rot2d(float angle) {
	float s = sin(angle);
	float c = cos(angle);
    return float2x2(c, -s, s, c);
}
