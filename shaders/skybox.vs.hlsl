struct Vertex
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    uint dir : SKYBOXDIR;
};

struct Pixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    uint dir : SKYBOXDIR;
};

cbuffer Presets : register(b0) {
    row_major float4x4 world;
    uint back_tex_idx : BACKIDX;
    uint bottom_tex_idx : BOTTOMIDX;
    uint front_tex_idx : FRONTIDX;
    uint left_tex_idx : LEFTIDX;
    uint right_tex_idx : RIGHTIDX;
    uint top_tex_idx : TOPIDX;
}

cbuffer Scene : register(b1) {
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
    row_major float4x4 wvp = mul(world, vp);
    Pixel p;
    p.position = mul(float4(v.position, 1.0), wvp);
    p.uv = v.uv;
    p.dir = v.dir;
    return p;
}