#include "gtest/gtest.h"
#include "../code/SharedMemoryStruct.h"
#include <fstream>
#include <ctime>
#include <iostream>

class LocalDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 测试文件路径
        dataFilePath = "test_data.txt";
        indexedDataFilePath = "test_indexed_data.txt";
        cycleFilePath = "test_cycle_data.txt";
    }

    void TearDown() override {
        // 清理：删除测试过程中创建的文件
        // std::remove(dataFilePath.c_str());
        // std::remove(indexedDataFilePath.c_str());
        // std::remove(cycleFilePath.c_str());
    }

    std::string dataFilePath;
    std::string indexedDataFilePath;
    std::string cycleFilePath;
};

// 测试保存数据到文件
TEST_F(LocalDataTest, SaveDataToFile) {
    // 创建测试数据
    EMP::LocalData data("test_force", "test_mesh");
    data.isFieldData = true;
    data.type = EMP::DataGeoType::VertexData;
    data.t = 1.0;
    data.sysTimeStamp = time(nullptr);

    // 添加几何位置信息
    data.dimtags.push_back(std::make_pair(3, 1));
    data.dimtags.push_back(std::make_pair(3, 2));

    // 添加分量数据
    data.addComponent("fx", { 1.2, 2.3, 3.4, 4.5, 5.6 }, "N");
    data.addComponent("fy", { 6.7, 7.8, 8.9, 9.0, 10.1 }, "N");
    data.addComponent("fz", { 11.2, 12.3, 13.4, 14.5, 15.6 }, "N");

    // 顺序匹配，索引可以为空或匹配位置
    data.index = { 1, 2, 3, 4, 5 };

    // 保存到文件
    bool result = data.saveToFile(dataFilePath);
    EXPECT_TRUE(result);

    // 验证文件存在
    std::ifstream file(dataFilePath);
    EXPECT_TRUE(file.good());
    file.close();
}

// 测试保存带索引的数据到文件
TEST_F(LocalDataTest, SaveIndexedDataToFile) {
    // 创建测试数据
    EMP::LocalData data("test_force_non_seq", "test_mesh");
    data.isFieldData = true;
    data.type = EMP::DataGeoType::EdgeData;
    data.t = 2.0;
    data.sysTimeStamp = time(nullptr);

    // 添加分量数据
    data.addComponent("pressure", { 10.1, 20.2, 30.3 }, "Pa");
    data.addComponent("temperature", { 100.5, 200.6, 300.7 }, "K");

    // 添加非顺序索引
    data.index = { 5, 8, 12 };

    // 保存到文件
    bool result = data.saveToFile(indexedDataFilePath);
    EXPECT_TRUE(result);

    // 验证文件存在
    std::ifstream file(indexedDataFilePath);
    EXPECT_TRUE(file.good());
    file.close();
}

// 测试从文件加载数据
TEST_F(LocalDataTest, LoadDataFromFile) {
    // 首先创建并保存测试数据
    {
        EMP::LocalData data("test_force", "test_mesh");
        data.isFieldData = true;
        data.type = EMP::DataGeoType::VertexData;
        data.t = 1.0;
        data.sysTimeStamp = time(nullptr);
        data.dimtags.push_back(std::make_pair(3, 1));

        // 添加索引
        data.index = { 1, 2, 3, 4, 5 };

        // 添加分量数据
        data.addComponent("x", { 10.2, 0.3, 30.4, 14.5, 5.6 }, "m");
        data.addComponent("y", { 1.1, 2.2, 3.3, 4.4, 5.5 }, "m");

        data.saveToFile(dataFilePath);
    }

    // 从文件加载数据
    EMP::LocalData loadedData;
    bool result = loadedData.loadFromFile(dataFilePath);
    EXPECT_TRUE(result);

    // 验证加载的数据
    EXPECT_EQ("test_force", loadedData.name);
    EXPECT_EQ("test_mesh", loadedData.meshName);
    EXPECT_TRUE(loadedData.isFieldData);
    EXPECT_EQ(EMP::DataGeoType::VertexData, loadedData.type);
    EXPECT_DOUBLE_EQ(1.0, loadedData.t);

    // 验证dimtags
    ASSERT_EQ(1, loadedData.dimtags.size());
    EXPECT_EQ(3, loadedData.dimtags[0].first);
    EXPECT_EQ(1, loadedData.dimtags[0].second);

    // 验证分量数据
    ASSERT_EQ(2, loadedData.titles.size());
    EXPECT_EQ("x", loadedData.titles[0]);
    EXPECT_EQ("y", loadedData.titles[1]);

    // 验证分量单位
    ASSERT_EQ(2, loadedData.units.size());
    EXPECT_EQ("m", loadedData.units[0]);
    EXPECT_EQ("m", loadedData.units[1]);

    // 验证数据
    ASSERT_EQ(2, loadedData.data.size());
    ASSERT_EQ(5, loadedData.data[0].size());
    ASSERT_EQ(5, loadedData.data[1].size());

    // 验证第一个分量的数据
    EXPECT_DOUBLE_EQ(10.2, loadedData.data[0][0]);
    EXPECT_DOUBLE_EQ(0.3, loadedData.data[0][1]);
    EXPECT_DOUBLE_EQ(30.4, loadedData.data[0][2]);
    EXPECT_DOUBLE_EQ(14.5, loadedData.data[0][3]);
    EXPECT_DOUBLE_EQ(5.6, loadedData.data[0][4]);

    // 验证第二个分量的数据
    EXPECT_DOUBLE_EQ(1.1, loadedData.data[1][0]);
    EXPECT_DOUBLE_EQ(2.2, loadedData.data[1][1]);
    EXPECT_DOUBLE_EQ(3.3, loadedData.data[1][2]);
    EXPECT_DOUBLE_EQ(4.4, loadedData.data[1][3]);
    EXPECT_DOUBLE_EQ(5.5, loadedData.data[1][4]);
}

