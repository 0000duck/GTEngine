// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.23.2 (2019/05/03)

#include "GpuGaussianBlur3Window.h"
#include <LowLevel/GteLogReporter.h>
#include <Applications/GteCommand.h>
#include <Graphics/GteGraphicsDefaults.h>

static bool gsUseDirichlet;

int main(int numArguments, char const* arguments[])
{
#if defined(_DEBUG)
    LogReporter reporter(
        "LogReport.txt",
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL);
#endif
    Command command(numArguments, arguments);
    gsUseDirichlet = (command.GetBoolean("d") > 0 ? true : false);

    // The window size is that of the 8x8 tiled Head_U16_X128_Y128_Z64.binary
    // image.
    Window::Parameters parameters(L"GpuGaussianBlur3Window", 0, 0, 1024, 1024);
    auto window = TheWindowSystem.Create<GpuGaussianBlur3Window>(parameters);
    TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
    TheWindowSystem.Destroy<GpuGaussianBlur3Window>(window);
    return 0;
}

GpuGaussianBlur3Window::GpuGaussianBlur3Window(Parameters& parameters)
    :
    Window3(parameters),
    mNumXThreads(8),
    mNumYThreads(8),
    mNumXGroups(mXSize / mNumXThreads),
    mNumYGroups(mYSize / mNumYThreads)
{
    if (!SetEnvironment() || !CreateImages() || !CreateShaders())
    {
        parameters.created = false;
        return;
    }

    std::string path = mEnvironment.GetPath(DefaultShaderName("DrawImage.ps"));
    std::string psSource = ProgramFactory::GetStringFromFile(path);

    // Create an overlay that covers the entire window.  The blurred image
    // is drawn by the overlay effect.
    mOverlay = std::make_shared<OverlayEffect>(mProgramFactory, mXSize,
        mYSize, mXSize, mYSize, psSource);

    auto nearestSampler = std::make_shared<SamplerState>();
    nearestSampler->filter = SamplerState::MIN_P_MAG_P_MIP_P;
    nearestSampler->mode[0] = SamplerState::CLAMP;
    nearestSampler->mode[1] = SamplerState::CLAMP;
    auto pshader = mOverlay->GetProgram()->GetPShader();
    pshader->Set("inImage", mImage[0], "imageSampler", nearestSampler);
    pshader->Set("inMask", mMaskTexture, "maskSampler", nearestSampler);
}

void GpuGaussianBlur3Window::OnIdle()
{
    mTimer.Measure();

    mEngine->Execute(mGaussianBlurProgram, mNumXGroups, mNumYGroups, 1);
    if (gsUseDirichlet)
    {
        mEngine->Execute(mBoundaryDirichletProgram, mNumXGroups, mNumYGroups, 1);
    }
    else
    {
        mEngine->Execute(mBoundaryNeumannProgram, mNumXGroups, mNumYGroups, 1);
    }

    mEngine->Draw(mOverlay);

    mEngine->Draw(8, mYSize - 8, { 1.0f, 1.0f, 0.0f, 1.0f }, mTimer.GetFPS());
    mEngine->DisplayColorBuffer(0);

    mTimer.UpdateFrameCount();
}

bool GpuGaussianBlur3Window::SetEnvironment()
{
    std::string path = GetGTEPath();
    if (path == "")
    {
        return false;
    }

    mEnvironment.Insert(path + "/Samples/Imagics/GpuGaussianBlur3/Shaders/");
    mEnvironment.Insert(path + "/Samples/Data/");
    std::vector<std::string> inputs =
    {
        "Head_U16_X128_Y128_Z64.binary",
        DefaultShaderName("BoundaryDirichlet.cs"),
        DefaultShaderName("BoundaryNeumann.cs"),
        DefaultShaderName("GaussianBlur.cs"),
        DefaultShaderName("DrawImage.ps")
    };

    for (auto const& input : inputs)
    {
        if (mEnvironment.GetPath(input) == "")
        {
            LogError("Cannot find file " + input);
            return false;
        }
    }

    return true;
}

