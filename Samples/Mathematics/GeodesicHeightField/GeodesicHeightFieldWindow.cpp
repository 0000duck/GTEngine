// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.23.2 (2019/05/03)

#include "GeodesicHeightFieldWindow.h"
#include <LowLevel/GteLogReporter.h>
#include <Imagics/GteImageUtility2.h>
#include <Graphics/GteMeshFactory.h>
#include <Graphics/GteDirectionalLightTextureEffect.h>
#include <sstream>
#include <iomanip>

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

    Window::Parameters parameters(L"GeodesicHeightFieldWindow", 0, 0, 1024, 768);
    auto window = TheWindowSystem.Create<GeodesicHeightFieldWindow>(parameters);
    TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
    TheWindowSystem.Destroy<GeodesicHeightFieldWindow>(window);
    return 0;
}

GeodesicHeightFieldWindow::GeodesicHeightFieldWindow(Parameters& parameters)
    :
    Window3(parameters),
    mSelected(0),
    mPathQuantity(0),
    mDistance(1.0),
    mCurvature(0.0)
{
    if (!SetEnvironment())
    {
        parameters.created = false;
        return;
    }

    mLightWorldDirection = { 0.0f, 0.0f, 0.0f, 0.0f };
    mXIntr = { 0, 0 };
    mYIntr = { 0, 0 };
    mPoint[0].SetSize(2);
    mPoint[1].SetSize(2);
    mTextColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    mEngine->SetClearColor({ 0.9f, 0.9f, 0.9f, 1.0f });

    mNoCullState = std::make_shared<RasterizerState>();
    mNoCullState->cullMode = RasterizerState::CULL_NONE;
    mNoCullWireState = std::make_shared<RasterizerState>();
    mNoCullWireState->cullMode = RasterizerState::CULL_NONE;
    mNoCullWireState->fillMode = RasterizerState::FILL_WIREFRAME;
    mEngine->SetRasterizerState(mNoCullState);

    InitializeCamera(60.0f, GetAspectRatio(), 0.1f, 100.0f, 0.01f, 0.01f,
        { 0.0f, -4.0f, 0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });

    CreateScene();

    mTrackball.Update();
    mPVWMatrices.Update();

    // Set the callback for drawing the geodesic curve in a texture that
    // is associated with the mesh of the B-spline height field.
    mDrawCallback = [this](int x, int y)
    {
        int bound0 = mTexture->GetDimensionFor(0, 0);
        auto texels = mTexture->GetFor<uint8_t>(0);
        int index = 4 * (x + bound0 * y);
        texels[index++] = 0x00;
        texels[index++] = 0x00;
        texels[index++] = 0x00;
        texels[index++] = 0xFF;
    };

    // Create the geodesic calculator.
    mGeodesic = std::make_unique<BSplineGeodesic<double>>(*mSurface);
    mGeodesic->subdivisions = 6;
    mGeodesic->refinements = 1;
    mGeodesic->searchRadius = 0.1;
    mGeodesic->refineCallback = [this]()
    {
        int currQuantity = mGeodesic->GetCurrentQuantity();
        if (currQuantity == 0)
        {
            currQuantity = mPathQuantity;
        }

        mDistance = mGeodesic->ComputeTotalLength(currQuantity, mPath);
        mCurvature = mGeodesic->ComputeTotalCurvature(currQuantity, mPath);

        OnIdle();
    };

    mPathQuantity = (1 << mGeodesic->subdivisions) + 1;
}

void GeodesicHeightFieldWindow::OnIdle()
{
    mTimer.Measure();

    if (mCameraRig.Move())
    {
        mPVWMatrices.Update();
    }

    if (mSelected == 2)
    {
        int currQuantity = mGeodesic->GetCurrentQuantity();
        if (currQuantity == 0)
        {
            currQuantity = mPathQuantity;
        }

        // Clear the base-level mipmap of the texture image to white.
        std::memset(mTexture->GetDataFor(0), 0xFF, mTexture->GetNumBytesFor(0));

        // Draw the approximate path.
        int bound0 = mTexture->GetDimensionFor(0, 0);
        int bound1 = mTexture->GetDimensionFor(0, 1);
        int x0 = static_cast<int>(bound0 * mPath[0][0] + 0.5);
        int y0 = static_cast<int>(bound1 * mPath[0][1] + 0.5);
        for (int i = 1; i < currQuantity; ++i)
        {
            int x1 = (int)(bound0*mPath[i][0] + 0.5);
            int y1 = (int)(bound1*mPath[i][1] + 0.5);
            ImageUtility2::DrawLine(x0, y0, x1, y1, mDrawCallback);
            x0 = x1;
            y0 = y1;
        }

        // The mipmap levels will be automatically computed.
        mEngine->CopyCpuToGpu(mTexture);
    }

    mEngine->ClearBuffers();
    mEngine->Draw(mMesh);

    int substep = mGeodesic->GetSubdivisionStep();
    int refstep = mGeodesic->GetRefinementStep();
    std::ostringstream oss;
    oss << std::setprecision(12);
    oss << "sub = " << substep << ", ";
    oss << "ref = " << refstep << ", ";
    oss << "len = " << mDistance << ", ";
    oss << "avgcrv = " << mCurvature / mDistance;

    mEngine->Draw(8, 24, mTextColor, oss.str());
    mEngine->Draw(8, mYSize - 8, mTextColor, mTimer.GetFPS());

    mEngine->DisplayColorBuffer(0);

    mTimer.UpdateFrameCount();
}