// 测试从文件加载带索引的数据
TEST_F(LocalDataTest, LoadIndexedDataFromFile) {
    // 首先创建并保存测试数据
    {
        EMP::LocalData data("test_force_non_seq", "test_mesh");
        data.isFieldData = true;
        data.type = EMP::DataGeoType::EdgeData;
        data.t = 2.0;
        data.sysTimeStamp = time(nullptr);

        // 添加分量数据
        data.addComponent("pressure", { 10.1, 20.2, 30.3 }, "Pa");
        data.addComponent("temperature", { 100.5, 200.6, 300.7 }, "K");

        // 添加非顺序索引
        data.index = { 5, 8, 12 };

        data.saveToFile(indexedDataFilePath);
    }

    // 从文件加载数据
    EMP::LocalData loadedData;
    bool result = loadedData.loadFromFile(indexedDataFilePath);
    EXPECT_TRUE(result);

    // 验证加载的数据
    EXPECT_EQ("test_force_non_seq", loadedData.name);
    EXPECT_EQ("test_mesh", loadedData.meshName);
    EXPECT_TRUE(loadedData.isFieldData);
    EXPECT_EQ(EMP::DataGeoType::EdgeData, loadedData.type);
    EXPECT_DOUBLE_EQ(2.0, loadedData.t);

    // 验证分量数据
    ASSERT_EQ(2, loadedData.titles.size());
    EXPECT_EQ("pressure", loadedData.titles[0]);
    EXPECT_EQ("temperature", loadedData.titles[1]);

    // 验证分量单位
    ASSERT_EQ(2, loadedData.units.size());
    EXPECT_EQ("Pa", loadedData.units[0]);
    EXPECT_EQ("K", loadedData.units[1]);

    // 验证数据和索引
    ASSERT_EQ(2, loadedData.data.size());
    ASSERT_EQ(3, loadedData.data[0].size());
    ASSERT_EQ(3, loadedData.data[1].size());
    ASSERT_EQ(3, loadedData.index.size());

    // 验证索引
    EXPECT_EQ(5, loadedData.index[0]);
    EXPECT_EQ(8, loadedData.index[1]);
    EXPECT_EQ(12, loadedData.index[2]);

    // 验证第一个分量的数据
    EXPECT_DOUBLE_EQ(10.1, loadedData.data[0][0]);
    EXPECT_DOUBLE_EQ(20.2, loadedData.data[0][1]);
    EXPECT_DOUBLE_EQ(30.3, loadedData.data[0][2]);

    // 验证第二个分量的数据
    EXPECT_DOUBLE_EQ(100.5, loadedData.data[1][0]);
    EXPECT_DOUBLE_EQ(200.6, loadedData.data[1][1]);
    EXPECT_DOUBLE_EQ(300.7, loadedData.data[1][2]);
}

