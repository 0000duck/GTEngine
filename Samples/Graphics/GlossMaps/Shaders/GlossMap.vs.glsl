// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.25.0 (2019/04/16)

uniform PVWMatrix
{
    mat4 pvwMatrix;
};

uniform Material
{
    vec4 materialEmissive;
    vec4 materialAmbient;
    vec4 materialDiffuse;
    vec4 materialSpecular;
};

uniform Lighting
{
    vec4 lightingAmbient;
    vec4 lightingDiffuse;
    vec4 lightingSpecular;
    vec4 lightingAttenuation;
};

uniform LightCameraGeometry
{
    vec4 lightModelDirection;
    vec4 cameraModelPosition;
};

layout(location = 0) in vec3 modelPosition;
layout(location = 1) in vec3 modelNormal;
layout(location = 2) in vec2 modelTCoord;

layout(location = 0) out vec3 emsAmbDifColor;
layout(location = 1) out vec3 spcColor;
layout(location = 2) out vec2 vertexTCoord;

void main()
{
    float NDotL = -dot(modelNormal, lightModelDirection.xyz);
    vec3 viewVector = normalize(cameraModelPosition.xyz - modelPosition);
    vec3 halfVector = normalize(viewVector - lightModelDirection.xyz);
    float NDotH = dot(modelNormal, halfVector);
    vec4 lighting = lit(NDotL, NDotH, materialSpecular.a);

    emsAmbDifColor = materialEmissive.rgb +
        materialAmbient.rgb * lightingAmbient.rgb +
        lighting.y * materialDiffuse.rgb * lightingDiffuse.rgb;

    spcColor = lighting.z * materialSpecular.rgb * lightingSpecular.rgb;

    vertexTCoord = modelTCoord;

#if GTE_USE_MAT_VEC
    gl_Position = pvwMatrix * vec4(modelPosition, 1.0f);
#else
    gl_Position = vec4(modelPosition, 1.0f) * pvwMatrix;
#endif
}