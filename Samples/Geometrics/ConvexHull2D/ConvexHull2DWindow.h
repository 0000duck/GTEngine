// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.1 (2019/04/13)

#pragma once

#include <Applications/GteWindow2.h>
#include <Mathematics/GteArbitraryPrecision.h>
#include <Mathematics/GteConvexHull2.h>
using namespace gte;

class ConvexHull2DWindow : public Window2
{
public:
    ConvexHull2DWindow(Parameters& parameters);

    virtual void OnDisplay() override;

private:
    std::vector<Vector2<float>> mVertices;
    std::vector<int> mHull;
    ConvexHull2<float, BSNumber<UIntegerAP32>> mConvexHull;
};
