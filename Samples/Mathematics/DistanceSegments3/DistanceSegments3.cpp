// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.4 (2019/04/23)

#include <Applications/GteEnvironment.h>
#include <LowLevel/GteLogReporter.h>
#include <LowLevel/GteTimer.h>
#include <GTGraphics.h>
#include <Mathematics/GteArbitraryPrecision.h>
#include <Mathematics/GteDistSegmentSegment.h>
#include <Mathematics/GteDistSegmentSegmentExact.h>
#include <iostream>
using namespace gte;

Environment gEnvironment;
std::string gShaderFile = DefaultShaderName("DistanceSeg3Seg3.cs");

// The function dist3D_Segment_to_Segment is from Dan Sunday's website:
//   http://geomalgorithms.com/a07-_distance.html
// with some modifications.  The inputs of type Segment were replaced by
// point pairs of type Vector3<double> and the algebraic operator calls were
// replaced accordingly.  The distance is now returned (with other parameters)
// as arguments to the function.  The SMALL_NUM macro was replaced by a 
// 'const' declaration.  The modified code computes the closest points.  See
// the revised document (as of 2014/11/05)
//   https://www.geometrictools.com/Documentation/DistanceLine3Line3.pdf
// that describes an algorithm that is robust, particularly for nearly
// segments, and that uses floating-point arithmetic.  An example in this PDF
// shows that there is a problem with the logic of Sunday's algorithm when
// D < SMALL_NUM and the search is started on the s=0 edge. Specifically,
// the closest points are not found correctly--the closest point on the
// first segment occurs when s=1.  No contact information is at his website,
// so we are unable to report the problem to him.

void dist3D_Segment_to_Segment(
    Vector3<double> const& P0, Vector3<double> const& P1,
    Vector3<double> const& Q0, Vector3<double> const& Q1,
    double& sqrDistance, double& s, double& t, Vector3<double> closest[2])
{
    double const SMALL_NUM = 0.00000001;
    Vector3<double>   u = P1 - P0;
    Vector3<double>   v = Q1 - Q0;
    Vector3<double>   w = P0 - Q0;
    double    a = Dot(u, u);         // always >= 0
    double    b = Dot(u, v);
    double    c = Dot(v, v);         // always >= 0
    double    d = Dot(u, w);
    double    e = Dot(v, w);
    double    D = a*c - b*b;        // always >= 0
    double    sc, sN, sD = D;       // sc = sN / sD, default sD = D >= 0
    double    tc, tN, tD = D;       // tc = tN / tD, default tD = D >= 0

    // compute the line parameters of the two closest points
    if (D < SMALL_NUM) { // the lines are almost parallel
        sN = 0.0;         // force using point P0 on segment S1
        sD = 1.0;         // to prevent possible division by 0.0 later
        tN = e;
        tD = c;
    }
    else {                 // get the closest points on the infinite lines
        sN = (b*e - c*d);
        tN = (a*e - b*d);
        if (sN < 0.0) {        // sc < 0 => the s=0 edge is visible
            sN = 0.0;
            tN = e;
            tD = c;
        }
        else if (sN > sD) {  // sc > 1  => the s=1 edge is visible
            sN = sD;
            tN = e + b;
            tD = c;
        }
    }

    if (tN < 0.0) {            // tc < 0 => the t=0 edge is visible
        tN = 0.0;
        // recompute sc for this edge
        if (-d < 0.0)
            sN = 0.0;
        else if (-d > a)
            sN = sD;
        else {
            sN = -d;
            sD = a;
        }
    }
    else if (tN > tD) {      // tc > 1  => the t=1 edge is visible
        tN = tD;
        // recompute sc for this edge
        if ((-d + b) < 0.0)
            sN = 0;
        else if ((-d + b) > a)
            sN = sD;
        else {
            sN = (-d + b);
            sD = a;
        }
    }
    // finally do the division to get sc and tc
    sc = (std::abs(sN) < SMALL_NUM ? 0.0 : sN / sD);
    tc = (std::abs(tN) < SMALL_NUM ? 0.0 : tN / tD);

    // get the difference of the two closest points
    s = sc;
    t = tc;
    closest[0] = (1.0 - sc) * P0 + sc * P1;
    closest[1] = (1.0 - tc) * Q0 + tc * Q1;
    Vector3<double> diff = closest[0] - closest[1];
    sqrDistance = Dot(diff, diff);
}

