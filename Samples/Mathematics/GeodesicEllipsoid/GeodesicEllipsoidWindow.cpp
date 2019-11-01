// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.23.1 (2019/05/02)

#include "GeodesicEllipsoidWindow.h"
#include <LowLevel/GteLogReporter.h>
#include <random>

int main(int, char const*[])
{
#if defined(_DEBUG)
    LogReporter reporter(
        "LogReport.txt",
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL);
#endif

    Window::Parameters parameters(L"GeodesicEllipsoidWindow", 0, 0, 512, 512);
    auto window = TheWindowSystem.Create<GeodesicEllipsoidWindow>(parameters);
    TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
    TheWindowSystem.Destroy<GeodesicEllipsoidWindow>(window);
    return 0;
}

GeodesicEllipsoidWindow::GeodesicEllipsoidWindow(Parameters& parameters)
    :
    Window2(parameters),
    mSize(mXSize),
    mGeodesic(1.0f, 1.0f, 1.0f),
    mParam0(2),
    mParam1(2),
    mXMin(0.0f),
    mXMax(static_cast<float>(GTE_C_HALF_PI)),
    mXDelta((mXMax - mXMin) / static_cast<float>(mSize)),
    mYMin(static_cast<float>(GTE_C_HALF_PI) / static_cast<float>(mSize)),
    mYMax(static_cast<float>(GTE_C_HALF_PI)),
    mYDelta((mYMax - mYMin) / static_cast<float>(mSize)),
    mNumTruePoints(129),
    mTruePoints(mNumTruePoints),
    mNumApprPoints((1 << mGeodesic.subdivisions) + 1),
    mApprPoints(mNumApprPoints),
    mCurrNumApprPoints(0),
    mTrueDistance(0.0f),
    mApprDistance(0.0f),
    mApprCurvature(0.0f)
{
    mTextColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    for (int i = 0; i < mNumTruePoints; ++i)
    {
        mTruePoints[i].SetSize(2);
    }

    for (int i = 0; i < mNumApprPoints; ++i)
    {
        mApprPoints[i].SetSize(2);
    }

    mGeodesic.refineCallback = [this]()
    {
        ComputeApprLength();
        OnDisplay();
    };

    ComputeTruePath();
    mDoFlip = true;
    OnDisplay();
}

void GeodesicEllipsoidWindow::OnDisplay()
{
    unsigned int const white = 0xFFFFFFFF;
    unsigned int const red = 0xFF0000FF;
    unsigned int const green = 0xFF00FF00;

    ClearScreen(white);

    // Draw the true path.
    int x0, y0, x1, y1;
    ParamToXY(mTruePoints[0], x0, y0);
    for (int i = 1; i < mNumTruePoints; ++i)
    {
        ParamToXY(mTruePoints[i], x1, y1);
        DrawLine(x0, y0, x1, y1, green);
        x0 = x1;
        y0 = y1;
    }

    // Draw the approximate path.
    int numApprPoints = mGeodesic.GetCurrentQuantity();
    if (numApprPoints == 0)
    {
        numApprPoints = mCurrNumApprPoints;
    }

    ParamToXY(mApprPoints[0], x0, y0);
    for (int i = 1; i < numApprPoints; ++i)
    {
        ParamToXY(mApprPoints[i], x1, y1);
        DrawLine(x0, y0, x1, y1, red);
        x0 = x1;
        y0 = y1;
    }

    mScreenTextureNeedsUpdate = true;
    Window2::OnDisplay();
}

void GeodesicEllipsoidWindow::DrawScreenOverlay()
{
    std::string message = "true dist = " + std::to_string(mTrueDistance) +
        ", appr dist = " + std::to_string(mApprDistance) +
        ", appr curv = " + std::to_string(mApprCurvature);

    mEngine->Draw(8, 16, mTextColor, message);

    int substep = mGeodesic.GetSubdivisionStep();
    int refstep = mGeodesic.GetRefinementStep();
    int currquan = mGeodesic.GetCurrentQuantity();
    message = "sub = " + std::to_string(substep) +
        ", ref = " + std::to_string(refstep) +
        ", currquan = " + std::to_string(currquan);

    mEngine->Draw(8, 32, mTextColor, message);
}

