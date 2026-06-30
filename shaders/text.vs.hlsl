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
};

cbuffer UIPresets : register(b0) {
    uint width;
    uint height;
}

struct Vertex
{
    float3 element : POSITION;
    float2 uv : TEXCOORD;
    uint2 rel_pos : RELPOS;
    float scale_factor : SCALE;
    uint char_idx : CIDX;
    uint tex_idx : TEXIDX;
    float4 color : COLOR;
};

struct Pixel {
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    uint tex_idx : TEXIDX;
};

Pixel main(Vertex v) {
    StructuredBuffer<FontMeta> Fonts = ResourceDescriptorHeap[NonUniformResourceIndex(v.tex_idx)];
    FontMeta fm = Fonts[v.char_idx];
    float scaled_fwidth = fm.width * v.scale_factor;
    float scaled_fheight = fm.height * v.scale_factor;
    float ndc_scaled_bearing_x = fm.bearing_x * v.scale_factor / width;
    float ndc_scaled_bearing_y = fm.bearing_y * v.scale_factor / height;
    float ndc_start_x = -1.0f + (float)(v.rel_pos.x) / (width / 2);
    float ndc_start_y = 1.0f - (float)(v.rel_pos.y) / (height / 2);
    float ch_wscale = fm.width / width * v.scale_factor;
    float ch_hscale = fm.height / height * v.scale_factor;
    float transformed_elx = v.element.x * ch_wscale;
    float transformed_ely = v.element.y * ch_hscale;
    float2 ndc_pos = float2( ndc_start_x + transformed_elx + ndc_scaled_bearing_x, ndc_start_y + transformed_ely + ndc_scaled_bearing_y );
    float ptex_w = fm.tex_u1 - fm.tex_u0;
    float ptex_h = fm.tex_v1 - fm.tex_v0;
    Pixel pixel;
    pixel.position = float4(ndc_pos, 0.0f, 1.0f);
    pixel.uv = float2(fm.tex_u0 + v.uv.x * ptex_w, fm.tex_v0 + v.uv.y * ptex_h);
    pixel.color = v.color;
    pixel.tex_idx = v.tex_idx;
    return pixel;
}