// 测试文件不存在的情况
TEST_F(LocalDataTest, LoadNonExistentFile) {
    EMP::LocalData data;
    bool result = data.loadFromFile("non_existent_file.txt");
    EXPECT_FALSE(result);
}

// 测试完整的保存和加载周期
TEST_F(LocalDataTest, SaveAndLoadCycle) {
    // 创建测试数据
    EMP::LocalData originalData("cycle_test", "cycle_mesh");
    originalData.isFieldData = false; // 全局数据
    originalData.type = EMP::DataGeoType::FacetData;
    originalData.t = 3.14;
    originalData.sysTimeStamp = time(nullptr);
    originalData.dimtags.push_back(std::make_pair(2, 7));
    originalData.dimtags.push_back(std::make_pair(2, 8));

    // 添加索引
    originalData.index = { 1, 2, 3, 4 };

    // 添加多分量数据
    originalData.addComponent("displacement", { 99.1, 88.2, 77.3, 66.4 }, "mm");
    originalData.addComponent("velocity", { 1.1, 2.2, 3.3, 4.4 }, "mm/s");
    originalData.addComponent("acceleration", { 0.1, 0.2, 0.3, 0.4 }, "mm/s^2");

    // 保存到文件
    EXPECT_TRUE(originalData.saveToFile(cycleFilePath));

    // 加载到新对象
    EMP::LocalData loadedData;
    EXPECT_TRUE(loadedData.loadFromFile(cycleFilePath));

    // 验证数据一致性
    EXPECT_EQ(originalData.name, loadedData.name);
    EXPECT_EQ(originalData.meshName, loadedData.meshName);
    EXPECT_EQ(originalData.isFieldData, loadedData.isFieldData);
    EXPECT_EQ(originalData.type, loadedData.type);
    EXPECT_DOUBLE_EQ(originalData.t, loadedData.t);

    // 验证dimtags
    ASSERT_EQ(originalData.dimtags.size(), loadedData.dimtags.size());
    for (size_t i = 0; i < originalData.dimtags.size(); i++) {
        EXPECT_EQ(originalData.dimtags[i].first, loadedData.dimtags[i].first);
        EXPECT_EQ(originalData.dimtags[i].second, loadedData.dimtags[i].second);
    }

    // 验证分量标题和单位
    ASSERT_EQ(originalData.titles.size(), loadedData.titles.size());
    ASSERT_EQ(originalData.units.size(), loadedData.units.size());
    for (size_t i = 0; i < originalData.titles.size(); i++) {
        EXPECT_EQ(originalData.titles[i], loadedData.titles[i]);
        EXPECT_EQ(originalData.units[i], loadedData.units[i]);
    }

    // 验证数据
    ASSERT_EQ(originalData.data.size(), loadedData.data.size());
    for (size_t i = 0; i < originalData.data.size(); i++) {
        ASSERT_EQ(originalData.data[i].size(), loadedData.data[i].size());
        for (size_t j = 0; j < originalData.data[i].size(); j++) {
            EXPECT_DOUBLE_EQ(originalData.data[i][j], loadedData.data[i][j]);
        }
    }
}

// 测试分量管理功能
TEST_F(LocalDataTest, ComponentManagement) {
    // 创建测试数据
    EMP::LocalData data("component_test", "test_mesh");

    // 添加分量
    data.addComponent("x", { 1.0, 2.0, 3.0 }, "m");
    data.addComponent("y", { 4.0, 5.0, 6.0 }, "m");
    data.addComponent("z", { 7.0, 8.0, 9.0 }, "m");

    // 验证分量数量
    EXPECT_EQ(3, data.getComponentCount());

    // 验证分量索引
    EXPECT_EQ(0, data.getComponentIndex("x"));
    EXPECT_EQ(1, data.getComponentIndex("y"));
    EXPECT_EQ(2, data.getComponentIndex("z"));
    EXPECT_EQ(static_cast<size_t>(-1), data.getComponentIndex("w")); // 不存在的分量

    // 验证获取分量数据
    std::vector<double> xData = data.getComponent("x");
    std::vector<double> yData = data.getComponent("y");
    std::vector<double> zData = data.getComponent("z");
    std::vector<double> wData = data.getComponent("w"); // 不存在的分量

    // 验证分量数据
    ASSERT_EQ(3, xData.size());
    EXPECT_DOUBLE_EQ(1.0, xData[0]);
    EXPECT_DOUBLE_EQ(2.0, xData[1]);
    EXPECT_DOUBLE_EQ(3.0, xData[2]);

    ASSERT_EQ(3, yData.size());
    EXPECT_DOUBLE_EQ(4.0, yData[0]);
    EXPECT_DOUBLE_EQ(5.0, yData[1]);
    EXPECT_DOUBLE_EQ(6.0, yData[2]);

    ASSERT_EQ(3, zData.size());
    EXPECT_DOUBLE_EQ(7.0, zData[0]);
    EXPECT_DOUBLE_EQ(8.0, zData[1]);
    EXPECT_DOUBLE_EQ(9.0, zData[2]);

    // 验证不存在的分量返回空向量
    EXPECT_TRUE(wData.empty());

    // 测试替换分量数据
    data.addComponent("x", { 10.0, 20.0, 30.0 }, "cm"); // 替换已存在的分量

    // 验证分量数量保持不变
    EXPECT_EQ(3, data.getComponentCount());

    // 验证分量数据已更新
    xData = data.getComponent("x");
    ASSERT_EQ(3, xData.size());
    EXPECT_DOUBLE_EQ(10.0, xData[0]);
    EXPECT_DOUBLE_EQ(20.0, xData[1]);
    EXPECT_DOUBLE_EQ(30.0, xData[2]);

    // 验证单位已更新
    EXPECT_EQ("cm", data.units[0]);
}

