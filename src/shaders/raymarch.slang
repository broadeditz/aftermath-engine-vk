module raymarch;

import light;
import sdf;

public struct raymarchResult {
    public float distance;
    public float lastStepSize;
    public int steps;
    public bool hit;
}

public raymarchResult raymarch(float3 rayOrigin, float3 rayDirection, int maxSteps, float maxDistance, float epsilon, float time) {
    float totalDistance = 0.0;
    float stepSize = 0.0;
    float3 lastPosition = rayOrigin;
    float3 lastDirection = rayDirection;
    int steps = 0;
    bool hit = false;

    float frequency = 0.05;
    float amplitude = 0.3;

    for (steps = 0; steps < maxSteps; steps++) {
        // create perpendicular vectors to ray direction
        float3 right = normalize(cross(lastDirection, float3(0, 1, 0)));
        float3 up = normalize(cross(lastDirection, right));

        float3 offset = right * (sin(totalDistance * frequency) * amplitude) +
                        up * (cos(totalDistance * frequency) * amplitude);
        float3 wiggledDirection = normalize(rayDirection + offset);
        
        float3 currentPosition = lastPosition + wiggledDirection * stepSize;


        //float3 offset = float3(cos(totalDistance * 0.1) * 0.5, sin(totalDistance * 0.1) * 0.5, 0);
        //float3 wiggledDirection = normalize(rayDirection * stepSize + offset);
        //
        //float3 currentPosition = lastPosition + wiggledDirection * stepSize;

        //float3 currentPosition = rayOrigin + rayDirection * totalDistance;

        stepSize = sceneSDF(currentPosition);
        totalDistance += stepSize;

        if (stepSize < epsilon) {
            hit = true;
            break;
        }
        if (totalDistance >= maxDistance) {
            break;
        }
        lastPosition = currentPosition;
        lastDirection = wiggledDirection;
    }

    raymarchResult res;
    res.distance = totalDistance;
    res.lastStepSize = stepSize;
    res.steps = steps;
    res.hit = hit;
    return res;
}

float sceneSDF(float3 pos) {
    return repeatingSphere(pos - float3(3.5,3.5,0), 1, float3(7, 7, 7));
}

/*float sceneSDF(float3 pos) {
    return subtract(cylinder(pos, 50, 500000), union(repeatingSphere(pos, 1, float3(7, 7, 7)), cube(pos - float3(-1, 0, 0.5), 0.8)));
}

float sceneSDF(float3 pos) {
    return subtract(sphere(pos - float3(0,0.5,0), 1.2), union(repeatingSphere(pos, 1, float3(7, 7, 7)), cube(pos - float3(-1, 0 , 0.5), 0.8)));
}
*/
