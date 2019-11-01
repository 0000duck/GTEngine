// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2019/05/01)

cbuffer PVWMatrix
{
    float4x4 pvwMatrix;
};

static float4 color[4] =
{
    float4(0.0f, 0.0f, 0.5f, 1.0f),
    float4(0.5f, 0.0f, 0.0f, 1.0f),
    float4(0.0f, 0.5f, 0.5f, 1.0f),
    float4(0.5f, 0.5f, 0.0f, 1.0f)
};

struct VS_INPUT
{
    float4 modelPositionColorIndex : POSITION;
};

struct VS_OUTPUT
{
    float4 vertexColor : COLOR0;
    float4 clipPosition : SV_POSITION;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 modelPosition = float4(input.modelPositionColorIndex.xyz, 1.0f);
#if GTE_USE_MAT_VEC
    output.clipPosition = mul(pvwMatrix, modelPosition);
#else
    output.clipPosition = mul(modelPosition, pvwMatrix);
#endif
    uint i = uint(input.modelPositionColorIndex.w);
    output.vertexColor = color[i];
    return output;
}
