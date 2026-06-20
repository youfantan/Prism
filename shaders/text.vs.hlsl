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
    uint tex_index;
    uint sb_index;
    uint width;
    uint height;
    float4 text_color;
    float scale_factor;
}

struct Vertex
{
    float3 element : POSITION;
    float2 uv : TEXCOORD;
    uint2 pos : RELPOS;
    uint idx : CIDX;
};

struct Pixel {
    float4 position : SV_Position;
    float4 debug : DEBUG;
    float2 uv : TEXCOORD;
};

Pixel main(Vertex v) {
    StructuredBuffer<FontMeta> Fonts = ResourceDescriptorHeap[NonUniformResourceIndex(sb_index)];
    FontMeta fm = Fonts[v.idx];
    float scaled_fwidth = fm.width * scale_factor;
    float scaled_fheight = fm.height * scale_factor;
    float ndc_scaled_bearing_x = fm.bearing_x * scale_factor / width;
    float ndc_scaled_bearing_y = fm.bearing_y * scale_factor / height;
    float ndc_start_x = -1.0f + (float)(v.pos.x) / (width / 2);
    float ndc_start_y = 1.0f - (float)(v.pos.y) / (height / 2);
    float ch_wscale = fm.width / width * scale_factor;
    float ch_hscale = fm.height / height * scale_factor;
    float transformed_elx = v.element.x * ch_wscale;
    float transformed_ely = v.element.y * ch_hscale;
    float2 ndc_pos = float2( ndc_start_x + transformed_elx + ndc_scaled_bearing_x, ndc_start_y + transformed_ely + ndc_scaled_bearing_y );
    float ptex_w = fm.tex_u1 - fm.tex_u0;
    float ptex_h = fm.tex_v1 - fm.tex_v0;
    Pixel pixel;
    pixel.debug = float4(ndc_start_x, ndc_start_y, transformed_elx, transformed_ely);
    pixel.position = float4(ndc_pos, 0.0f, 1.0f);
    pixel.uv = float2(fm.tex_u0 + v.uv.x * ptex_w, fm.tex_v0 + v.uv.y * ptex_h);
    return pixel;
}