bool GeodesicHeightFieldWindow::OnCharPress(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'w':
    case 'W':
        if (mEngine->GetRasterizerState() == mNoCullState)
        {
            mEngine->SetRasterizerState(mNoCullWireState);
        }
        else
        {
            mEngine->SetRasterizerState(mNoCullState);
        }
        return true;
    }
    return Window3::OnCharPress(key, x, y);
}

bool GeodesicHeightFieldWindow::OnMouseClick(MouseButton button, MouseState state,
    int x, int y, unsigned int modifiers)
{
    if ((modifiers & MODIFIER_SHIFT) == 0)
    {
        return Window3::OnMouseClick(button, state, x, y, modifiers);
    }

    if (state != MOUSE_DOWN || button != MOUSE_LEFT)
    {
        return false;
    }

    // Convert to right-handed coordinates.
    y = mYSize - 1 - y;

    // Do a picking operation.
    int viewX, viewY, viewW, viewH;
    mEngine->GetViewport(viewX, viewY, viewW, viewH);
    Vector4<float> origin, direction;
    if (mCamera->GetPickLine(viewX, viewY, viewW, viewH, x, y, origin, direction))
    {
        mPicker(mMesh, origin, direction, 0.0f, std::numeric_limits<float>::max());
        if (mPicker.records.size() > 0)
        {
            const PickRecord& record = mPicker.GetClosestNonnegative();

            // Get the vertex indices for the picked triangle.
            int i0 = 3 * record.primitiveIndex;
            int i1 = i0 + 1;
            int i2 = i0 + 2;
            auto indices = mMesh->GetIndexBuffer()->Get<int>();
            int v0 = indices[i0];
            int v1 = indices[i1];
            int v2 = indices[i2];

            // Get the texture coordinates for the point of intersection.
            auto vbuffer = mMesh->GetVertexBuffer();
            auto vertices = vbuffer->Get<Vertex>();
            Vector2<float> tcoordIntr =
                vertices[v0].tcoord * record.bary[0] +
                vertices[v1].tcoord * record.bary[1] +
                vertices[v2].tcoord * record.bary[2];

            // Save the point.
            mPoint[mSelected][0] = static_cast<double>(tcoordIntr[0]);
            mPoint[mSelected][1] = static_cast<double>(tcoordIntr[1]);

            // Clear the texture image to white.
            auto texels = mTexture->GetFor<uint8_t>(0);
            std::memset(texels, 0xFF, mTexture->GetNumBytesFor(0));

            // Get an endpoint.
            int bound0 = mTexture->GetDimensionFor(0, 0);
            int bound1 = mTexture->GetDimensionFor(0, 1);
            int tx = static_cast<int>(bound0 * tcoordIntr[0] + 0.5f);
            int ty = static_cast<int>(bound1 * tcoordIntr[1] + 0.5f);
            mXIntr[mSelected] = tx;
            mYIntr[mSelected] = ty;
            ++mSelected;

            // Mark the endpoints in black.
            for (int i = 0; i < mSelected; ++i)
            {
                int index = 4 * (mXIntr[i] + bound0 * mYIntr[i]);
                texels[index++] = 0x00;
                texels[index++] = 0x00;
                texels[index++] = 0x00;
                texels[index++] = 0xFF;
            }

            // The mipmap levels will be automatically computed.
            mEngine->CopyCpuToGpu(mTexture);

            if (mSelected == 2)
            {
                mGeodesic->ComputeGeodesic(mPoint[0], mPoint[1], mPathQuantity, mPath);
                mSelected = 0;
            }
        }
    }

    return true;
}

bool GeodesicHeightFieldWindow::SetEnvironment()
{
    std::string path = GetGTEPath();
    if (path == "")
    {
        return false;
    }

    mEnvironment.Insert(path + "/Samples/Mathematics/GeodesicHeightField/Data");

    if (mEnvironment.GetPath("ControlPoints.txt") == "")
    {
        LogError("Cannot find file ControlPoints.txt");
        return false;
    }
    return true;
}

