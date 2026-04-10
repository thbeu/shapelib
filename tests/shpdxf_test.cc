#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

extern "C"
{
#include "shapefil.h"
    int shpdxf_run(int argc, char **argv);
}

namespace fs = std::filesystem;

namespace
{

std::string GenerateUniqueName(std::string_view stem)
{
    const auto now = std::chrono::system_clock::now();
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch())
            .count();

    std::ostringstream oss;
    oss << stem << '_' << timestamp;
    return oss.str();
}

struct TestWorkspace
{
    fs::path dir;
    fs::path shpPath;
    fs::path dbfPath;
    fs::path shxPath;
    fs::path dxfPath;

    explicit TestWorkspace(std::string_view stem)
    {
        dir = fs::current_path() / GenerateUniqueName(stem);
        fs::create_directories(dir);

        const fs::path base = dir / "case";
        shpPath = base;
        shpPath += ".shp";
        dbfPath = base;
        dbfPath += ".dbf";
        shxPath = base;
        shxPath += ".shx";
        dxfPath = base;
        dxfPath += ".dxf";
    }

    ~TestWorkspace()
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

void WritePointDataset(const TestWorkspace &workspace, bool withElevField,
                       double elevValue)
{
    SHPHandle shp = SHPCreate(workspace.shpPath.string().c_str(), SHPT_POINT);
    ASSERT_NE(nullptr, shp);

    const double x = 1.25;
    const double y = 2.5;
    SHPObject *shape = SHPCreateSimpleObject(SHPT_POINT, 1, &x, &y, nullptr);
    ASSERT_NE(nullptr, shape);
    EXPECT_EQ(0, SHPWriteObject(shp, -1, shape));
    SHPDestroyObject(shape);
    SHPClose(shp);

    DBFHandle dbf = DBFCreate(workspace.dbfPath.string().c_str());
    ASSERT_NE(nullptr, dbf);

    if (withElevField)
    {
        const int elevField = DBFAddField(dbf, "ELEV", FTDouble, 12, 3);
        ASSERT_GE(elevField, 0);
        EXPECT_TRUE(DBFWriteDoubleAttribute(dbf, 0, elevField, elevValue));
    }
    else
    {
        const int otherField = DBFAddField(dbf, "OTHER", FTDouble, 12, 3);
        ASSERT_GE(otherField, 0);
        EXPECT_TRUE(DBFWriteDoubleAttribute(dbf, 0, otherField, elevValue));
    }

    DBFClose(dbf);
}

void WritePointZDataset(const TestWorkspace &workspace, bool withElevField,
                        double geometryZ, double elevValue)
{
    SHPHandle shp = SHPCreate(workspace.shpPath.string().c_str(), SHPT_POINTZ);
    ASSERT_NE(nullptr, shp);

    const int parts[] = {0};
    const int partTypes[] = {SHPP_RING};
    const double x[] = {1.25};
    const double y[] = {2.5};
    const double z[] = {geometryZ};
    SHPObject *shape = SHPCreateObject(SHPT_POINTZ, -1, 1, parts, partTypes, 1,
                                       x, y, z, nullptr);
    ASSERT_NE(nullptr, shape);
    EXPECT_EQ(0, SHPWriteObject(shp, -1, shape));
    SHPDestroyObject(shape);
    SHPClose(shp);

    DBFHandle dbf = DBFCreate(workspace.dbfPath.string().c_str());
    ASSERT_NE(nullptr, dbf);

    if (withElevField)
    {
        const int elevField = DBFAddField(dbf, "ELEV", FTDouble, 12, 3);
        ASSERT_GE(elevField, 0);
        EXPECT_TRUE(DBFWriteDoubleAttribute(dbf, 0, elevField, elevValue));
    }
    else
    {
        const int otherField = DBFAddField(dbf, "OTHER", FTDouble, 12, 3);
        ASSERT_GE(otherField, 0);
        EXPECT_TRUE(DBFWriteDoubleAttribute(dbf, 0, otherField, elevValue));
    }

    DBFClose(dbf);
}

void WriteMultipartArcDataset(const TestWorkspace &workspace)
{
    SHPHandle shp = SHPCreate(workspace.shpPath.string().c_str(), SHPT_ARC);
    ASSERT_NE(nullptr, shp);

    const int parts[] = {0, 2};
    const int partTypes[] = {SHPP_RING, SHPP_RING};
    const double x[] = {0.0, 1.0, 10.0, 11.0};
    const double y[] = {0.0, 1.0, 10.0, 11.0};
    SHPObject *shape = SHPCreateObject(SHPT_ARC, -1, 2, parts, partTypes, 4, x,
                                       y, nullptr, nullptr);
    ASSERT_NE(nullptr, shape);
    EXPECT_EQ(0, SHPWriteObject(shp, -1, shape));
    SHPDestroyObject(shape);
    SHPClose(shp);

    DBFHandle dbf = DBFCreate(workspace.dbfPath.string().c_str());
    ASSERT_NE(nullptr, dbf);
    const int nameField = DBFAddField(dbf, "NAME", FTString, 16, 0);
    ASSERT_GE(nameField, 0);
    EXPECT_TRUE(DBFWriteStringAttribute(dbf, 0, nameField, "segment"));
    DBFClose(dbf);
}

void WritePolygonDataset(const TestWorkspace &workspace)
{
    SHPHandle shp = SHPCreate(workspace.shpPath.string().c_str(), SHPT_POLYGON);
    ASSERT_NE(nullptr, shp);

    const int parts[] = {0};
    const int partTypes[] = {SHPP_OUTERRING};
    const double x[] = {0.0, 0.0, 5.0, 5.0, 0.0};
    const double y[] = {0.0, 5.0, 5.0, 0.0, 0.0};
    SHPObject *shape = SHPCreateObject(SHPT_POLYGON, -1, 1, parts, partTypes, 5,
                                       x, y, nullptr, nullptr);
    ASSERT_NE(nullptr, shape);
    EXPECT_EQ(0, SHPWriteObject(shp, -1, shape));
    SHPDestroyObject(shape);
    SHPClose(shp);

    DBFHandle dbf = DBFCreate(workspace.dbfPath.string().c_str());
    ASSERT_NE(nullptr, dbf);
    const int nameField = DBFAddField(dbf, "NAME", FTString, 16, 0);
    ASSERT_GE(nameField, 0);
    EXPECT_TRUE(DBFWriteStringAttribute(dbf, 0, nameField, "polygon"));
    DBFClose(dbf);
}

std::string ReadFile(const fs::path &filename)
{
    std::ifstream file(filename, std::ios::binary);
    EXPECT_TRUE(file.is_open()) << filename.string();
    std::ostringstream contents;
    contents << file.rdbuf();

    std::string text = contents.str();
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

std::string RunConverter(const TestWorkspace &workspace)
{
    const std::string shpArg =
        fs::relative(workspace.shpPath, fs::current_path()).generic_string();
    std::vector<char> arg0{'s', 'h', 'p', 'd', 'x', 'f', '\0'};
    std::vector<char> arg1(shpArg.begin(), shpArg.end());
    arg1.push_back('\0');
    char *argv[] = {arg0.data(), arg1.data()};

    EXPECT_EQ(0, shpdxf_run(2, argv));
    EXPECT_TRUE(fs::exists(workspace.dxfPath));
    return ReadFile(workspace.dxfPath);
}

size_t CountOccurrences(const std::string &text, std::string_view needle)
{
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos)
    {
        count++;
        pos += needle.size();
    }
    return count;
}

TEST(SHPDxfTest, UsesElevFieldWhenPresent)
{
    TestWorkspace workspace{"shpdxf_elev"};
    WritePointDataset(workspace, true, 123.25);

    const std::string dxf = RunConverter(workspace);
    EXPECT_NE(std::string::npos, dxf.find("123.25000")) << dxf;
}

TEST(SHPDxfTest, FallsBackToZeroWithoutElevField)
{
    TestWorkspace workspace{"shpdxf_noelev"};
    WritePointDataset(workspace, false, 987.5);

    const std::string dxf = RunConverter(workspace);
    EXPECT_NE(std::string::npos, dxf.find("POINT")) << dxf;
    EXPECT_EQ(std::string::npos, dxf.find("987.50000")) << dxf;
}

TEST(SHPDxfTest, PrefersGeometryZOverElevField)
{
    TestWorkspace workspace{"shpdxf_pointz"};
    WritePointZDataset(workspace, true, 321.5, 123.25);

    const std::string dxf = RunConverter(workspace);
    EXPECT_NE(std::string::npos, dxf.find("321.50000")) << dxf;
    EXPECT_EQ(std::string::npos, dxf.find("123.25000")) << dxf;
}

TEST(SHPDxfTest, WritesMultipartPolylineAsSeparatePolylines)
{
    TestWorkspace workspace{"shpdxf_arc"};
    WriteMultipartArcDataset(workspace);

    const std::string dxf = RunConverter(workspace);
    EXPECT_EQ(2u, CountOccurrences(dxf, "POLYLINE")) << dxf;
    EXPECT_EQ(4u, CountOccurrences(dxf, "VERTEX")) << dxf;
    EXPECT_EQ(2u, CountOccurrences(dxf, "SEQEND")) << dxf;
}

TEST(SHPDxfTest, WritesPolygonAsClosedPolyline)
{
    TestWorkspace workspace{"shpdxf_polygon"};
    WritePolygonDataset(workspace);

    const std::string dxf = RunConverter(workspace);
    EXPECT_EQ(1u, CountOccurrences(dxf, "POLYLINE")) << dxf;
    EXPECT_EQ(5u, CountOccurrences(dxf, "VERTEX")) << dxf;
    EXPECT_EQ(1u, CountOccurrences(dxf, "SEQEND")) << dxf;
    EXPECT_NE(std::string::npos, dxf.find("\n 70\n1\n")) << dxf;
}

}  // namespace

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}