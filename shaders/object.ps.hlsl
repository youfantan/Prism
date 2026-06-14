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
    float4 dotlight_pos[16];
    float4 dotlight_color[16];
}

Texture2D Textures[8] : register(t0);
SamplerState Sampler : register(s0);

float4 main(Pixel p) : SV_Target {
    float3 N = normalize(p.normal);
    float3 V = normalize(camera_pos.xyz - p.w_position);
    float3 tex_color = Textures[NonUniformResourceIndex(tex_index)].Sample(Sampler, p.uv);
    float3 result;
    for (uint i = 0; i < dotlight_count; i++) {
        float3 L = normalize(dotlight_pos[i].xyz - p.w_position);
        float diffuse = saturate(dot(N, L));
        float3 R = reflect(-L, N);
        float intensity = pow(saturate(dot(R, V)), 32);
        result += dotlight_color[i].rgb * diffuse * tex_color + intensity * dotlight_color[i].rgb;
    }
    return float4(result, 1.0);
}