void GeodesicHeightFieldWindow::CreateScene()
{
    // Create the ground.  It covers a square with vertices (1,1,0), (1,-1,0),
    // (-1,1,0), and (-1,-1,0).
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_NORMAL, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_TEXCOORD, DF_R32G32_FLOAT, 0);

    MeshFactory mf;
    mf.SetVertexFormat(vformat);
    int const numXSamples = 64, numYSamples = 64;
    float const xExtent = 1.0f, yExtent = 1.0f;
    mMesh = mf.CreateRectangle(numXSamples, numYSamples, xExtent, yExtent);

    // Create a B-Spline height field.  The heights of the control point are
    // defined in an input file.  The input file is structured as
    //
    // numUCtrlPoints numVCtrlPoints UDegree VDegree
    // z[0][0] z[0][1] ... z[0][numV-1]
    // z[1][0] z[1][1] ... z[1][numV_1]
    // :
    // z[numU-1][0] z[numU-1][1] ... z[numU-1][numV-1]
    std::string path = mEnvironment.GetPath("ControlPoints.txt");
    std::ifstream inFile(path.c_str());
    int numControls[2], degree[2];
    inFile >> numControls[0];
    inFile >> numControls[1];
    inFile >> degree[0];
    inFile >> degree[1];

    BasisFunctionInput<double> input[2];
    for (int dim = 0; dim < 2; ++dim)
    {
        input[dim].numControls = numControls[dim];
        input[dim].degree = degree[dim];
        input[dim].uniform = true;
        input[dim].periodic = false;
        input[dim].numUniqueKnots = numControls[dim] - degree[dim] + 1;
        input[dim].uniqueKnots.resize(input[dim].numUniqueKnots);
        input[dim].uniqueKnots[0].t = 0.0;
        input[dim].uniqueKnots[0].multiplicity = degree[dim] + 1;
        int last = input[dim].numUniqueKnots - 1;
        double factor = 1.0 / static_cast<double>(last);
        for (int i = 1; i < last; ++i)
        {
            input[dim].uniqueKnots[i].t = factor * static_cast<double>(i);
            input[dim].uniqueKnots[i].multiplicity = 1;
        }
        input[dim].uniqueKnots[last].t = 1.0;
        input[dim].uniqueKnots[last].multiplicity = degree[dim] + 1;
    }

    mSurface = std::make_unique<BSplineSurface<3, double>>(input, nullptr);
    auto controls = mSurface->GetControls();

    for (int i = 0; i < numControls[0]; ++i)
    {
        double u = xExtent * (-1.0 + 2.0 * i / (numControls[0] - 1));
        for (int j = 0; j < numControls[1]; ++j)
        {
            double v = yExtent * (-1.0 + 2.0 * j / (numControls[1] - 1));
            double height;
            inFile >> height;
            controls[i + numControls[0] * j] = { u, v, height };
        }
    }
    inFile.close();

    auto vbuffer = mMesh->GetVertexBuffer();
    unsigned int numVertices = vbuffer->GetNumElements();
    auto vertices = vbuffer->Get<Vertex>();
    for (unsigned int i = 0; i < numVertices; ++i)
    {
        auto& position = vertices[i].position;
        double u = (position[0] + xExtent) / (2.0 * xExtent);
        double v = (position[1] + yExtent) / (2.0 * yExtent);
        Vector<3, double> jet[6];
        mSurface->Evaluate(u, v, 0, jet);
        position[2] = static_cast<float>(jet[0][2]);
    }
    mMesh->UpdateModelBound();
    mMesh->UpdateModelNormals();

    // Attach an effect that uses lights, material, and texture.
    // The other material members use their default values.
    auto material = std::make_shared<Material>();
    material->ambient = { 0.24725f, 0.2245f, 0.0645f, 1.0f };
    material->diffuse = { 0.34615f, 0.3143f, 0.0903f, 1.0f };
    material->specular = { 0.797357f, 0.723991f, 0.208006f, 83.2f };

    // The other lighting members use their default values.
    auto lighting = std::make_shared<Lighting>();
    lighting->specular = { 0.0f, 0.0f, 0.0f, 1.0f };

    // The light shines down onto the height field.
    auto geometry = std::make_shared<LightCameraGeometry>();
    mLightWorldDirection = { 0.0f, 0.0f, -1.0f, 0.0f };

    mTexture = std::make_shared<Texture2>(DF_R8G8B8A8_UNORM, 512, 512, true);
    mTexture->AutogenerateMipmaps();
    mTexture->SetCopyType(Resource::COPY_CPU_TO_STAGING);
    std::memset(mTexture->GetDataFor(0), 0xFF, mTexture->GetNumBytesFor(0));

    auto effect = std::make_shared<DirectionalLightTextureEffect>(mProgramFactory,
        mUpdater, material, lighting, geometry, mTexture,
        SamplerState::MIN_L_MAG_L_MIP_L, SamplerState::CLAMP, SamplerState::CLAMP);

    mMesh->SetEffect(effect);
    mPVWMatrices.Subscribe(mMesh->worldTransform, effect->GetPVWMatrixConstant());
    mTrackball.Attach(mMesh);
}
