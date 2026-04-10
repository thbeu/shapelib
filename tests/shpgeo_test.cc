#include <array>
#include <cmath>
#include <filesystem>
#include <memory>

#include <gtest/gtest.h>

extern "C"
{
#include "shapefil.h"
#include "shpgeo.h"
}

namespace
{

namespace fs = std::filesystem;

fs::path GetTestDataPath()
{
    for (const fs::path &candidate :
         {fs::path{"shape_eg_data"}, fs::path{"tests"} / "shape_eg_data"})
    {
        if (fs::exists(candidate))
            return candidate;
    }

    return fs::path{"shape_eg_data"};
}

const auto kTestData = GetTestDataPath();

struct ShapeDeleter
{
    void operator()(SHPObject *shape) const
    {
        if (shape != nullptr)
            SHPDestroyObject(shape);
    }
};

using ShapePtr = std::unique_ptr<SHPObject, ShapeDeleter>;

struct HandleDeleter
{
    void operator()(SHPHandle handle) const
    {
        if (handle != nullptr)
            SHPClose(handle);
    }
};

using HandlePtr = std::unique_ptr<SHPInfo, HandleDeleter>;

struct WKBHolder
{
    WKBStreamObj stream{};

    ~WKBHolder()
    {
        free(stream.wStream);
    }
};

ShapePtr RoundTrip(const SHPObject *shape)
{
    WKBHolder writer;
    EXPECT_EQ(0, SHPWriteOGisWKB(&writer.stream, shape));
    EXPECT_NE(nullptr, writer.stream.wStream);
    EXPECT_GT(writer.stream.StreamPos, 0);

    WKBStreamObj reader{};
    reader.wStream = writer.stream.wStream;
    ShapePtr roundTripped{SHPReadOGisWKB(&reader)};
    return roundTripped;
}

void ExpectSameShape(const SHPObject *expected, const SHPObject *actual)
{
    ASSERT_NE(nullptr, expected);
    ASSERT_NE(nullptr, actual);

    EXPECT_EQ(expected->nSHPType, actual->nSHPType);
    EXPECT_EQ(expected->nParts, actual->nParts);
    EXPECT_EQ(expected->nVertices, actual->nVertices);

    for (int i = 0; i < expected->nParts; i++)
    {
        EXPECT_EQ(expected->panPartStart[i], actual->panPartStart[i]);
        EXPECT_EQ(expected->panPartType[i], actual->panPartType[i]);
    }

    for (int i = 0; i < expected->nVertices; i++)
    {
        EXPECT_DOUBLE_EQ(expected->padfX[i], actual->padfX[i]);
        EXPECT_DOUBLE_EQ(expected->padfY[i], actual->padfY[i]);
        EXPECT_DOUBLE_EQ(expected->padfZ[i], actual->padfZ[i]);
        EXPECT_DOUBLE_EQ(expected->padfM[i], actual->padfM[i]);
    }
}

void ExpectDatasetRoundTrip(const fs::path &filename, int maxEntities = -1)
{
    const HandlePtr handle{SHPOpen(filename.string().c_str(), "rb")};
    ASSERT_NE(nullptr, handle) << filename.string();

    int nEntities = 0;
    int nShapeType = SHPT_NULL;
    SHPGetInfo(handle.get(), &nEntities, &nShapeType, nullptr, nullptr);
    ASSERT_GT(nEntities, 0) << filename.string();

    const int nLimit =
        (maxEntities >= 0 && maxEntities < nEntities) ? maxEntities : nEntities;
    for (int i = 0; i < nLimit; i++)
    {
        SCOPED_TRACE(filename.string());
        SCOPED_TRACE(i);
        ShapePtr shape{SHPReadObject(handle.get(), i)};
        ASSERT_NE(nullptr, shape);
        if (shape->nSHPType == SHPT_NULL)
            continue;

        const ShapePtr roundTripped = RoundTrip(shape.get());
        ExpectSameShape(shape.get(), roundTripped.get());
    }
}

TEST(SHPGeoWKBTest, RoundTripPoint)
{
    const std::array<double, 1> x{3.5};
    const std::array<double, 1> y{-1.25};
    ShapePtr shape{
        SHPCreateSimpleObject(SHPT_POINT, 1, x.data(), y.data(), nullptr)};

    const ShapePtr roundTripped = RoundTrip(shape.get());
    ExpectSameShape(shape.get(), roundTripped.get());
}

TEST(SHPGeoWKBTest, RoundTripMultiPoint)
{
    const std::array<double, 3> x{1.0, 2.5, 4.0};
    const std::array<double, 3> y{5.0, 6.5, 8.0};
    ShapePtr shape{SHPCreateSimpleObject(SHPT_MULTIPOINT,
                                         static_cast<int>(x.size()), x.data(),
                                         y.data(), nullptr)};

    const ShapePtr roundTripped = RoundTrip(shape.get());
    ExpectSameShape(shape.get(), roundTripped.get());
}

TEST(SHPGeoWKBTest, RoundTripPointZ)
{
    const std::array<double, 1> x{3.5};
    const std::array<double, 1> y{-1.25};
    const std::array<double, 1> z{42.0};
    ShapePtr shape{
        SHPCreateSimpleObject(SHPT_POINTZ, 1, x.data(), y.data(), z.data())};

    const ShapePtr roundTripped = RoundTrip(shape.get());
    ExpectSameShape(shape.get(), roundTripped.get());
}

TEST(SHPGeoWKBTest, RoundTripMultipartLine)
{
    const std::array<int, 2> parts{0, 3};
    const std::array<int, 2> partTypes{SHPP_RING, SHPP_RING};
    const std::array<double, 5> x{0.0, 1.0, 2.0, 10.0, 12.0};
    const std::array<double, 5> y{0.0, 1.0, 0.0, 3.0, 3.0};
    ShapePtr shape{SHPCreateObject(SHPT_ARC, -1, static_cast<int>(parts.size()),
                                   parts.data(), partTypes.data(),
                                   static_cast<int>(x.size()), x.data(),
                                   y.data(), nullptr, nullptr)};

    const ShapePtr roundTripped = RoundTrip(shape.get());
    ExpectSameShape(shape.get(), roundTripped.get());
}

TEST(SHPGeoWKBTest, RoundTripMultipartLineM)
{
    const std::array<int, 2> parts{0, 3};
    const std::array<int, 2> partTypes{SHPP_RING, SHPP_RING};
    const std::array<double, 5> x{0.0, 1.0, 2.0, 10.0, 12.0};
    const std::array<double, 5> y{0.0, 1.0, 0.0, 3.0, 3.0};
    const std::array<double, 5> m{7.0, 8.0, 9.0, 10.0, 11.0};
    ShapePtr shape{SHPCreateObject(SHPT_ARCM, -1,
                                   static_cast<int>(parts.size()), parts.data(),
                                   partTypes.data(), static_cast<int>(x.size()),
                                   x.data(), y.data(), nullptr, m.data())};

    const ShapePtr roundTripped = RoundTrip(shape.get());
    ExpectSameShape(shape.get(), roundTripped.get());
}

TEST(SHPGeoWKBTest, RoundTripPolygonWithHole)
{
    const std::array<int, 2> parts{0, 5};
    const std::array<int, 2> partTypes{SHPP_OUTERRING, SHPP_INNERRING};
    const std::array<double, 10> x{
        0.0, 0.0, 10.0, 10.0, 0.0, 2.0, 8.0, 8.0, 2.0, 2.0,
    };
    const std::array<double, 10> y{
        0.0, 10.0, 10.0, 0.0, 0.0, 2.0, 2.0, 8.0, 8.0, 2.0,
    };
    ShapePtr shape{SHPCreateObject(SHPT_POLYGON, -1,
                                   static_cast<int>(parts.size()), parts.data(),
                                   partTypes.data(), static_cast<int>(x.size()),
                                   x.data(), y.data(), nullptr, nullptr)};

    ASSERT_EQ(1, SHPRingDir_2d(shape.get(), 0));
    ASSERT_EQ(-1, SHPRingDir_2d(shape.get(), 1));

    const ShapePtr roundTripped = RoundTrip(shape.get());
    ExpectSameShape(shape.get(), roundTripped.get());
}

TEST(SHPGeoUtilityTest, SwapGSwapsAllItems)
{
    const std::array<unsigned char, 8> in{0x01, 0x02, 0x03, 0x04,
                                          0x10, 0x20, 0x30, 0x40};
    std::array<unsigned char, 8> out{};

    SwapG(out.data(), in.data(), 2, 4);

    const std::array<unsigned char, 8> expected{0x04, 0x03, 0x02, 0x01,
                                                0x40, 0x30, 0x20, 0x10};
    EXPECT_EQ(expected, out);
}

TEST(SHPGeoDatasetTest, RoundTripPolygonDataset)
{
    ExpectDatasetRoundTrip(kTestData / "polygon.shp");
}

TEST(SHPGeoDatasetTest, RoundTripPolylineDataset)
{
    ExpectDatasetRoundTrip(kTestData / "pline.shp");
}

TEST(SHPGeoDatasetTest, RoundTripMultiPointDataset)
{
    ExpectDatasetRoundTrip(kTestData / "multipnt.shp");
}

TEST(SHPGeoDatasetTest, RoundTripPointZDataset)
{
    ExpectDatasetRoundTrip(kTestData / "3dpoints.shp", 16);
}

}  // namespace

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}