enum { PERF_SUNDAY, PERF_ROBUST, PERF_RATIONAL };
typedef BSRational<UIntegerFP32<128>> Rational;
typedef DCPQuery<double, Segment<3, double>, Segment<3, double>> RobustQuery;
typedef DistanceSegmentSegmentExact<3, Rational> RationalQuery;

template <int N>
void LoadInput(bool testNonparallel, unsigned int numInputs, Segment<N, double>* segment)
{
    int numChannels = N;

    if (testNonparallel)
    {
        std::ifstream input("InputNonparallel.binary", std::ios::binary);
        for (unsigned int i = 0; i < numInputs; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                input.read((char*)& segment[i].p[0][j], sizeof(double));
                input.read((char*)& segment[i].p[1][j], sizeof(double));
            }
            if (numChannels == 4)
            {
                segment[i].p[0][3] = 1.0;
                segment[i].p[1][3] = 1.0;
            }
        }
        input.close();
    }
    else
    {
        std::ifstream input("InputParallel.binary", std::ios::binary);
        for (unsigned int i = 0; i < numInputs; ++i)
        {
            input.read((char*)& segment[i].p[0][0], sizeof(double));
            input.read((char*)& segment[i].p[0][1], sizeof(double));
            input.read((char*)& segment[i].p[0][2], sizeof(double));
            input.read((char*)& segment[i].p[1][0], sizeof(double));
            input.read((char*)& segment[i].p[1][1], sizeof(double));
            input.read((char*)& segment[i].p[1][2], sizeof(double));
            if (numChannels == 4)
            {
                segment[i].p[0][3] = 1.0;
                segment[i].p[1][3] = 1.0;
            }
        }
        input.close();
    }
}

void CPUAccuracyTest(bool compareUsingExact, bool testNonparallel)
{
    // NOTE:  When comparing to exact arithmetic results, the number of inputs
    // needs to be smaller because the exact algorithm is expensive to compute.
    // In this case the maximum errors are all small (4e-16).  However, when
    // not comparing to exact results, maxError01 is on the order of 1e-4.
    // The pair of segments that generate the maximum error shows that the
    // logic of dist3D_Segment_to_Segment when segments are nearly parallel
    // is not correct.
    unsigned int const numInputs = (compareUsingExact ? 1024 : 16384);
    unsigned int const numBlocks = 16;
    std::vector<Segment<3, double>> segment(numInputs);

    LoadInput(testNonparallel, numInputs, segment.data());

    double maxError01 = 0.0, maxError02 = 0.0, maxError12 = 0.0, error;
    unsigned int xmax01 = 0, ymax01 = 0;
    unsigned int xmax02 = 0, ymax02 = 0;
    unsigned int xmax12 = 0, ymax12 = 0;

    for (unsigned int y = 0; y < numInputs; ++y)
    {
        if ((y % numBlocks) == 0)
        {
            std::cout << "y = " << y << std::endl;
        }

        Vector3<double> Q0 = segment[y].p[0];
        Vector3<double> Q1 = segment[y].p[1];

        for (unsigned int x = y + 1; x < numInputs; ++x)
        {
            Vector3<double> P0 = segment[x].p[0];
            Vector3<double> P1 = segment[x].p[1];

            // Sunday's query
            double sqrDistance0, s0, t0;
            Vector3<double> closest0[2];
            dist3D_Segment_to_Segment(P0, P1, Q0, Q1, sqrDistance0, s0, t0, closest0);
            double distance0 = std::sqrt(sqrDistance0);

            // robust query
            RobustQuery query1;
            auto result1 = query1(P0, P1, Q0, Q1);
            double distance1 = result1.distance;

            if (compareUsingExact)
            {
                // rational query
                Vector3<Rational> RP0{ P0[0], P0[1], P0[2] };
                Vector3<Rational> RP1{ P1[0], P1[1], P1[2] };
                Vector3<Rational> RQ0{ Q0[0], Q0[1], Q0[2] };
                Vector3<Rational> RQ1{ Q1[0], Q1[1], Q1[2] };
                RationalQuery query2;
                auto result2 = query2(RP0, RP1, RQ0, RQ1);
                double distance2 = std::sqrt((double)result2.sqrDistance);

                error = std::abs(distance0 - distance2);
                if (error > maxError02)
                {
                    maxError02 = error;
                    xmax02 = x;
                    ymax02 = y;
                }

                error = std::abs(distance1 - distance2);
                if (error > maxError12)
                {
                    maxError12 = error;
                    xmax12 = x;
                    ymax12 = y;
                }
            }

            error = std::abs(distance0 - distance1);
            if (error > maxError01)
            {
                maxError01 = error;
                xmax01 = x;
                ymax01 = y;
            }
        }
    }

    if (compareUsingExact)
    {
        std::cout << "max error02 = " << maxError02 << std::endl;
        std::cout << "x, y = " << xmax02 << " " << ymax02 << std::endl;
        std::cout << "max error12 = " << maxError12 << std::endl;
        std::cout << "x, y = " << xmax12 << " " << ymax12 << std::endl;
    }
    std::cout << "max error01 = " << maxError01 << std::endl;
    std::cout << "x, y = " << xmax01 << " " << ymax01 << std::endl;
}

