// Hard-coded triangle: positions and colors indexed by vertex ID.

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

static const float3 positions[3] = {
    float3( 1.0,  1.0, 0.0),
    float3(-1.0,  1.0, 0.0),
    float3( 0.0, -1.0, 0.0),
};

static const float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
};

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.position = float4(positions[vertexID], 1.0);
    output.color    = colors[vertexID];
    return output;
}
