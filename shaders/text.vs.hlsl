struct FontMeta {
    uint ch;
    float tex_u0;
    float tex_v0;
    float tex_u1;
    float tex_v1;
    float width;
    float height;
    float bearing_x;
    float bearing_y;
    float advance;
}

StructuredBuffer<FontMeta> Fonts : register(t0);

struct Vertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};