void CPUPerformanceTest(int select, bool testNonparallel)
{
    unsigned int const numInputs = (select == PERF_RATIONAL ? 1024 : 16384);
    std::vector<Segment<3, double>> segment(numInputs);

    LoadInput(testNonparallel, numInputs, segment.data());

    Timer timer;

    if (select == PERF_SUNDAY)
    {
        double sqrDistance0, s0, t0;
        Vector3<double> closest0[2];
        for (unsigned int y = 0; y < numInputs; ++y)
        {
            for (unsigned int x = y + 1; x < numInputs; ++x)
            {
                dist3D_Segment_to_Segment(
                    segment[x].p[0], segment[x].p[1],
                    segment[y].p[0], segment[y].p[1],
                    sqrDistance0, s0, t0, closest0);
            }
        }
    }
    else if (select == PERF_ROBUST)
    {
        RobustQuery query;
        RobustQuery::Result result;
        for (unsigned int y = 0; y < numInputs; ++y)
        {
            for (unsigned int x = y + 1; x < numInputs; ++x)
            {
                result = query(segment[x], segment[y]);
            }
        }
    }
    else  // select == PERF_RATIONAL
    {
        RationalQuery query;
        RationalQuery::Result result;
        Vector3<Rational> RP0, RP1, RQ0, RQ1;
        for (unsigned int y = 0; y < numInputs; ++y)
        {
            for (int i = 0; i < 3; ++i)
            {
                RQ0[i] = segment[y].p[0][i];
                RQ1[i] = segment[y].p[1][i];
            }
            for (unsigned int x = y + 1; x < numInputs; ++x)
            {
                for (int i = 0; i < 3; ++i)
                {
                    RP0[i] = segment[x].p[0][i];
                    RP1[i] = segment[x].p[1][i];
                }
                result = query(RP0, RP1, RQ0, RQ1);
            }
        }
    }

    std::cout << timer.GetSeconds() << std::endl;
}