// 测试科学计数法输出
TEST_F(LocalDataTest, ScientificNotation) {
    // 创建测试数据
    std::string scientificFilePath = "scientific_test.txt";
    EMP::LocalData data("scientific_test", "test_mesh");
    data.isFieldData = true;
    data.type = EMP::DataGeoType::VertexData;
    data.t = 0.00000123456789; // 非常小的数
    data.sysTimeStamp = time(nullptr);

    // 添加分量数据，不同量级
    data.addComponent("small", { 0.00000000123, 0.00000000456, 0.00000000789 }, "m");
    data.addComponent("medium", { 123.456789, 456.789123, 789.123456 }, "m");
    data.addComponent("large", { 1234567890.12, 4567891234.56, 7891234567.89 }, "m");

    // 添加索引
    data.index = { 1, 2, 3 };

    // 保存到文件
    bool result = data.saveToFile(scientificFilePath);
    EXPECT_TRUE(result);

    // 验证文件存在
    std::ifstream file(scientificFilePath);
    EXPECT_TRUE(file.good());
    file.close();

    // 从文件加载数据
    EMP::LocalData loadedData;
    EXPECT_TRUE(loadedData.loadFromFile(scientificFilePath));

    // 验证加载的数据
    EXPECT_EQ("scientific_test", loadedData.name);
    EXPECT_EQ("test_mesh", loadedData.meshName);
    EXPECT_TRUE(loadedData.isFieldData);
    EXPECT_EQ(EMP::DataGeoType::VertexData, loadedData.type);
    EXPECT_DOUBLE_EQ(0.00000123456789, loadedData.t);

    // 验证分量数据
    ASSERT_EQ(3, loadedData.titles.size());
    EXPECT_EQ("small", loadedData.titles[0]);
    EXPECT_EQ("medium", loadedData.titles[1]);
    EXPECT_EQ("large", loadedData.titles[2]);

    // 验证小数据
    ASSERT_EQ(3, loadedData.data[0].size());
    EXPECT_DOUBLE_EQ(0.00000000123, loadedData.data[0][0]);
    EXPECT_DOUBLE_EQ(0.00000000456, loadedData.data[0][1]);
    EXPECT_DOUBLE_EQ(0.00000000789, loadedData.data[0][2]);

    // 验证中等数据
    ASSERT_EQ(3, loadedData.data[1].size());
    EXPECT_DOUBLE_EQ(123.456789, loadedData.data[1][0]);
    EXPECT_DOUBLE_EQ(456.789123, loadedData.data[1][1]);
    EXPECT_DOUBLE_EQ(789.123456, loadedData.data[1][2]);

    // 验证大数据
    ASSERT_EQ(3, loadedData.data[2].size());
    EXPECT_DOUBLE_EQ(1234567890.12, loadedData.data[2][0]);
    EXPECT_DOUBLE_EQ(4567891234.56, loadedData.data[2][1]);
    EXPECT_DOUBLE_EQ(7891234567.89, loadedData.data[2][2]);

    // 清理测试文件
    // std::remove(scientificFilePath.c_str());
}