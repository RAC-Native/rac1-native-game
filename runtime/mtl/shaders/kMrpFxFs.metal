// kMrpFxFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
// rac1: flat inputs (the triangles drawn last vertex first)
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Implementation of signed integer mod accurate to SPIR-V specification
template<typename Tx, typename Ty>
inline Tx spvSMod(Tx x, Ty y)
{
    Tx remainder = x - y * (x / y);
    return select(Tx(remainder + y), remainder, remainder == 0 || (x >= 0) == (y >= 0));
}

struct Push
{
    float4x4 uVP;
    float4 uFrame;
    float4 uXyOff;
    int uMode;
    int uShade;
    int uPass;
    int _pad;
};

struct kMrpFxFs_out
{
    float4 FragColor [[color(0), index(0)]];
    float4 FragFactor [[color(0), index(1)]];
};

struct kMrpFxFs_in
{
    float3 vStq [[user(locn0)]];
    float4 vColor [[user(locn1), center_no_perspective]];
    float4 vFlat [[user(locn2), flat]];
    float vFog [[user(locn3), center_no_perspective]];
    uint4 vTex [[user(locn4)]];
};

static inline __attribute__((always_inline))
int2 wrapc(thread int2& c, thread const int2& sz, thread const bool& rs, thread const bool& rt)
{
    int _20;
    if (rs)
    {
        _20 = spvSMod(spvSMod(c.x, sz.x) + sz.x, sz.x);
    }
    else
    {
        _20 = clamp(c.x, 0, sz.x - 1);
    }
    c.x = _20;
    int _48;
    if (rt)
    {
        _48 = spvSMod(spvSMod(c.y, sz.y) + sz.y, sz.y);
    }
    else
    {
        _48 = clamp(c.y, 0, sz.y - 1);
    }
    c.y = _48;
    return c;
}

