// RgbToNv12.hlsl
// Converts BGRA8_UNORM to NV12 format (Y, UV)

cbuffer Params : register(b0)
{
    uint Width;
    uint Height;
};

Texture2D<float4> inputTex : register(t0);
RWTexture2D<float> lumaOut : register(u0);
RWTexture2D<float2> chromaOut : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= Width || DTid.y >= Height)
    {
        return;
    }

    // Read input pixel
    // Assuming SRV format DXGI_FORMAT_B8G8R8A8_UNORM which hardware swizzles 
    // to output R in .r, G in .g, B in .b. If not swizzled by hardware, 
    // these would need to be swapped (e.g. R = color.b, B = color.r).
    float4 color = inputTex[DTid.xy];
    float R = color.r;
    float G = color.g;
    float B = color.b;

    // BT.709 Luma
    float Y = 0.2126 * R + 0.7152 * G + 0.0722 * B;
    lumaOut[DTid.xy] = saturate(Y);

    // Threads at even (x,y) calculate chroma by averaging 2x2 block
    if ((DTid.x % 2 == 0) && (DTid.y % 2 == 0))
    {
        // Gather 2x2 block colors
        float4 c00 = color;
        float4 c10 = (DTid.x + 1 < Width) ? inputTex[DTid.xy + uint2(1, 0)] : c00;
        float4 c01 = (DTid.y + 1 < Height) ? inputTex[DTid.xy + uint2(0, 1)] : c00;
        float4 c11 = (DTid.x + 1 < Width && DTid.y + 1 < Height) ? inputTex[DTid.xy + uint2(1, 1)] : c00;

        float avgR = (c00.r + c10.r + c01.r + c11.r) * 0.25;
        float avgG = (c00.g + c10.g + c01.g + c11.g) * 0.25;
        float avgB = (c00.b + c10.b + c01.b + c11.b) * 0.25;

        // BT.709 Chroma
        float Cb = -0.1146 * avgR - 0.3854 * avgG + 0.5000 * avgB + 0.5;
        float Cr =  0.5000 * avgR - 0.4542 * avgG - 0.0458 * avgB + 0.5;

        chromaOut[DTid.xy / 2] = saturate(float2(Cb, Cr));
    }
}
