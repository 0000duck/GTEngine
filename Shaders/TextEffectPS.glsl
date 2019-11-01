// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

uniform TextColor
{
    vec4 textColor;
};

layout(location = 0) in vec2 vertexTCoord;
layout(location = 0) out vec4 pixelColor;

uniform sampler2D baseSampler;

void main()
{
    float bitmapAlpha = texture(baseSampler, vertexTCoord).r;
    if (bitmapAlpha > 0.5f)
    {
        discard;
    }
    pixelColor = textColor;
}