fragment kMrpFxFs_out kMrpFxFs(kMrpFxFs_in in [[stage_in]], constant Push& _458 [[buffer(0)]], texture2d_array<float> uAtlas [[texture(0)]], sampler uAtlasSmplr [[sampler(0)]])
{
    kMrpFxFs_out out = {};
    uint fl = in.vTex.z;
    bool tme = (fl & 1u) != 0u;
    bool iip = ((fl >> uint(4)) & 1u) != 0u;
    bool fge = ((fl >> uint(5)) & 1u) != 0u;
    bool ate = ((fl >> uint(6)) & 1u) != 0u;
    uint tfx = (fl >> uint(1)) & 3u;
    uint tcc = (fl >> uint(3)) & 1u;
    uint atst = (fl >> uint(7)) & 7u;
    uint aref = (fl >> uint(10)) & 255u;
    uint afail = (fl >> uint(22)) & 3u;
    bool lin = ((fl >> uint(18)) & 1u) != 0u;
    bool rs = ((fl >> uint(19)) & 1u) != 0u;
    bool rt = ((fl >> uint(20)) & 1u) != 0u;
    float4 c = iip ? in.vColor : in.vFlat;
    float4 o = c;
    if (tme)
    {
        int2 sz = int2(int(in.vTex.y & 65535u), int(in.vTex.y >> uint(16)));
        float2 uv = (in.vStq.xy / float2(in.vStq.z)) * float2(sz);
        int layer = int(in.vTex.x);
        float4 t;
        if (lin)
        {
            float2 p = uv - float2(0.5);
            int2 i0 = int2(floor(p));
            float2 f = p - float2(i0);
            int2 param = i0;
            int2 param_1 = sz;
            bool param_2 = rs;
            bool param_3 = rt;
            int2 _230 = wrapc(param, param_1, param_2, param_3);
            int3 _235 = int3(_230, layer);
            float4 t00 = uAtlas.read(uint2(_235.xy), uint(_235.z), 0);
            int2 param_4 = i0 + int2(1, 0);
            int2 param_5 = sz;
            bool param_6 = rs;
            bool param_7 = rt;
            int2 _250 = wrapc(param_4, param_5, param_6, param_7);
            int3 _254 = int3(_250, layer);
            float4 t10 = uAtlas.read(uint2(_254.xy), uint(_254.z), 0);
            int2 param_8 = i0 + int2(0, 1);
            int2 param_9 = sz;
            bool param_10 = rs;
            bool param_11 = rt;
            int2 _269 = wrapc(param_8, param_9, param_10, param_11);
            int3 _273 = int3(_269, layer);
            float4 t01 = uAtlas.read(uint2(_273.xy), uint(_273.z), 0);
            int2 param_12 = i0 + int2(1);
            int2 param_13 = sz;
            bool param_14 = rs;
            bool param_15 = rt;
            int2 _288 = wrapc(param_12, param_13, param_14, param_15);
            int3 _292 = int3(_288, layer);
            float4 t11 = uAtlas.read(uint2(_292.xy), uint(_292.z), 0);
            t = mix(mix(t00, t10, float4(f.x)), mix(t01, t11, float4(f.x)), float4(f.y));
        }
        else
        {
            int2 param_16 = int2(floor(uv));
            int2 param_17 = sz;
            bool param_18 = rs;
            bool param_19 = rt;
            int2 _325 = wrapc(param_16, param_17, param_18, param_19);
            int3 _329 = int3(_325, layer);
            t = uAtlas.read(uint2(_329.xy), uint(_329.z), 0);
        }
        if (tfx == 0u)
        {
            float3 _342 = (t.xyz * c.xyz) * 1.9921875;
            o.x = _342.x;
            o.y = _342.y;
            o.z = _342.z;
            float _351;
            if (tcc != 0u)
            {
                _351 = (t.w * c.w) * 1.9921875;
            }
            else
            {
                _351 = c.w;
            }
            o.w = _351;
        }
        else
        {
            if (tfx == 1u)
            {
                o.x = t.xyz.x;
                o.y = t.xyz.y;
                o.z = t.xyz.z;
                float _380;
                if (tcc != 0u)
                {
                    _380 = t.w;
                }
                else
                {
                    _380 = c.w;
                }
                o.w = _380;
            }
            else
            {
                if (tfx == 2u)
                {
                    float3 _404 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _404.x;
                    o.y = _404.y;
                    o.z = _404.z;
                    float _413;
                    if (tcc != 0u)
                    {
                        _413 = t.w + c.w;
                    }
                    else
                    {
                        _413 = c.w;
                    }
                    o.w = _413;
                }
                else
                {
                    float3 _436 = ((t.xyz * c.xyz) * 1.9921875) + float3(c.w);
                    o.x = _436.x;
                    o.y = _436.y;
                    o.z = _436.z;
                    float _445;
                    if (tcc != 0u)
                    {
                        _445 = t.w;
                    }
                    else
                    {
                        _445 = c.w;
                    }
                    o.w = _445;
                }
            }
        }
        if (_458.uShade == 2)
        {
            o.x = t.xyz.x;
            o.y = t.xyz.y;
            o.z = t.xyz.z;
        }
    }
    if (_458.uShade == 1)
    {
        o.x = c.xyz.x;
        o.y = c.xyz.y;
        o.z = c.xyz.z;
    }
    o = fast::clamp(o, float4(0.0), float4(1.0));
    if (fge)
    {
        uint fc = in.vTex.w;
        float3 fogcol = float3(float(fc & 255u), float((fc >> uint(8)) & 255u), float((fc >> uint(16)) & 255u)) / float3(255.0);
        float f_1 = in.vFog / 255.0;
        float4 _521 = o;
        float3 _529 = (_521.xyz * f_1) + (fogcol * (1.0 - f_1));
        o.x = _529.x;
        o.y = _529.y;
        o.z = _529.z;
    }
    float a = o.w * 255.0;
    if (ate)
    {
        bool pass = true;
        if (atst == 0u)
        {
            pass = false;
        }
        else
        {
            if (atst == 2u)
            {
                pass = a < float(aref);
            }
            else
            {
                if (atst == 3u)
                {
                    pass = a <= float(aref);
                }
                else
                {
                    if (atst == 4u)
                    {
                        pass = abs(a - float(aref)) < 0.5;
                    }
                    else
                    {
                        if (atst == 5u)
                        {
                            pass = a >= float(aref);
                        }
                        else
                        {
                            if (atst == 6u)
                            {
                                pass = a > float(aref);
                            }
                            else
                            {
                                if (atst == 7u)
                                {
                                    pass = abs(a - float(aref)) >= 0.5;
                                }
                            }
                        }
                    }
                }
            }
        }
        bool _612 = !pass;
        bool _618;
        if (_612)
        {
            _618 = _458.uPass == 0;
        }
        else
        {
            _618 = _612;
        }
        bool _626;
        if (_618)
        {
            _626 = (afail == 0u) || (afail == 2u);
        }
        else
        {
            _626 = _618;
        }
        if (_626)
        {
            discard_fragment();
        }
        bool _630 = !pass;
        bool _636;
        if (_630)
        {
            _636 = _458.uPass == 1;
        }
        else
        {
            _636 = _630;
        }
        if (_636 && (afail != 2u))
        {
            discard_fragment();
        }
    }
    float as = fast::min(o.w * 1.9921875, 1.0);
    uint bk = (fl >> uint(24)) & 3u;
    if ((bk == 1u) || (bk == 2u))
    {
        out.FragColor = float4(o.xyz * as, as);
        float _673;
        if (bk == 1u)
        {
            _673 = 1.0;
        }
        else
        {
            _673 = 1.0 - as;
        }
        out.FragFactor = float4(_673);
    }
    else
    {
        out.FragColor = float4(o.xyz, as);
        out.FragFactor = float4(0.0);
    }
    return out;
}