bool GeodesicEllipsoidWindow::OnCharPress(unsigned char key, int x, int y)
{
    switch (key)
    {
    case '0':
        ComputeTruePath();
        OnDisplay();
        return true;
    case '1':
        ComputeApprPath(true);
        OnDisplay();
        return true;
    case '2':
        ComputeApprPath(false);
        OnDisplay();
        return true;
    case '3':
        mGeodesic.ComputeGeodesic(mParam0, mParam1, mCurrNumApprPoints, mApprPoints);
        ComputeApprLength();
        OnDisplay();
        return true;
    }

    return Window2::OnCharPress(key, x, y);
}

void GeodesicEllipsoidWindow::ComputeTruePath()
{
    // Random selection of endpoints.  The angles are (theta,phi) with
    // 0 <= theta < 2*pi and 0 <= phi < pi/2, thus placing the points on the
    // the first octant of the ellipsoid.
    std::default_random_engine dre;
    std::uniform_real_distribution<float> rnd(0.0f, static_cast<float>(GTE_C_HALF_PI));
    mParam0[0] = rnd(dre);
    mParam0[1] = rnd(dre);
    mParam1[0] = rnd(dre);
    mParam1[1] = rnd(dre);

    // Compute the true geodesic path.
    Vector3<float> pos0 = mGeodesic.ComputePosition(mParam0);
    Vector3<float> pos1 = mGeodesic.ComputePosition(mParam1);
    float angle = std::acos(Dot(pos0, pos1));
    float divisor = static_cast<float>(mNumTruePoints - 1);
    for (int i = 0; i < mNumTruePoints; ++i)
    {
        float t = i / divisor;
        float sn0 = std::sin((1.0f - t) * angle);
        float sn1 = std::sin(t * angle);
        float sn = std::sin(angle);
        Vector3<float> pos = (sn0 * pos0 + sn1 * pos1) / sn;
        mTruePoints[i][0] = std::atan2(pos[1], pos[0]);
        mTruePoints[i][1] = std::acos(pos[2]);
    }

    // Compute the true length of the geodesic path.
    mTrueDistance = angle;

    // Initialize the approximate path.
    mCurrNumApprPoints = 2;
    mApprPoints[0] = mParam0;
    mApprPoints[1] = mParam1;
    ComputeApprLength();
}

void GeodesicEllipsoidWindow::ComputeApprPath(bool subdivide)
{
    if (subdivide)
    {
        int newNumApprPoints = 2 * mCurrNumApprPoints - 1;
        if (newNumApprPoints > mNumApprPoints)
        {
            return;
        }

        // Copy the old points so that there are slots for the midpoints
        // during the subdivision interleaved between the old points.
        for (int i = mCurrNumApprPoints - 1; i > 0; --i)
        {
            mApprPoints[2 * i] = mApprPoints[i];
        }

        for (int i = 0; i <= mCurrNumApprPoints - 2; ++i)
        {
            mGeodesic.Subdivide(mApprPoints[2 * i], mApprPoints[2 * i + 1],
                mApprPoints[2 * i + 2]);
        }

        mCurrNumApprPoints = newNumApprPoints;
    }
    else // refine
    {
        for (int i = 1; i <= mCurrNumApprPoints - 2; ++i)
        {
            mGeodesic.Refine(mApprPoints[i - 1], mApprPoints[i],
                mApprPoints[i + 1]);
        }
    }

    ComputeApprLength();
}

void GeodesicEllipsoidWindow::ComputeApprLength()
{
    int numApprPoints = mGeodesic.GetCurrentQuantity();
    if (numApprPoints == 0)
    {
        numApprPoints = mCurrNumApprPoints;
    }

    mApprDistance = mGeodesic.ComputeTotalLength(numApprPoints, mApprPoints);
    mApprCurvature = mGeodesic.ComputeTotalCurvature(numApprPoints, mApprPoints);
}

void GeodesicEllipsoidWindow::ParamToXY(GVector<float> const& param, int& x, int& y)
{
    // Only the first octant of the ellipsoid is used.
    x = static_cast<int>((param[0] - mXMin) / mXDelta + 0.5f);
    y = static_cast<int>((param[1] - mYMin) / mYDelta + 0.5f);
}

void GeodesicEllipsoidWindow::XYToParam(int x, int y, GVector<float>& param)
{
    param[0] = mXMin + x * mXDelta;
    param[1] = mYMin + y * mYDelta;
}