bool GpuGaussianBlur3Window::CreateImages()
{
    for (int i = 0; i < 2; ++i)
    {
        mImage[i] = std::make_shared<Texture2>(DF_R32_FLOAT, mXSize, mYSize);
        mImage[i]->SetUsage(Resource::SHADER_OUTPUT);
    }

    // The head image is known to store 12 bits per pixel with values in
    // [0,3365].  The image is stored in lexicographical order with voxels
    // (x,y,z) mapped to 1-dimensional indices i = x + 128 * (y + 128 * z).
    std::string path = mEnvironment.GetPath("Head_U16_X128_Y128_Z64.binary");
    if (path == "")
    {
        return false;
    }
    std::vector<uint16_t> original(mXSize * mYSize);
    std::ifstream input(path, std::ios::binary);
    input.read((char*)original.data(), original.size() * sizeof(uint16_t));
    input.close();

    // Scale the 3D image to have values in [0,1).
    float const divisor = 3366.0f;
    std::vector<float> scaled(mXSize * mYSize);
    for (int i = 0; i < mXSize * mYSize; ++i)
    {
        scaled[i] = static_cast<float>(original[i]) / divisor;
    }

    // Map the 3D image to a 2D 8x8 tiled image where each tile is 128x128.
    float* texels = mImage[0]->Get<float>();
    for (int v = 0; v < mYSize; ++v)
    {
        for (int u = 0; u < mXSize; ++u)
        {
            int x, y, z;
            Map2Dto3D(u, v, x, y, z);
            texels[Map2Dto1D(u, v)] = scaled[x + 128 * (y + 128 * z)];
        }
    }

    // Create the mask texture for BoundaryDirichlet and the offset
    // texture for BoundaryNeumann.
    mMaskTexture = std::make_shared<Texture2>(DF_R32_FLOAT, mXSize, mYSize);
    auto* mask = mMaskTexture->Get<float>();
    mNeumannOffsetTexture = std::make_shared<Texture2>(DF_R32G32_SINT, mXSize, mYSize);
    auto* offset = mNeumannOffsetTexture->Get<std::array<int, 2>>();
    int const xBound = 128, yBound = 128, zBound = 64;
    int xBoundM1 = xBound - 1, yBoundM1 = yBound - 1, zBoundM1 = zBound - 1;
    int x, y, z, index;

    // Interior.
    for (z = 1; z < zBoundM1; ++z)
    {
        for (y = 1; y < yBoundM1; ++y)
        {
            for (x = 1; x < xBoundM1; ++x)
            {
                index = Map3Dto1D(x, y, z);
                mask[index] = 1.0f;
                offset[index] = { 0, 0 };
            }
        }
    }

    // x-face-interior.
    for (z = 1; z < zBoundM1; ++z)
    {
        for (y = 1; y < yBoundM1; ++y)
        {
            index = Map3Dto1D(0, y, z);
            mask[index] = 0.0f;
            offset[index] = { +1, 0 };
            index = Map3Dto1D(xBoundM1, y, z);
            mask[index] = 0.0f;
            offset[index] = { -1, 0 };
        }
    }

    // y-face-interior.
    for (z = 1; z < zBoundM1; ++z)
    {
        for (x = 1; x < xBoundM1; ++x)
        {
            index = Map3Dto1D(x, 0, z);
            mask[index] = 0.0f;
            offset[index] = { 0, +1 };
            index = Map3Dto1D(x, yBoundM1, z);
            mask[index] = 0.0f;
            offset[index] = { 0, -1 };
        }
    }

    // z-face-interior.
    for (y = 1; y < yBoundM1; ++y)
    {
        for (x = 1; x < xBoundM1; ++x)
        {
            index = Map3Dto1D(x, y, 0);
            mask[index] = 0.0f;
            offset[index] = { +xBound, 0 };
            index = Map3Dto1D(x, y, zBoundM1);
            mask[index] = 0.0f;
            offset[index] = { -xBound, 0 };
        }
    }

    // x-edge-interior.
    for (x = 1; x < xBoundM1; ++x)
    {
        index = Map3Dto1D(x, 0, 0);
        mask[index] = 0.0f;
        offset[index] = { +xBound, +1 };
        index = Map3Dto1D(x, 0, zBoundM1);
        mask[index] = 0.0f;
        offset[index] = { -xBound, +1 };
        index = Map3Dto1D(x, yBoundM1, 0);
        mask[index] = 0.0f;
        offset[index] = { +xBound, -1 };
        index = Map3Dto1D(x, yBoundM1, zBoundM1);
        mask[index] = 0.0f;
        offset[index] = { -xBound, -1 };
    }

    // y-edge-interior.
    for (y = 1; y < yBoundM1; ++y)
    {
        index = Map3Dto1D(0, y, 0);
        mask[index] = 0.0f;
        offset[index] = { +xBound + 1, 0 };
        index = Map3Dto1D(0, y, zBoundM1);
        mask[index] = 0.0f;
        offset[index] = { -xBound + 1, 0 };
        index = Map3Dto1D(xBoundM1, y, 0);
        mask[index] = 0.0f;
        offset[index] = { +xBound - 1, 0 };
        index = Map3Dto1D(xBoundM1, y, zBoundM1);
        mask[index] = 0.0f;
        offset[index] = { -xBound - 1, 0 };
    }

    // z-edge-interior
    for (z = 1; z < zBoundM1; ++z)
    {
        index = Map3Dto1D(0, 0, z);
        mask[index] = 0.0f;
        offset[index] = { +1, +1 };
        index = Map3Dto1D(0, yBoundM1, z);
        mask[index] = 0.0f;
        offset[index] = { +1, -1 };
        index = Map3Dto1D(xBoundM1, 0, z);
        mask[index] = 0.0f;
        offset[index] = { -1, +1 };
        index = Map3Dto1D(xBoundM1, yBoundM1, z);
        mask[index] = 0.0f;
        offset[index] = { -1, -1 };
    }

    // Corners.
    index = Map3Dto1D(0, 0, 0);
    mask[index] = 0.0f;
    offset[index] = { +xBound + 1, +1 };
    index = Map3Dto1D(xBoundM1, 0, 0);
    mask[index] = 0.0f;
    offset[index] = { +xBound - 1, +1 };
    index = Map3Dto1D(0, yBoundM1, 0);
    mask[index] = 0.0f;
    offset[index] = { +xBound + 1, -1 };
    index = Map3Dto1D(xBoundM1, yBoundM1, 0);
    mask[index] = 0.0f;
    offset[index] = { +xBound - 1, -1 };
    index = Map3Dto1D(0, 0, zBoundM1);
    mask[index] = 0.0f;
    offset[index] = { -xBound + 1, +1 };
    index = Map3Dto1D(xBoundM1, 0, zBoundM1);
    mask[index] = 0.0f;
    offset[index] = { -xBound - 1, +1 };
    index = Map3Dto1D(0, yBoundM1, zBoundM1);
    mask[index] = 0.0f;
    offset[index] = { -xBound + 1, -1 };
    index = Map3Dto1D(xBoundM1, yBoundM1, zBoundM1);
    mask[index] = 0.0f;
    offset[index] = { -xBound - 1, -1 };

    // Create the offset texture for GaussianBlur.
    mZNeighborTexture = std::make_shared<Texture2>(DF_R32G32B32A32_SINT, mXSize, mYSize);
    auto* zneighbor = mZNeighborTexture->Get<std::array<int, 4>>();
    std::memset(mZNeighborTexture->GetData(), 0, mZNeighborTexture->GetNumBytes());

    // Interior voxels.  The offsets at the boundary are all zero, so the
    // finite differences are incorrect at those locations.  However, the
    // boundary effect will overwrite those voxels, so it is irrelevant
    // about the finite difference approximations at those locations.
    for (z = 1; z < zBoundM1; ++z)
    {
        for (y = 1; y < yBoundM1; ++y)
        {
            for (x = 1; x < xBoundM1; ++x)
            {
                // Get the 2D location of(x,y,z).
                int u, v;
                Map3Dto2D(x, y, z, u, v);

                // Get the 2D location of the +z neighbor of (x,y,z).
                int upos, vpos;
                Map3Dto2D(x, y, z + 1, upos, vpos);

                // Get the 2D location of the -z neighbor of (x,y,z).
                int uneg, vneg;
                Map3Dto2D(x, y, z - 1, uneg, vneg);

                zneighbor[Map2Dto1D(u, v)] = { upos - u, vpos - v, uneg - u, vneg - v };
            }
        }
    }

    mWeightBuffer = std::make_shared<ConstantBuffer>(sizeof(Vector4<float>), false);
    auto& weight = *mWeightBuffer->Get<Vector4<float>>();
    weight[0] = 0.01f;  // = kappa*DeltaT/DeltaX^2
    weight[1] = 0.01f;  // = kappa*DeltaT/DeltaY^2
    weight[2] = 0.01f;  // = kappa*DeltaT/DeltaZ^2
    weight[3] = 1.0f - 2.0f * (weight[0] + weight[1] + weight[2]); // positive
    return true;
}

