// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.16.1 (2019/05/02)
#pragma once

#include <Applications/GteWindow3.h>
#include <Mathematics/GteIntrAlignedBox3Sphere3.h>
#include <Mathematics/GteIntrOrientedBox3Sphere3.h>
using namespace gte;

// The default is to test the query for aligned box and sphere.
// Uncomment this to test the query for oriented box and sphere.
//#define APP_USE_OBB

class MovingSphereBoxWindow : public Window3
{
public:
    MovingSphereBoxWindow(Parameters& parameters);

    virtual void OnIdle() override;
    virtual bool OnCharPress(unsigned char key, int x, int y) override;

private:
    enum { DENSITY = 32 };
    void CreateScene();
    void CreateRoundedBoxVertices();
    void CreateRoundedBoxEdges();
    void CreateRoundedBoxFaces();
    void CreateBox();
    void CreateSpheres();
    void CreateMotionCylinder();
    void UpdateSphereVelocity();
    void UpdateSphereCenter();

    std::shared_ptr<BlendState> mBlendState;
    std::shared_ptr<RasterizerState> mNoCullState;
    float mAlpha;

    // Octants of spheres for the rounded box corners.
    std::array<std::shared_ptr<Visual>, 8> mVertexVisual;
    std::array<Vector3<float>, 8> mVNormal;

    // Quarter cylinders for the rounded box edges.
    std::array<std::shared_ptr<Visual>, 12> mEdgeVisual;
    std::array<Vector3<float>, 12> mENormal;

    // Rectangles for the rounded box faces.
    std::array<std::shared_ptr<Visual>, 6> mFaceVisual;
    std::array<Vector3<float>, 6> mFNormal;

    // The visual representation of mBox.
    std::shared_ptr<Visual> mBoxVisual;

    // The scene graph that represents the box and features.
    std::shared_ptr<Node> mBoxRoot;

    // The visual representation of mSphere.
    std::shared_ptr<Visual> mSphereVisual;
    std::shared_ptr<Visual> mSphereContactVisual;

    // The visual representation of the moving path of the sphere.
    std::shared_ptr<Visual> mVelocityVisual;

    // The contact point representation.
    std::shared_ptr<Visual> mPointContactVisual;

#if defined(APP_USE_OBB)
    OrientedBox3<float> mBox;
    FIQuery<float, OrientedBox3<float>, Sphere3<float>> mQuery;
#else
    AlignedBox3<float> mBox;
    FIQuery<float, AlignedBox3<float>, Sphere3<float>> mQuery;
#endif
    Sphere3<float> mSphere;
    Vector3<float> mBoxVelocity;
    Vector3<float> mSphereVelocity;
    int mNumSamples0, mNumSamples1, mSample0, mSample1;
    float mDX, mDY, mDZ;
    std::string mMessage;
    bool mDrawSphereVisual;
};
