struct Vertex
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct Pixel {
    float4 position : SV_Position;
};

cbuffer ObjectInfo : register(b1) {
    row_major float4x4 world;
}

cbuffer Scene : register(b0) {
    row_major float4x4 vp;
    row_major float4x4 light_vp;
    float4 ambient_light;
    float4 camera_pos;
    uint dotlight_count;
    float4 dotlight_pos[4];
    float4 dotlight_color[4];
    uint4 shadow_index;
}

Pixel main(Vertex v) {
    Pixel p;
    row_major float4x4 wvp = mul(world, light_vp);
    p.position = mul(float4(v.position, 1.0), wvp);
    return p;
}