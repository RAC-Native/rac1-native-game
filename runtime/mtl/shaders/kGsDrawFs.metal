// kGsDrawFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: flat inputs (the triangles drawn last vertex first)
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float2 uFb;
    float uScaleX;
    int uRt;
    packed_float3 uFogCol;
    int uFge;
    float4 uScrMap;
    int uTme;
    int uTfx;
    int uTcc;
    int uAte;
    int uAtst;
    int uAref;
    int uAfail;
    int uPass;
    int uScrTex;
    int uDate;
    int uDatm;
    int uMipN;
    int uMipLo;
    int uMipLcm;
    int uMipMxl;
    float uMipK;
    float uMipL;
    int _pad0;
    int _pad1;
    int _pad2;
};

struct kGsDrawFs_out
{
    float4 FragColor [[color(0), index(0)]];
    float4 FragFactor [[color(0), index(1)]];
};

struct kGsDrawFs_in
{
    float4 vColor [[user(locn0)]];
    float3 vTex [[user(locn1)]];
    float vFog [[user(locn2)]];
    float4 vClamp [[user(locn3), flat]];
};

fragment kGsDrawFs_out kGsDrawFs(kGsDrawFs_in in [[stage_in]], constant Push& _13 [[buffer(0)]], texture2d<float> uScreen [[texture(0)]], texture2d<float> uTexture [[texture(1)]], texture2d<float> uMip1 [[texture(2)]], texture2d<float> uMip2 [[texture(3)]], texture2d<float> uMip3 [[texture(4)]], sampler uScreenSmplr [[sampler(0)]], sampler uTextureSmplr [[sampler(1)]], sampler uMip1Smplr [[sampler(2)]], sampler uMip2Smplr [[sampler(3)]], sampler uMip3Smplr [[sampler(4)]], float4 gl_FragCoord [[position]])
{
    kGsDrawFs_out out = {};
    if (_13.uDate != 0)
    {
        bool b7 = uScreen.read(uint2(int2(gl_FragCoord.xy)), 0).w > 0.996078431606292724609375;
        if ((_13.uDatm == 0) == b7)
        {
            discard_fragment();
        }
    }
    float4 c = in.vColor;
    float4 o = c;
    if (_13.uTme != 0)
    {
        float2 st = in.vTex.xy / float2(in.vTex.z);
        float4 t;
        if (_13.uScrTex != 0)
        {
            t = uScreen.read(uint2(int2(int(_13.uScrMap.x + (_13.uScrMap.y * st.x)), int(_13.uScrMap.z + (_13.uScrMap.w * st.y)))), 0);
            t.w *= 0.501960813999176025390625;
        }
        else
        {
            int idx = 0;
            if (_13.uMipN > 0)
            {
                float _128;
                if (_13.uMipLcm != 0)
                {
                    _128 = _13.uMipK;
                }
                else
                {
                    _128 = (log2(1.0 / fast::max(abs(in.vTex.z), 9.9999996826552253889678874634872e-21)) * _13.uMipL) + _13.uMipK;
                }
                float lod = _128;
                int _155;
                if (lod <= 0.0)
                {
                    _155 = 0;
                }
                else
                {
                    _155 = min(int(floor(lod + 0.5)), _13.uMipMxl);
                }
                int level0 = _155;
                idx = clamp(level0 - _13.uMipLo, 0, _13.uMipN);
            }
            if (in.vClamp.x <= in.vClamp.z)
            {
                st = fast::clamp(st, in.vClamp.xy, in.vClamp.zw);
            }
            if (idx == 0)
            {
                t = uTexture.sample(uTextureSmplr, st);
            }
            else
            {
                if (idx == 1)
                {
                    t = uMip1.sample(uMip1Smplr, st);
                }
                else
                {
                    if (idx == 2)
                    {
                        t = uMip2.sample(uMip2Smplr, st);
                    }
                    else
                    {
                        t = uMip3.sample(uMip3Smplr, st);
                    }
                }
            }
        }
        if (_13.uTfx == 0)
        {
            float3 _236 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _236.x;
            o.y = _236.y;
            o.z = _236.z;
            float _247;
            if (_13.uTcc != 0)
            {
                _247 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _247 = c.w;
            }
            o.w = _247;
        }
        else
        {
            if (_13.uTfx == 1)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _278;
                if (_13.uTcc != 0)
                {
                    _278 = t.w;
                }
                else
                {
                    _278 = c.w;
                }
                o.w = _278;
            }
            else
            {
                if (_13.uTfx == 2)
                {
                    float3 _303 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _303.x;
                    o.y = _303.y;
                    o.z = _303.z;
                    float _313;
                    if (_13.uTcc != 0)
                    {
                        _313 = t.w + c.w;
                    }
                    else
                    {
                        _313 = c.w;
                    }
                    o.w = _313;
                }
                else
                {
                    float3 _336 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _336.x;
                    o.y = _336.y;
                    o.z = _336.z;
                    float _346;
                    if (_13.uTcc != 0)
                    {
                        _346 = t.w;
                    }
                    else
                    {
                        _346 = c.w;
                    }
                    o.w = _346;
                }
            }
        }
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (_13.uFge != 0)
    {
        float f = in.vFog / 255.0;
        float4 _371 = o;
        float3 _382 = (_371.xyz * f) + (float3(_13.uFogCol) * (1.0 - f));
        o.x = _382.x;
        o.y = _382.y;
        o.z = _382.z;
    }
    float a = o.w * 255.0;
    if (_13.uAte != 0)
    {
        bool pass = true;
        if (_13.uAtst == 0)
        {
            pass = false;
        }
        else
        {
            if (_13.uAtst == 2)
            {
                pass = a < float(_13.uAref);
            }
            else
            {
                if (_13.uAtst == 3)
                {
                    pass = a <= float(_13.uAref);
                }
                else
                {
                    if (_13.uAtst == 4)
                    {
                        pass = abs(a - float(_13.uAref)) < 0.5;
                    }
                    else
                    {
                        if (_13.uAtst == 5)
                        {
                            pass = a >= float(_13.uAref);
                        }
                        else
                        {
                            if (_13.uAtst == 6)
                            {
                                pass = a > float(_13.uAref);
                            }
                            else
                            {
                                if (_13.uAtst == 7)
                                {
                                    pass = abs(a - float(_13.uAref)) >= 0.5;
                                }
                            }
                        }
                    }
                }
            }
        }
        bool _480 = !pass;
        bool _487;
        if (_480)
        {
            _487 = _13.uPass == 0;
        }
        else
        {
            _487 = _480;
        }
        bool _501;
        if (_487)
        {
            bool _493 = _13.uAfail == 0;
            bool _500;
            if (!_493)
            {
                _500 = _13.uAfail == 2;
            }
            else
            {
                _500 = _493;
            }
            _501 = _500;
        }
        else
        {
            _501 = _487;
        }
        if (_501)
        {
            discard_fragment();
        }
        bool _505 = !pass;
        bool _511;
        if (_505)
        {
            _511 = _13.uPass == 1;
        }
        else
        {
            _511 = _505;
        }
        bool _517;
        if (_511)
        {
            _517 = _13.uAfail != 2;
        }
        else
        {
            _517 = _511;
        }
        if (_517)
        {
            discard_fragment();
        }
    }
    float4 _525;
    if (_13.uRt != 0)
    {
        _525 = o;
    }
    else
    {
        _525 = float4(o.xyz, fast::min(o.w * 1.9921875, 1.0));
    }
    out.FragColor = _525;
    out.FragFactor = float4(fast::min(o.w * 1.9921875, 1.0));
    return out;
}

