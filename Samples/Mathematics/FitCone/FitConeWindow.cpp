// Geometric Tools LLC, Redmond WA 98052
// Copyright (c) 1998-2018
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.18.1 (2019/05/02)

#include "FitConeWindow.h"
#include <LowLevel/GteLogReporter.h>
#include <Graphics/GteMeshFactory.h>
#include <Graphics/GteConstantColorEffect.h>
#include <Mathematics/GteApprCone3.h>
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

    Window::Parameters parameters(L"FitConeWindow", 0, 0, 512, 512);
    auto window = TheWindowSystem.Create<FitConeWindow>(parameters);
    TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
    TheWindowSystem.Destroy<FitConeWindow>(window);
    return 0;
}

FitConeWindow::FitConeWindow(Parameters& parameters)
    :
    Window3(parameters)
{
    mTextColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    mNoCullSolidState = std::make_shared<RasterizerState>();
    mNoCullSolidState->cullMode = RasterizerState::CULL_NONE;
    mNoCullSolidState->fillMode = RasterizerState::FILL_SOLID;
    mNoCullWireState = std::make_shared<RasterizerState>();
    mNoCullWireState->cullMode = RasterizerState::CULL_NONE;
    mNoCullWireState->fillMode = RasterizerState::FILL_WIREFRAME;
    mEngine->SetRasterizerState(mNoCullSolidState);

    mBlendState = std::make_shared<BlendState>();
    mBlendState->target[0].enable = true;
    mBlendState->target[0].srcColor = BlendState::BM_SRC_ALPHA;
    mBlendState->target[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
    mBlendState->target[0].srcAlpha = BlendState::BM_SRC_ALPHA;
    mBlendState->target[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    CreateScene();

    InitializeCamera(60.0f, GetAspectRatio(), 0.01f, 100.0f, 0.005f, 0.002f,
        { -6.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });

    mTrackball.Update();
    mPVWMatrices.Update();
}

void FitConeWindow::OnIdle()
{
    mTimer.Measure();

    if (mCameraRig.Move())
    {
        mPVWMatrices.Update();
    }

    mEngine->ClearBuffers();

    mEngine->Draw(mPoints);

    mEngine->SetBlendState(mBlendState);
    if (mGNCone->culling == CullingMode::CULL_NEVER)
    {
        mEngine->Draw(mGNCone);
    }
    if (mLMCone->culling == CullingMode::CULL_NEVER)
    {
        mEngine->Draw(mLMCone);
    }
    mEngine->SetDefaultBlendState();

    mEngine->Draw(8, 24, mTextColor, "key '0' toggles GN-generated mesh");
    mEngine->Draw(8, 48, mTextColor, "key '1' toggles LM-generated mesh");
    mEngine->Draw(8, 72, mTextColor, "key 'w' toggles wireframe");
    mEngine->Draw(8, mYSize - 8, mTextColor, mTimer.GetFPS());
    mEngine->DisplayColorBuffer(0);

    mTimer.UpdateFrameCount();
}

bool FitConeWindow::OnCharPress(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'w':
    case 'W':
        if (mEngine->GetRasterizerState() == mNoCullSolidState)
        {
            mEngine->SetRasterizerState(mNoCullWireState);
        }
        else
        {
            mEngine->SetRasterizerState(mNoCullSolidState);
        }
        return true;
    case '0':
        if (mGNCone->culling == CullingMode::CULL_NEVER)
        {
            mGNCone->culling = CullingMode::CULL_ALWAYS;
        }
        else
        {
            mGNCone->culling = CullingMode::CULL_NEVER;
        }
        return true;
    case '1':
        if (mLMCone->culling == CullingMode::CULL_NEVER)
        {
            mLMCone->culling = CullingMode::CULL_ALWAYS;
        }
        else
        {
            mLMCone->culling = CullingMode::CULL_NEVER;
        }
        return true;
    }
    return Window3::OnCharPress(key, x, y);
}

void FitConeWindow::CreateScene()
{
    std::default_random_engine dre;
    std::uniform_real_distribution<double> rnd(-1.0, 1.0);
    double const epsilon = 0.01;

    Vector3<double> V = { 3.0, 2.0, 1.0 };
    Vector3<double> U = { 1.0, 2.0, 3.0 };
    Vector3<double> basis[3];
    basis[0] = U;
    ComputeOrthogonalComplement(1, basis);
    U = basis[0];
    Vector3<double> W0 = basis[1];
    Vector3<double> W1 = basis[2];
    double H0 = 1.0;
    double H1 = 2.0;
    double theta = GTE_C_PI / 4.0;
    double tantheta = std::tan(theta);

    unsigned int const numPoints = 8196;
    std::vector<Vector3<double>> X(numPoints);
    for (unsigned int i = 0; i < numPoints; ++i)
    {
        double unit = 0.5 * (rnd(dre) + 1.0); // in [0,1)
        double h = H0 + (H1 - H0) * unit;
        double perturb = 1.0 + epsilon * rnd(dre);  // in [1-e,1+e)
        double r = perturb * (h * tantheta);
        double symm = rnd(dre);  // in [-1,1)
        double phi = GTE_C_PI * symm;
        double csphi = std::cos(phi);
        double snphi = std::sin(phi);
        X[i] = V + h * U + r * (csphi * W0 + snphi * W1);
    }

    CreatePoints(X);

    Vector3<double> coneVertex;
    Vector3<double> coneAxis;
    double coneAngle;

    CreateGNCone(X, coneVertex, coneAxis, coneAngle);
    Vector4<float> green{ 0.0f, 1.0f, 0.0f, 0.25f };
    mGNCone = CreateConeMesh(X, coneVertex, coneAxis, coneAngle, green);

    CreateLMCone(X, coneVertex, coneAxis, coneAngle);
    Vector4<float> blue{ 0.0f, 0.0f, 1.0f, 0.25f };
    mLMCone = CreateConeMesh(X, coneVertex, coneAxis, coneAngle, blue);
}

void FitConeWindow::CreatePoints(std::vector<Vector3<double>> const& X)
{
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    unsigned int numVertices = static_cast<unsigned int>(X.size());
    auto vbuffer = std::make_shared<VertexBuffer>(vformat, numVertices);
    auto vertices = vbuffer->Get<Vector3<float>>();
    mCenter = { 0.0f, 0.0f, 0.0f };
    for (unsigned int i = 0; i < numVertices; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            vertices[i][j] = static_cast<float>(X[i][j]);
        }
        mCenter += vertices[i];
    }
    mCenter /= static_cast<float>(numVertices);

    auto ibuffer = std::make_shared<IndexBuffer>(IP_POLYPOINT, numVertices);

    Vector4<float> black{ 0.0f, 0.0f, 0.0f, 1.0f };
    auto effect = std::make_shared<ConstantColorEffect>(mProgramFactory, black);

    mPoints = std::make_shared<Visual>(vbuffer, ibuffer, effect);
    mPoints->localTransform.SetTranslation(-mCenter);

    mPVWMatrices.Subscribe(mPoints->worldTransform, effect->GetPVWMatrixConstant());
    mTrackball.Attach(mPoints);
}