void GPUAccuracyTest(bool getClosest, bool testNonparallel)
{
    unsigned int const numInputs = 16384;
    unsigned int const blockSize = 1024;
    unsigned int const numBlocks = numInputs / blockSize;
    unsigned int const numThreads = 8;
    unsigned int const numGroups = blockSize / numThreads;

    DefaultEngine engine;
    DefaultProgramFactory factory;
    factory.defines.Set("NUM_X_THREADS", numThreads);
    factory.defines.Set("NUM_Y_THREADS", numThreads);
    factory.defines.Set("BLOCK_SIZE", blockSize);
    factory.defines.Set("REAL", "double");
#if defined(GTE_DEV_OPENGL)
    factory.defines.Set("VECREAL", "dvec4");
#else
    factory.defines.Set("VECREAL", "double4");
#endif
    factory.defines.Set("GET_CLOSEST", (getClosest ? 1 : 0));

    auto cprogram = factory.CreateFromFile(gEnvironment.GetPath(gShaderFile));
    auto cshader = cprogram->GetCShader();
    auto block = std::make_shared<ConstantBuffer>(2 * sizeof(uint32_t), true);
    cshader->Set("Block", block);
    auto* origin = block->Get<uint32_t>();

    auto input = std::make_shared<StructuredBuffer>(numInputs, sizeof(Segment<4, double>));
    input->SetUsage(Resource::DYNAMIC_UPDATE);
    cshader->Set("inSegment", input);
    auto* segment = input->Get<Segment<4, double>>();

    LoadInput(testNonparallel, numInputs, segment);

    std::shared_ptr<StructuredBuffer> output;
    double maxError = 0.0;
    int xmax = 0, ymax = 0;
    if (getClosest)
    {
        // GLSL wants closest[] to be aligned on a dvec4 boundary, so
        // parameter[2] is padding.
        struct Result
        {
            double sqrDistance;
            double parameter[3];
            Vector4<double> closest[2];
        };

        output = std::make_shared<StructuredBuffer>(blockSize * blockSize, sizeof(Result));
        output->SetUsage(Resource::SHADER_OUTPUT);
        output->SetCopyType(Resource::COPY_STAGING_TO_CPU);
        Result* gpuResult = output->Get<Result>();
        cshader->Set("outResult", output);

        for (unsigned int y = 0, i = 0; y < numBlocks; ++y)
        {
            std::cout << "block = " << y << std::endl;
            origin[1] = y * blockSize;
            for (unsigned int x = y; x < numBlocks; ++x, ++i)
            {
                origin[0] = x * blockSize;
                engine.Update(block);
                engine.Execute(cprogram, numGroups, numGroups, 1);
                engine.CopyGpuToCpu(output);

                for (unsigned int r = 0; r < blockSize; ++r)
                {
                    int sy = origin[1] + r;
                    Vector3<double> Q0 = HProject(segment[sy].p[0]);
                    Vector3<double> Q1 = HProject(segment[sy].p[1]);

                    unsigned int cmin = (x != y ? 0 : r + 1);
                    for (unsigned int c = cmin; c < blockSize; ++c)
                    {
                        int sx = origin[0] + c;
                        Vector3<double> P0 = HProject(segment[sx].p[0]);
                        Vector3<double> P1 = HProject(segment[sx].p[1]);

                        Result result0 = gpuResult[c + blockSize * r];
                        double distance0 = std::sqrt(result0.sqrDistance);

                        RobustQuery query1;
                        auto result1 = query1(P0, P1, Q0, Q1);
                        double distance1 = result1.distance;

                        double error = std::abs(distance0 - distance1);
                        if (error > maxError)
                        {
                            maxError = error;
                            xmax = sx;
                            ymax = sy;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // GLSL wants closest[] to be aligned on a dvec4 boundary, so
        // parameter[2] is padding.
        struct Result
        {
            double sqrDistance;
            double parameter[3];
        };

        output = std::make_shared<StructuredBuffer>(blockSize * blockSize, sizeof(Result));
        output->SetUsage(Resource::SHADER_OUTPUT);
        output->SetCopyType(Resource::COPY_STAGING_TO_CPU);
        Result* gpuResult = output->Get<Result>();
        cshader->Set("outResult", output);

        for (unsigned int y = 0, i = 0; y < numBlocks; ++y)
        {
            std::cout << "block = " << y << std::endl;
            origin[1] = y * blockSize;
            for (unsigned int x = y; x < numBlocks; ++x, ++i)
            {
                origin[0] = x * blockSize;
                engine.Update(block);
                engine.Execute(cprogram, numGroups, numGroups, 1);
                engine.CopyGpuToCpu(output);

                for (unsigned int r = 0; r < blockSize; ++r)
                {
                    int sy = origin[1] + r;
                    Vector3<double> Q0 = HProject(segment[sy].p[0]);
                    Vector3<double> Q1 = HProject(segment[sy].p[1]);

                    unsigned int cmin = (x != y ? 0 : r + 1);
                    for (unsigned int c = cmin; c < blockSize; ++c)
                    {
                        int sx = origin[0] + c;
                        Vector3<double> P0 = HProject(segment[sx].p[0]);
                        Vector3<double> P1 = HProject(segment[sx].p[1]);

                        Result result0 = gpuResult[c + blockSize * r];
                        double distance0 = std::sqrt(result0.sqrDistance);

                        RobustQuery query1;
                        auto result1 = query1(P0, P1, Q0, Q1);
                        double distance1 = result1.distance;

                        double error = std::abs(distance0 - distance1);
                        if (error > maxError)
                        {
                            maxError = error;
                            xmax = sx;
                            ymax = sy;
                        }
                    }
                }
            }
        }
    }

    std::cout << "max error = " << maxError << std::endl;
    std::cout << "x, y = " << xmax << " " << ymax << std::endl;

    output = nullptr;
    input = nullptr;
    block = nullptr;
    cprogram = nullptr;
    cshader = nullptr;
}

void GPUPerformanceTest(bool getClosest, bool testNonparallel)
{
    unsigned int const numInputs = 16384;
    unsigned int const blockSize = 1024;
    unsigned int const numBlocks = numInputs / blockSize;
    unsigned int const numThreads = 8;
    unsigned int const numGroups = blockSize / numThreads;

    DefaultEngine engine;
    DefaultProgramFactory factory;
    factory.defines.Set("NUM_X_THREADS", numThreads);
    factory.defines.Set("NUM_Y_THREADS", numThreads);
    factory.defines.Set("BLOCK_SIZE", blockSize);
    factory.defines.Set("REAL", "double");
#if defined(GTE_DEV_OPENGL)
    factory.defines.Set("VECREAL", "dvec4");
#else
    factory.defines.Set("VECREAL", "double4");
#endif
    factory.defines.Set("GET_CLOSEST", (getClosest ? 1 : 0));
    auto cprogram = factory.CreateFromFile(gEnvironment.GetPath(gShaderFile));
    auto cshader = cprogram->GetCShader();

    auto block = std::make_shared<ConstantBuffer>(2 * sizeof(uint32_t), true);
    cshader->Set("Block", block);
    auto* origin = block->Get<uint32_t>();

    auto input = std::make_shared<StructuredBuffer>(numInputs, sizeof(Segment<4, double>));
    input->SetUsage(Resource::DYNAMIC_UPDATE);
    cshader->Set("inSegment", input);
    Segment<4, double>* segment = input->Get<Segment<4, double>>();

    LoadInput(testNonparallel, numInputs, segment);

    Timer timer;
    std::shared_ptr<StructuredBuffer> output;
    if (getClosest)
    {
        // GLSL wants closest[] to be aligned on a dvec4 boundary, so
        // parameter[2] is padding.
        struct Result
        {
            double sqrDistance;
            double parameter[3];
            Vector3<double> closest[2];
        };

        output = std::make_shared<StructuredBuffer>(blockSize * blockSize, sizeof(Result));
        output->SetUsage(Resource::SHADER_OUTPUT);
        output->SetCopyType(Resource::COPY_STAGING_TO_CPU);
        cshader->Set("outResult", output);

        for (unsigned int y = 0, i = 0; y < numBlocks; ++y)
        {
            origin[1] = y * blockSize;
            for (unsigned int x = y; x < numBlocks; ++x, ++i)
            {
                origin[0] = x * blockSize;
                engine.Update(block);
                engine.Execute(cprogram, numGroups, numGroups, 1);
                engine.CopyGpuToCpu(output);
            }
        }

    }
    else
    {
        // GLSL wants closest[] to be aligned on a dvec4 boundary, so
        // parameter[2] is padding.
        struct Result
        {
            double sqrDistance;
            double parameter[3];
        };

        output = std::make_shared<StructuredBuffer>(blockSize * blockSize, sizeof(Result));
        output->SetUsage(Resource::SHADER_OUTPUT);
        output->SetCopyType(Resource::COPY_STAGING_TO_CPU);
        cshader->Set("outResult", output);

        for (unsigned int y = 0, i = 0; y < numBlocks; ++y)
        {
            origin[1] = y * blockSize;
            for (unsigned int x = y; x < numBlocks; ++x, ++i)
            {
                origin[0] = x * blockSize;
                engine.Update(block);
                engine.Execute(cprogram, numGroups, numGroups, 1);
                engine.CopyGpuToCpu(output);
            }
        }
    }
    std::cout << "seconds = " << timer.GetSeconds() << std::endl;

    output = nullptr;
    input = nullptr;
    block = nullptr;
    cshader = nullptr;
    cprogram = nullptr;
}

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

    std::string message;
    std::string path = gEnvironment.GetVariable("GTE_PATH");
    if (path == "")
    {
        message = "You must create the environment variable GTE_PATH.";
        std::cout << message << std::endl;
        LogError(message);
        return -1;
    }

    gEnvironment.Insert(path + "/Samples/Mathematics/DistanceSegments3/Shaders/");

    if (gEnvironment.GetPath(gShaderFile) == "")
    {
        message = "Cannot find file " + gShaderFile;
        std::cout << message << std::endl;
        LogError(message);
        return -2;
    }

    // The experiments were run on an Intel Core i7-6700 CPU @ 3.40 GHz and an
    // NVIDIA GeForce GTX 1080.  The CPU runs are single-threaded.  The times
    // are for the Release build run without a debugger attached.  The GPU
    // tests use the robust algorithm, so the times must be compared to those
    // of the CPU PERF_ROBUST.

    // max error02 = 4.44089e-16 at (x,y) = (346,1)
    // max error12 = 4.44089e-16 at (x,y) = (346,1)
    // max error01 = 6.66134e-16 at (x,y) = (520,288)
    CPUAccuracyTest(true, true);

    // max error02 = 3.52850e-07 at (x,y) = (362,283)
    // max error12 = 4.17519e-08 at (x,y) = (994,186)
    // max error01 = 3.51795e-07 at (x,y) = (722,362)
    CPUAccuracyTest(true, false);

    // max error01 = 6.66134e-16 at (x,y) = (520,288)
    CPUAccuracyTest(false, true);

    // max error01 = 1.09974e-06 at (x,y) = (1024,569)
    CPUAccuracyTest(false, false);

    // time = 4.022 seconds, 134209536 queries, 2.996806e-08 seconds/query
    CPUPerformanceTest(PERF_SUNDAY, true);

    // time = 2.863 seconds, 134209536 queries, 2.133231e-08 seconds/query
    CPUPerformanceTest(PERF_SUNDAY, false);

    // time = 6.290 seconds, 134209536 queries, 4.686701e-08 seconds/query
    CPUPerformanceTest(PERF_ROBUST, true);

    // time = 6.227 seconds, 134209536 queries, 4.639760e-08 seconds/query
    CPUPerformanceTest(PERF_ROBUST, false);

    // time = 6.782 seconds,    523776 queries, 1.294828e-05 seconds/query
    CPUPerformanceTest(PERF_RATIONAL, true);

    // time = 3.250 seconds,    523776 queries, 6.204943e-05 seconds/query
    CPUPerformanceTest(PERF_RATIONAL, false);

    // DX11,   max error = 0 at (x,y) = (0,0)
    // OpenGL, max error = 8.88178e-16 at (x,y) = (12279,89)
    GPUAccuracyTest(true, true);

    // DX11,   max error = 0 at (x,y) = (0,0)
    // OpenGL, max error = 4.62039e-08 at (x,y) = (15035,106)
    GPUAccuracyTest(true, false);

    // DX11,   max error = 0 at (x,y) = (0,0)
    // OpenGL, max error = 8.88178e-16 at (x,y) = (12279,89)
    GPUAccuracyTest(false, true);

    // DX11,   max error = 0 at (x,y) = (0,0)
    // OpenGL, max error = 4.62039e-08 at (x,y) = (15035,106)
    GPUAccuracyTest(false, false);

    // DX11,   time = 3.903 seconds, 134209536 queries, 2.421586e-08 seconds/query
    // OpenGL, time = 4.055 seconds, 134209536 queries, 3.021395e-08 seconds/query
    GPUPerformanceTest(true, true);

    // DX11,   time = 3.858 seconds, 134209536 queries, 2.874609e-08 seconds/query
    // OpenGL, time = 3.962 seconds, 134209536 queries, 2.952100e-08 seconds/query
    GPUPerformanceTest(true, false);

    // DX11,   time = 1.864 seconds, 134209536 queries, 1.388873e-08 seconds/query
    // OpenGL, time = 1.866 seconds, 134209536 queries, 1.390363e-08 seconds/query
    GPUPerformanceTest(false, true);

    // DX11,   time = 1.693 seconds, 134209536 queries, 1.261460e-08 seconds/query
    // OpenGL, time = 1.721 seconds, 134209536 queries, 1.282323e-08 seconds/query
    GPUPerformanceTest(false, false);

    return 0;
}

