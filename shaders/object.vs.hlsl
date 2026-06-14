struct Vertex
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct Pixel
{
    float4 position : SV_Position;
    float3 w_position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

cbuffer Presets : register(b0) {
    row_major float4x4 world;
    uint tex_index;
}

cbuffer Scene : register(b1) {
    row_major float4x4 vp;
    float4 camera_pos;
    uint dotlight_count;
    float3 dotlight_pos[16];
    float3 dotlight_color[16];
}

Pixel main(Vertex v) {
    Pixel p;
    row_major float4x4 wvp = mul(world, vp);
    p.position = mul(float4(v.position, 1.0), wvp);
    p.w_position = mul(float4(v.position, 1.0), world);
    p.normal = mul(v.normal, transpose((float3x3)world));
    p.uv = v.uv;
    return p;
}
