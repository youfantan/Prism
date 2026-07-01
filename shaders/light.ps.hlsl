struct Pixel
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

cbuffer Light : register(b1) {
    row_major float4x4 world;
    float4 color;
}

float4 main(Pixel p) : SV_Target {
    return float4(color.rgb, 1.0f);
}