bool GpuGaussianBlur3Window::CreateShaders()
{
    mProgramFactory->defines.Set("NUM_X_THREADS", mNumXThreads);
    mProgramFactory->defines.Set("NUM_Y_THREADS", mNumYThreads);

    std::string csPath = mEnvironment.GetPath(DefaultShaderName("GaussianBlur.cs"));
    mGaussianBlurProgram = mProgramFactory->CreateFromFile(csPath);
    if (!mGaussianBlurProgram)
    {
        return false;
    }

    csPath = mEnvironment.GetPath(DefaultShaderName("BoundaryDirichlet.cs"));
    mBoundaryDirichletProgram = mProgramFactory->CreateFromFile(csPath);
    if (!mBoundaryDirichletProgram)
    {
        return false;
    }

    csPath = mEnvironment.GetPath(DefaultShaderName("BoundaryNeumann.cs"));
    mBoundaryNeumannProgram = mProgramFactory->CreateFromFile(csPath);
    if (!mBoundaryNeumannProgram)
    {
        return false;
    }

    auto cshader = mGaussianBlurProgram->GetCShader();
    cshader->Set("inImage", mImage[0]);
    cshader->Set("inZNeighbor", mZNeighborTexture);
    cshader->Set("outImage", mImage[1]);
    cshader->Set("Weight", mWeightBuffer);

    cshader = mBoundaryDirichletProgram->GetCShader();
    cshader->Set("inImage", mImage[1]);
    cshader->Set("inMask", mMaskTexture);
    cshader->Set("outImage", mImage[0]);

    cshader = mBoundaryNeumannProgram->GetCShader();
    cshader->Set("inImage", mImage[1]);
    cshader->Set("inOffset", mNeumannOffsetTexture);
    cshader->Set("outImage", mImage[0]);

    return true;
}
