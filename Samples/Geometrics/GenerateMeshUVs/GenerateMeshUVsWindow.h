// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2019/04/13)

#pragma once

#include <Applications/GteWindow3.h>
using namespace gte;

class GenerateMeshUVsWindow : public Window3
{
public:
    GenerateMeshUVsWindow(Parameters& parameters);

    virtual void OnIdle() override;
    virtual bool OnCharPress(unsigned char key, int x, int y) override;

private:
    bool SetEnvironment();
    void CreateScene();
    void CreateMeshOriginal();

    // The texture is applied differently for the resampled mesh, because
    // the original has texture coordinates generated by the mesh factory
    // (based on polar coordinates).  The resampled mesh has texture
    // coordinates generated by barycentric mapping.  The orientation of
    // the texture depends on how the mesh boundary is mapped to the
    // uv-square.
    void CreateMeshResampled();

    struct Vertex
    {
        Vector3<float> position;
        Vector2<float> tcoord;
    };

    std::shared_ptr<Visual> mMeshOriginal, mMeshResampled;
    std::shared_ptr<RasterizerState> mNoCullState;
    std::shared_ptr<RasterizerState> mNoCullWireState;
    bool mDrawMeshOriginal;
};
