struct Vertex
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct Pixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

cbuffer Scene : register(b0) {
    row_major float4x4 vp;
    float4 camera_pos;
    uint dotlight_count;
    float3 dotlight_pos[16];
    float3 dotlight_color[16];
}

cbuffer Light : register(b1) {
    row_major float4x4 world;
    float4 color;
}

Pixel main(Vertex v) {
    Pixel p;
    row_major float4x4 wvp = mul(world, vp);
    p.position = mul(float4(v.position, 1.0), wvp);
    p.uv = v.uv;
    return p;
}
