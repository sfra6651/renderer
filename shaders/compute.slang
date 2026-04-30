// Compute shader: gradient fill, leaves a 1-pixel black border at each
// workgroup's local origin to visualize the 16x16 dispatch tiling.

[[vk::binding(0, 0)]]
RWTexture2D<float4> image;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(
    uint3 globalID : SV_DispatchThreadID,
    uint3 localID  : SV_GroupThreadID)
{
    int2 texelCoord = int2(globalID.xy);

    uint2 size;
    image.GetDimensions(size.x, size.y);

    if (texelCoord.x < int(size.x) && texelCoord.y < int(size.y))
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        if (localID.x != 0 && localID.y != 0)
        {
            color.x = float(texelCoord.x) / float(size.x);
            color.y = float(texelCoord.y) / float(size.y);
        }
        image[texelCoord] = color;
    }
}
