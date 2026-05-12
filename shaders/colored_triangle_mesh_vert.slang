// Pulls vertex data from a buffer via buffer device address; transforms
// position by the push-constant render matrix.

struct Vertex {
    float3 position;
    float  uv_x;
    float3 normal;
    float  uv_y;
    float4 color;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

struct PushConstants {
    float4x4 render_matrix;
    Vertex  *vertexBuffer;
};

[[vk::push_constant]] PushConstants pc;

[shader("vertex")]
VSOutput main(uint vertexID : SV_VertexID)
{
    Vertex v = pc.vertexBuffer[vertexID];

    VSOutput output;
    output.position = mul(pc.render_matrix, float4(v.position, 1.0));
    output.color    = v.color.xyz;
    output.uv       = float2(v.uv_x, v.uv_y);
    return output;
}
