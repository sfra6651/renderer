// Compute shader: vertical gradient between two push-constant colors.
// data1 is the top color, data2 is the bottom color.

struct PushConstants {
    float4 data1;
    float4 data2;
    float4 data3;
    float4 data4;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pushConstants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> image;

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 globalID : SV_DispatchThreadID)
{
    int2 texelCoord = int2(globalID.xy);

    uint2 size;
    image.GetDimensions(size.x, size.y);

    if (texelCoord.x < int(size.x) && texelCoord.y < int(size.y))
    {
        float blend = float(texelCoord.y) / float(size.y);
        image[texelCoord] = lerp(pushConstants.data1, pushConstants.data2, blend);
    }
}