void FitConeWindow::CreateGNCone(std::vector<Vector3<double>> const& X,
    Vector3<double>& coneVertex, Vector3<double>& coneAxis, double& coneAngle)
{
    ApprCone3<double> fitter;
    size_t const maxIterations = 32;
    double const updateLengthTolerance = 1e-04;
    double const errorDifferenceTolerance = 1e-08;
    bool useConeInputAsInitialGuess = false;

    fitter(static_cast<int>(X.size()), X.data(),
        maxIterations, updateLengthTolerance, errorDifferenceTolerance,
        useConeInputAsInitialGuess, coneVertex, coneAxis, coneAngle);
}

void FitConeWindow::CreateLMCone(std::vector<Vector3<double>> const& X,
    Vector3<double>& coneVertex, Vector3<double>& coneAxis, double& coneAngle)
{
    ApprCone3<double> fitter;
    size_t const maxIterations = 32;
    double const updateLengthTolerance = 1e-04;
    double const errorDifferenceTolerance = 1e-08;
    double const lambdaFactor = 0.001;
    double const lambdaAdjust = 10.0;
    size_t const maxAdjustments = 8;
    bool useConeInputAsInitialGuess = false;

    fitter(static_cast<int>(X.size()), X.data(),
        maxIterations, updateLengthTolerance, errorDifferenceTolerance,
        lambdaFactor, lambdaAdjust, maxAdjustments, useConeInputAsInitialGuess,
        coneVertex, coneAxis, coneAngle);
}

std::shared_ptr<Visual> FitConeWindow::CreateConeMesh(std::vector<Vector3<double>> const& X,
    Vector3<double> const& coneVertex, Vector3<double> const& coneAxis, double coneAngle,
    Vector4<float> const& color)
{
    // Compute the cone height extremes.
    double hmin = std::numeric_limits<double>::max();
    double hmax = 0.0;
    for (size_t i = 0; i < X.size(); ++i)
    {
        Vector3<double> diff = X[i] - coneVertex;
        double h = Dot(coneAxis, diff);
        if (h > hmax)
        {
            hmax = h;
        }
        if (h < hmin)
        {
            hmin = h;
        }
    }

    // Compute the tangent of the cone angle.
    double tanTheta = std::tan(coneAngle);

    // Compute a right-handed basis from the cone axis direction;
    Vector3<double> basis[3];
    basis[0] = coneAxis;
    ComputeOrthogonalComplement(1, basis);
    Vector3<double> W0 = basis[1];
    Vector3<double> W1 = basis[2];

    // Create a cone frustum mesh.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    MeshFactory mf;
    mf.SetVertexFormat(vformat);

    unsigned int const numXSamples = 16;
    unsigned int const numYSamples = 16;
    auto cone = mf.CreateRectangle(numXSamples, numYSamples, 1.0f, 1.0f);
    cone->localTransform.SetTranslation(-mCenter);
    cone->culling = CullingMode::CULL_ALWAYS;

    auto vertices = cone->GetVertexBuffer()->Get<Vector3<float>>();
    double const xMult = GTE_C_TWO_PI / static_cast<double>(numYSamples - 1);
    double const YMult = (hmax - hmin) / static_cast<double>(numXSamples - 1);
    for (unsigned int y = 0, i = 0; y < numYSamples; ++y)
    {
        double h = hmin + static_cast<double>(y) * YMult;
        double r = h * tanTheta;
        for (unsigned int x = 0; x < numXSamples; ++x, ++i)
        {
            double phi = static_cast<double>(x) * xMult;
            double rcs = r * std::cos(phi);
            double rsn = r * std::sin(phi);
            Vector3<double> P = coneVertex + h * coneAxis + rcs * W0 + rsn * W1;
            for (int j = 0; j < 3; ++j)
            {
                vertices[i][j] = static_cast<float>(P[j]);
            }
        }
    }

    auto effect = std::make_shared<ConstantColorEffect>(mProgramFactory, color);
    cone->SetEffect(effect);

    mPVWMatrices.Subscribe(cone->worldTransform, effect->GetPVWMatrixConstant());
    mTrackball.Attach(cone);

    return cone;
}
