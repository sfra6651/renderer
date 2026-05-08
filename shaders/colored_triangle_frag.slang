// Pass-through: write the interpolated vertex color.

struct FSInput {
    float3 color : COLOR0;
};

[shader("fragment")]
float4 main(FSInput input) : SV_Target
{
    return float4(input.color, 1.0);
}
