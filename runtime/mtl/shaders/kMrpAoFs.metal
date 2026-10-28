// kMrpAoFs (frag): made by tools/gen_metal_shaders.py from tools/gen_vk_shaders.py - do not edit.
#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Push
{
    float4x4 uInvVP;
    float4x4 uVP;
    float4 uCam_Pad;
    float4 uParam;
    float4 uDMap;
};

struct kMrpAoFs_out
{
    float FragAo [[color(0)]];
};

struct kMrpAoFs_in
{
    float2 vUv [[user(locn0)]];
};

static inline __attribute__((always_inline))
float3 world(thread const float2& uv, thread const float& d, constant Push& _22)
{
    float4 p = _22.uInvVP * float4((uv * 2.0) - float2(1.0), (d * 2.0) - 1.0, 1.0);
    return p.xyz / float3(p.w);
}

fragment kMrpAoFs_out kMrpAoFs(kMrpAoFs_in in [[stage_in]], constant Push& _22 [[buffer(0)]], texture2d<float> uDepth [[texture(0)]], sampler uDepthSmplr [[sampler(0)]], float4 gl_FragCoord [[position]])
{
    kMrpAoFs_out out = {};
    float d = uDepth.sample(uDepthSmplr, ((in.vUv * _22.uDMap.zw) + _22.uDMap.xy)).x;
    float2 param = in.vUv;
    float param_1 = d;
    float3 P = world(param, param_1, _22);
    float3 dPdx = dfdx(P);
    float3 dPdy = dfdy(P);
    if (d >= 1.0)
    {
        out.FragAo = 1.0;
        return out;
    }
    int mode = int(_22.uParam.w + 0.5);
    float3 N;
    if (mode <= 1)
    {
        N = cross(dPdx, dPdy);
    }
    else
    {
        float2 texel = float2(1.0) / (float2(int2(uDepth.get_width(), uDepth.get_height())) * _22.uDMap.zw);
        float dr = uDepth.sample(uDepthSmplr, (((in.vUv + float2(texel.x, 0.0)) * _22.uDMap.zw) + _22.uDMap.xy)).x;
        float dl = uDepth.sample(uDepthSmplr, (((in.vUv - float2(texel.x, 0.0)) * _22.uDMap.zw) + _22.uDMap.xy)).x;
        float du = uDepth.sample(uDepthSmplr, (((in.vUv + float2(0.0, texel.y)) * _22.uDMap.zw) + _22.uDMap.xy)).x;
        float dd = uDepth.sample(uDepthSmplr, (((in.vUv - float2(0.0, texel.y)) * _22.uDMap.zw) + _22.uDMap.xy)).x;
        float3 _208;
        if ((abs(dr - d) < abs(d - dl)) && (dr < 1.0))
        {
            float2 param_2 = in.vUv + float2(texel.x, 0.0);
            float param_3 = dr;
            _208 = world(param_2, param_3, _22) - P;
        }
        else
        {
            float3 _225;
            if (dl < 1.0)
            {
                float2 param_4 = in.vUv - float2(texel.x, 0.0);
                float param_5 = dl;
                _225 = P - world(param_4, param_5, _22);
            }
            else
            {
                _225 = dPdx;
            }
            _208 = _225;
        }
        float3 dPx = _208;
        float3 _256;
        if ((abs(du - d) < abs(d - dd)) && (du < 1.0))
        {
            float2 param_6 = in.vUv + float2(0.0, texel.y);
            float param_7 = du;
            _256 = world(param_6, param_7, _22) - P;
        }
        else
        {
            float3 _273;
            if (dd < 1.0)
            {
                float2 param_8 = in.vUv - float2(0.0, texel.y);
                float param_9 = dd;
                _273 = P - world(param_8, param_9, _22);
            }
            else
            {
                _273 = dPdy;
            }
            _256 = _273;
        }
        float3 dPy = _256;
        N = cross(dPx, dPy);
    }
    if (dot(N, N) < 9.9999999600419720025001879548654e-13)
    {
        out.FragAo = 1.0;
        return out;
    }
    N = fast::normalize(N);
    float3 V = _22.uCam_Pad.xyz - P;
    float vlen = length(V);
    V /= float3(fast::max(vlen, 9.9999997473787516355514526367188e-06));
    if (dot(N, V) < 0.0)
    {
        N = -N;
    }
    float _noise = fract(52.98291778564453125 * fract(dot(gl_FragCoord.xy, float2(0.067110560834407806396484375, 0.005837149918079376220703125))));
    float _noise2 = fract(52.98291778564453125 * fract(dot(gl_FragCoord.xy + float2(17.0, 31.0), float2(0.067110560834407806396484375, 0.005837149918079376220703125))));
    float3 _359;
    if (abs(N.z) < 0.89999997615814208984375)
    {
        _359 = cross(N, float3(0.0, 0.0, 1.0));
    }
    else
    {
        _359 = cross(N, float3(1.0, 0.0, 0.0));
    }
    float3 t = fast::normalize(_359);
    float3 b = cross(N, t);
    float R = _22.uParam.x;
    float4 c0 = _22.uVP * float4(P, 1.0);
    float4 c1 = _22.uVP * float4(P + (b * R), 1.0);
    bool _402 = c0.w > 0.0;
    bool _408;
    if (_402)
    {
        _408 = c1.w > 0.0;
    }
    else
    {
        _408 = _402;
    }
    if (_408)
    {
        float px = length((c1.xy / float2(c1.w)) - (c0.xy / float2(c0.w))) * 0.5;
        if (px > 0.039999999105930328369140625)
        {
            R *= (0.039999999105930328369140625 / px);
        }
    }
    float invR2 = 1.0 / fast::max(R * R, 9.9999997473787516355514526367188e-05);
    if (mode <= 2)
    {
        float a = 6.283185482025146484375 * _noise;
        float3 T = (t * cos(a)) + (b * sin(a));
        float3 B = cross(N, T);
        float occ = 0.0;
        for (int i = 0; i < 12; i++)
        {
            float fi = (float(i) + 0.5) / 12.0;
            float z = sqrt(1.0 - fi);
            float r = sqrt(fi);
            float ph = float(i) * 2.3999631404876708984375;
            float3 h = ((T * (r * cos(ph))) + (B * (r * sin(ph)))) + (N * z);
            float s = mix(0.1500000059604644775390625, 1.0, fi * fi) * R;
            float3 S = (P + (N * _22.uParam.z)) + (h * s);
            float4 c = _22.uVP * float4(S, 1.0);
            if (c.w <= 0.0)
            {
                continue;
            }
            float2 suv = ((c.xy / float2(c.w)) * 0.5) + float2(0.5);
            bool _559 = any(suv < float2(0.0));
            bool _567;
            if (!_559)
            {
                _567 = any(suv > float2(1.0));
            }
            else
            {
                _567 = _559;
            }
            if (_567)
            {
                continue;
            }
            float sd = uDepth.sample(uDepthSmplr, ((suv * _22.uDMap.zw) + _22.uDMap.xy)).x;
            float zs = ((c.z / c.w) * 0.5) + 0.5;
            if (sd < (zs - 1.9999999494757503271102905273438e-05))
            {
                float2 param_10 = suv;
                float param_11 = sd;
                float dist = length(world(param_10, param_11, _22) - S);
                occ += smoothstep(0.0, 1.0, ((R * 1.5) / fast::max(dist, 0.001000000047497451305389404296875)) - 0.5);
            }
        }
        out.FragAo = 1.0 - ((_22.uParam.y * occ) / 12.0);
        return out;
    }
    if (mode == 3)
    {
        float occ_1 = 0.0;
        for (int d_i = 0; d_i < 4; d_i++)
        {
            float phi = (float(d_i) + _noise) * 1.57079637050628662109375;
            float3 dir = (t * cos(phi)) + (b * sin(phi));
            float max_sin_h = 0.119999997317790985107421875;
            float best_att = 0.0;
            for (int s_i = 1; s_i <= 3; s_i++)
            {
                float frac_r = ((float(s_i) - 0.5) + (0.5 * _noise2)) / 3.0;
                float3 Q = (P + (N * _22.uParam.z)) + (dir * (frac_r * R));
                float4 c_1 = _22.uVP * float4(Q, 1.0);
                if (c_1.w <= 0.0)
                {
                    continue;
                }
                float2 suv_1 = ((c_1.xy / float2(c_1.w)) * 0.5) + float2(0.5);
                bool _718 = any(suv_1 < float2(0.0));
                bool _725;
                if (!_718)
                {
                    _725 = any(suv_1 > float2(1.0));
                }
                else
                {
                    _725 = _718;
                }
                if (_725)
                {
                    continue;
                }
                float sd_1 = uDepth.sample(uDepthSmplr, ((suv_1 * _22.uDMap.zw) + _22.uDMap.xy)).x;
                if (sd_1 >= 1.0)
                {
                    continue;
                }
                float2 param_12 = suv_1;
                float param_13 = sd_1;
                float3 H = world(param_12, param_13, _22) - P;
                float d2 = dot(H, H);
                bool _760 = d2 < 9.9999997473787516355514526367188e-06;
                bool _771;
                if (!_760)
                {
                    _771 = d2 > ((R * R) * 1.440000057220458984375);
                }
                else
                {
                    _771 = _760;
                }
                if (_771)
                {
                    continue;
                }
                float sin_h = dot(H, N) * rsqrt(d2);
                if (sin_h > max_sin_h)
                {
                    float att = fast::clamp(1.0 - (d2 * invR2), 0.0, 1.0);
                    best_att += ((sin_h - max_sin_h) * att);
                    max_sin_h = sin_h;
                }
            }
            occ_1 += fast::clamp(best_att, 0.0, 1.0);
        }
        out.FragAo = fast::clamp(1.0 - ((_22.uParam.y * 1.25) * (occ_1 / 4.0)), 0.0, 1.0);
        return out;
    }
    float vis = 0.0;
    for (int d_i_1 = 0; d_i_1 < 3; d_i_1++)
    {
        float phi_1 = (float(d_i_1) + _noise) * 1.0471975803375244140625;
        float3 dir_1 = (t * cos(phi_1)) + (b * sin(phi_1));
        float h1 = -1.0;
        float h2 = -1.0;
        for (int s_i_1 = 1; s_i_1 <= 3; s_i_1++)
        {
            float step_r = (((float(s_i_1) - 0.5) + (0.5 * _noise2)) / 3.0) * R;
            float4 cp = _22.uVP * float4((P + (N * _22.uParam.z)) + (dir_1 * step_r), 1.0);
            if (cp.w > 0.0)
            {
                float2 uvp = ((cp.xy / float2(cp.w)) * 0.5) + float2(0.5);
                bool _902 = all(uvp >= float2(0.0));
                bool _908;
                if (_902)
                {
                    _908 = all(uvp <= float2(1.0));
                }
                else
                {
                    _908 = _902;
                }
                if (_908)
                {
                    float sdp = uDepth.sample(uDepthSmplr, ((uvp * _22.uDMap.zw) + _22.uDMap.xy)).x;
                    if (sdp < 1.0)
                    {
                        float2 param_14 = uvp;
                        float param_15 = sdp;
                        float3 Hp = world(param_14, param_15, _22) - P;
                        float d2_1 = dot(Hp, Hp);
                        if (d2_1 > 9.9999997473787516355514526367188e-06)
                        {
                            float c_h = dot(Hp, N) * rsqrt(d2_1);
                            float falloff = fast::clamp(1.0 - (d2_1 * invR2), 0.0, 1.0);
                            h1 = fast::max(h1, mix(-1.0, c_h, falloff));
                        }
                    }
                }
            }
            float4 cn = _22.uVP * float4((P + (N * _22.uParam.z)) - (dir_1 * step_r), 1.0);
            if (cn.w > 0.0)
            {
                float2 uvn = ((cn.xy / float2(cn.w)) * 0.5) + float2(0.5);
                bool _997 = all(uvn >= float2(0.0));
                bool _1003;
                if (_997)
                {
                    _1003 = all(uvn <= float2(1.0));
                }
                else
                {
                    _1003 = _997;
                }
                if (_1003)
                {
                    float sdn = uDepth.sample(uDepthSmplr, ((uvn * _22.uDMap.zw) + _22.uDMap.xy)).x;
                    if (sdn < 1.0)
                    {
                        float2 param_16 = uvn;
                        float param_17 = sdn;
                        float3 Hn = world(param_16, param_17, _22) - P;
                        float d2_2 = dot(Hn, Hn);
                        if (d2_2 > 9.9999997473787516355514526367188e-06)
                        {
                            float c_h_1 = dot(Hn, N) * rsqrt(d2_2);
                            float falloff_1 = fast::clamp(1.0 - (d2_2 * invR2), 0.0, 1.0);
                            h2 = fast::max(h2, mix(-1.0, c_h_1, falloff_1));
                        }
                    }
                }
            }
        }
        float slice_vis = 0.5 * ((1.0 - fast::max(h1, 0.0)) + (1.0 - fast::max(h2, 0.0)));
        vis += fast::clamp(slice_vis, 0.0, 1.0);
    }
    vis /= 3.0;
    float mb = vis / ((vis * 0.60000002384185791015625) + 0.4000000059604644775390625);
    out.FragAo = fast::clamp(mix(1.0, mix(vis, mb, 0.3499999940395355224609375), _22.uParam.y * 1.14999997615814208984375), 0.0, 1.0);
    return out;
}

