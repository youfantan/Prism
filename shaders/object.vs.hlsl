struct Vertex
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct Pixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

cbuffer Mapping : register(b0) {
    uint tex_index;
}

cbuffer Transformation : register(b1) {
    row_major float4x4 wvp;
}

Pixel main(Vertex v) {
    Pixel p;
    p.position = mul(float4(v.position, 1.0), wvp);
    p.uv = v.uv;
    return p;
}
