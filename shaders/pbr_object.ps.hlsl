struct Pixel
{
    float4 position : SV_Position;
    float4 light_position : LPOS;
    float3 world_position : WPOS;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    uint tex_idx : TEXIDX;
    uint normal_tex_idx : NTEXIDX;
    uint rough_tex_idx : RTEXIDX;
};

cbuffer Presets : register(b1) {
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

SamplerState LinearSampler : register(s0);
SamplerComparisonState CompSampler : register(s1);

#define PI 3.14159265359

float G1_Schlick(float NdotV, float k) {
    return NdotV / (NdotV * (1 - k) + k);
}

float3 PBR_Diffuse(float3 baseColor, float3 F, float metallic) {
    float3 kD = (1 - F) * (1 - metallic);
    return kD * baseColor / PI;
}

float3 PBR_Specular(float3 N, float3 V, float3 L, float3 H, float roughness, float3 F0) {
    float NdotV = max(dot(N, V), 1e-6);
    float NdotL = max(dot(N, L), 1e-6);
    float NdotH = max(dot(N, H), 1e-6);
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1) + 1;
    float D = a2 / (PI * denom * denom);
    float k = (roughness + 1) * (roughness + 1) / 8;
    float G = G1_Schlick(NdotL, k) * G1_Schlick(NdotV, k);
    float3 F = F0 + (1 - F0) * pow(1 - NdotV, 5);
    return D * G * F / (4 * NdotV * NdotL + 1e-6);
}

float3 PBR_Light(float3 world_pos, float3 camera_pos, float3 light_pos,
                 float3 normal, float3 light_color, float3 baseColor,
                 float rough, float metallic) {
    float3 L = normalize(light_pos - world_pos);
    float3 V = normalize(camera_pos - world_pos);
    float3 H = normalize(L + V);
    float3 N = normalize(normal);
    float NdotL = max(dot(N, L), 0);
    float3 F0 = lerp(0.04, baseColor, metallic);
    float3 F = F0 + (1 - F0) * pow(1 - max(dot(N, V), 0), 5);
    float3 diffuse  = PBR_Diffuse(baseColor, F, metallic);
    float3 specular = PBR_Specular(N, V, L, H, rough, F0);
    return (diffuse + specular) * light_color * NdotL;
}

float4 main(Pixel p) : SV_Target {
    Texture2D shadow_map = ResourceDescriptorHeap[NonUniformResourceIndex(shadow_index[0])];
    float3 tex_color;
    float alpha;
    if (p.tex_idx == 0xFFFFFFFF) {
        tex_color = 1.0;
        alpha = 1.0;
    } else {
        Texture2D tex = ResourceDescriptorHeap[NonUniformResourceIndex(p.tex_idx)];
        float4 sampled = tex.Sample(LinearSampler, p.uv);
        tex_color = sampled.rgb;
        alpha = sampled.a;
    }

    float3 N;
    if (p.normal_tex_idx == 0xFFFFFFFF) {
        N = normalize(p.normal);
    } else {
        Texture2D normal_tex = ResourceDescriptorHeap[NonUniformResourceIndex(p.normal_tex_idx)];
        float3 dx = ddx(p.world_position);
        float3 dy = ddy(p.world_position);
        float3 T = normalize(dx - dot(dx, p.normal) * p.normal);
        float3 B = cross(p.normal, T);
        float3x3 TBN = float3x3(T, B, p.normal);
        float3 tnormal = normal_tex.Sample(LinearSampler, p.uv).xyz * 2.0 - 1.0;
        N = normalize(mul(tnormal, TBN));
    }

    float rough;
    float metallic;
    if (p.rough_tex_idx == 0xFFFFFFFF) {
        rough = 1.0;
        metallic = 0.0;
    } else {
        Texture2D rough_tex = ResourceDescriptorHeap[NonUniformResourceIndex(p.rough_tex_idx)];
        float4 mr = rough_tex.Sample(LinearSampler, p.uv);
        rough = mr.y;
        metallic = mr.z;
    }

    float3 ndc = p.light_position.xyz / p.light_position.w;
    float2 uv = ndc.xy * 0.5 + 0.5;
    float d = ndc.z;
    float shadow = shadow_map.SampleCmpLevelZero(CompSampler, uv, d);
    float3 result = (float3)0;
    for (uint i = 0; i < dotlight_count; i++) {
        float3 light = PBR_Light(p.world_position, camera_pos.xyz, dotlight_pos[i].xyz, N, dotlight_color[i], tex_color, rough, metallic);
        result += light * shadow;
    }
    float3 ambient = ambient_light.rgb * tex_color;
    return float4(result + ambient, alpha);
}
