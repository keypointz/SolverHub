#include "SharedMemoryStruct.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <ctime>

namespace EMP {
    //================ LocalDataBase 实现 ================
    LocalDataBase::LocalDataBase()
        : sysTimeStamp(std::time(nullptr)),
        version(0),
        dataType(DataType::UNKNOWN_DATA)
    {
    }

    LocalDataBase::LocalDataBase(const std::string& name_, DataType dataType_)
        : name(name_),
        sysTimeStamp(std::time(nullptr)),
        version(0),
        dataType(dataType_)
    {
    }

    std::string LocalDataBase::getName() const {
        return name;
    }

    uint64_t LocalDataBase::getVersion() const {
        return version;
    }

    time_t LocalDataBase::getTimeStamp() const {
        return sysTimeStamp;
    }

    DataType LocalDataBase::getDataType() const {
        return dataType;
    }

    void LocalDataBase::setDataType(DataType type) {
        dataType = type;
    }

    //================ SharedDataBase 实现 ================
    SharedDataBase::SharedDataBase(bip::managed_shared_memory::segment_manager* segment_manager, DataType dataType_)
        : name(SharedMemoryAllocator<char>(segment_manager)),
        dataType(dataType_)
    {
        version.store(0);
        writing.store(false);
        dataRead.store(true); // 初始设为已读
        sysTimeStamp = std::time(nullptr);
    }

    SharedDataBase::SharedDataBase(const SharedDataBase& other)
        : version(other.version.load()),
        writing(other.writing.load()),
        dataRead(other.dataRead.load()),
        sysTimeStamp(other.sysTimeStamp),
        name(other.name),
        dataType(other.dataType)
    {
    }

    SharedDataBase& SharedDataBase::operator=(const SharedDataBase& other)
    {
        version.store(other.version.load());
        writing.store(other.writing.load());
        dataRead.store(other.dataRead.load());
        sysTimeStamp = other.sysTimeStamp;
        name = other.name;
        dataType = other.dataType;
        return *this;
    }

    DataType SharedDataBase::getDataType() const {
        return dataType;
    }

    void SharedDataBase::setDataType(DataType type) {
        dataType = type;
    }

    //================ MeshInfo 实现 ================
    MeshInfo::MeshInfo() {
        myNbNodes = 0;
        myNbEdges = 0;
        myNbTriangles = 0;
        myNbTetras = 0;
    }

    MeshInfo::MeshInfo(const MeshInfo& other) {
        myNbNodes = other.myNbNodes;
        myNbEdges = other.myNbEdges;
        myNbTriangles = other.myNbTriangles;
        myNbTetras = other.myNbTetras;
    }

    MeshInfo& MeshInfo::operator=(const MeshInfo& other) {
        myNbNodes = other.myNbNodes;
        myNbEdges = other.myNbEdges;
        myNbTriangles = other.myNbTriangles;
        myNbTetras = other.myNbTetras;
        return *this;
    }

    //================ LocalDefinitionList 实现 ================
    LocalDefinitionList::LocalDefinitionList()
        : LocalDataBase()
    {
        setDataType(DataType::DEFINITION_DATA);
    }

    LocalDefinitionList::LocalDefinitionList(const std::string& name_, const std::string& description_)
        : LocalDataBase(name_),
        description(description_)
    {
        setDataType(DataType::DEFINITION_DATA);
    }

    void LocalDefinitionList::addDefinition(const Definition& def) {
        definitions.push_back(def);
    }

    Definition* LocalDefinitionList::findDefinitionById(int id) {
        for (auto& def : definitions) {
            if (def.id == id) {
                return &def;
            }
        }
        return nullptr;
    }

    size_t LocalDefinitionList::getDefinitionCount() const {
        return definitions.size();
    }

    DataType LocalDefinitionList::getDataType() const {
        return DataType::DEFINITION_DATA;
    }

    //================ LocalGeometry 实现 ================
    LocalGeometry::LocalGeometry()
        : LocalDataBase()
    {
        setDataType(DataType::GEOMETRY_DATA);
    }

    LocalGeometry::LocalGeometry(const std::string& name_, const std::string& shapeBrp_)
        : LocalDataBase(name_, DataType::GEOMETRY_DATA)
    {
        shapeNames.push_back(name_);
        shapeBrps.push_back(shapeBrp_);
    }

    void LocalGeometry::addGeometry(const std::string& name_, const std::string& shapeBrp_) {
        // 检查是否已存在相同名称的几何体
        for (size_t i = 0; i < shapeNames.size(); i++) {
            if (shapeNames[i] == name_) {
                // 更新已存在的几何体
                shapeBrps[i] = shapeBrp_;
                return;
            }
        }

        // 添加新的几何体
        shapeNames.push_back(name_);
        shapeBrps.push_back(shapeBrp_);
    }

    std::string LocalGeometry::getShapeBrpByName(const std::string& name_) const {
        for (size_t i = 0; i < shapeNames.size(); i++) {
            if (shapeNames[i] == name_) {
                return shapeBrps[i];
            }
        }
        return "";
    }

    size_t LocalGeometry::getGeometryCount() const {
        return shapeNames.size();
    }

    std::string LocalGeometry::getPrimaryName() const {
        return shapeNames.empty() ? "" : shapeNames[0];
    }

    std::string LocalGeometry::getPrimaryShapeBrp() const {
        return shapeBrps.empty() ? "" : shapeBrps[0];
    }

    DataType LocalGeometry::getDataType() const {
        return DataType::GEOMETRY_DATA;
    }

    //================ LocalMesh 实现 ================
    LocalMesh::LocalMesh()
        : LocalDataBase()
    {
        setDataType(DataType::MESH_DATA);
    }

    LocalMesh::LocalMesh(const std::string& name_, const std::string& modelName_)
        : LocalDataBase(name_),
        modelName(modelName_)
    {
        setDataType(DataType::MESH_DATA);
    }

    DataType LocalMesh::getDataType() const {
        return DataType::MESH_DATA;
    }

    //================ LocalData 实现 ================
    LocalData::LocalData()
        : LocalDataBase(),
        isFieldData(true),
        type(VertexData),
        isSequentiallyMatchedWithMesh(true),
        t(0.0)
    {
        setDataType(DataType::CALCULATION_DATA);
    }

    LocalData::LocalData(const std::string& name_, const std::string& meshName_)
        : LocalDataBase(name_),
        meshName(meshName_),
        isFieldData(true),
        type(VertexData),
        isSequentiallyMatchedWithMesh(true),
        t(0.0)
    {
        setDataType(DataType::CALCULATION_DATA);
    }

    DataType LocalData::getDataType() const {
        return DataType::CALCULATION_DATA;
    }

    bool LocalData::saveToFile(const std::string& filePath) const {
        try {
            std::ofstream file(filePath);
            if (!file.is_open()) {
                return false;
            }

            // 写入版本信息
            file << "# Default format version\n";
            file << "VERSION {V2.0}\n\n";

            // 写入名称
            file << "# name: {name}\n";
            file << "NAME {" << name << "}\n\n";

            // 写入网格名
            file << "# meshName: {meshName}\n";
            file << "MESHNAME {" << meshName << "} \n\n";

            // 如果有dimtags，写入dimtags
            if (!dimtags.empty()) {
                file << "# dimtags (dim, tag)\n";
                file << "DIMTAG {";
                for (size_t i = 0; i < dimtags.size(); ++i) {
                    file << "(" << dimtags[i].first << "," << dimtags[i].second << ")";
                    if (i < dimtags.size() - 1) file << ",";
                }
                file << "}\n\n";
            }

            // 写入数据类型（场数据或全局数据）
            file << "# Field/Global Data\n";
            file << "DTYPE {" << (isFieldData ? "Field" : "Global") << "}\n\n";

            // 写入数据几何类型
            file << "# DataGeoType: {NodalData/EdgeData/FacetData/BlockData}\n";
            const char* typeStr = "";
            switch (type) {
                case VertexData: typeStr = "NODAL"; break;
                case EdgeData:   typeStr = "EDGE"; break;
                case FacetData:  typeStr = "FACET"; break;
                case BlockData:  typeStr = "BLOCK"; break;
            }
            file << "VTYPE {" << typeStr << "}\n\n";

            // 写入时间值
            file << "# simulation time value\n";
            file << "t {" << t << "}\n\n";

            // 写入系统时间戳
            file << "# sysTimeStamp: \n";
            char timeBuffer[100];
            struct tm* timeinfo = localtime(&sysTimeStamp);
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeinfo);
            file << "CLOCK {" << timeBuffer << "}\n\n";

            // 写入数据向量大小
            file << "# data vector size\n";
            file << "NrDOF {" << data.size() << "}\n\n";

            // 写入数据
            if (isSequentiallyMatchedWithMesh) {
                file << "# isSequentiallyMatchedWithMesh == true, only output data\n";
                for (const auto& val : data) {
                    file << val << "\n";
                }
            } else {
                file << "# isSequentiallyMatchedWithMesh == false, output data with index\n";
                for (size_t i = 0; i < data.size(); ++i) {
                    file << index[i] << " " << data[i] << "\n";
                }
            }

            file.close();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error in saveToFile: " << e.what() << std::endl;
            return false;
        }
    }

    bool LocalData::loadFromFile(const std::string& filePath) {
        try {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                return false;
            }

            std::string line, key, value;

            // 清空现有数据
            dimtags.clear();
            index.clear();
            data.clear();

            // 标记是否已经设置了isSequentiallyMatchedWithMesh
            bool formatExplicitlySet = false;

            // 解析文件头部信息
            while (std::getline(file, line)) {
                // 跳过注释行和空行
                if (line.empty() || line[0] == '#') continue;

                // 解析键值对
                size_t startPos = line.find('{');
                size_t endPos = line.find('}');

                if (startPos != std::string::npos && endPos != std::string::npos) {
                    key = line.substr(0, startPos);
                    value = line.substr(startPos + 1, endPos - startPos - 1);

                    // 去除键中的空格
                    key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());

                    if (key == "NAME") {
                        name = value;
                    } else if (key == "MESHNAME") {
                        meshName = value;
                    } else if (key == "DTYPE") {
                        isFieldData = (value == "Field");
                    } else if (key == "VTYPE") {
                        if (value == "NODAL") type = VertexData;
                        else if (value == "EDGE") type = EdgeData;
                        else if (value == "FACET") type = FacetData;
                        else if (value == "BLOCK") type = BlockData;
                    } else if (key == "t") {
                        t = std::stod(value);
                    } else if (key == "CLOCK") {
                        // 时间戳处理，这里采用Windows兼容的方式
                        struct tm tm = {};
                        int year, month, day, hour, min, sec;
                        if (sscanf(value.c_str(), "%d-%d-%d %d:%d:%d",
                                  &year, &month, &day, &hour, &min, &sec) == 6) {
                            tm.tm_year = year - 1900;  // 年份需要减去1900
                            tm.tm_mon = month - 1;     // 月份从0开始
                            tm.tm_mday = day;
                            tm.tm_hour = hour;
                            tm.tm_min = min;
                            tm.tm_sec = sec;
                            sysTimeStamp = mktime(&tm);
                        }
                    } else if (key == "DIMTAG") {
                        // 解析 dimtags, 格式如 "(3,2),(3,3),(3,4)"
                        std::string dimtagStr = value;
                        size_t pos = 0;
                        while ((pos = dimtagStr.find('(', pos)) != std::string::npos) {
                            size_t endPos = dimtagStr.find(')', pos);
                            if (endPos == std::string::npos) break;

                            std::string dimtagPair = dimtagStr.substr(pos + 1, endPos - pos - 1);
                            size_t commaPos = dimtagPair.find(',');
                            if (commaPos != std::string::npos) {
                                int dim = std::stoi(dimtagPair.substr(0, commaPos));
                                int tag = std::stoi(dimtagPair.substr(commaPos + 1));
                                dimtags.push_back(std::make_pair(dim, tag));
                            }
                            pos = endPos + 1;
                        }
                    } else if (key == "NrDOF") {
                        int size = std::stoi(value);
                        data.reserve(size);
                        index.reserve(size);
                    } else if (key == "VERSION") {
                        // 版本检查
                        if (value != "V2.0") {
                            std::cerr << "Warning: Unsupported file version: " << value << std::endl;
                        }
                    }
                } else if (line.find("isSequentiallyMatchedWithMesh == true") != std::string::npos) {
                    isSequentiallyMatchedWithMesh = true;
                    formatExplicitlySet = true;

                    // 读取数据值
                    double val;
                    while (file >> val) {
                        data.push_back(val);
                        index.push_back(data.size()); // 自动生成索引
                    }
                } else if (line.find("isSequentiallyMatchedWithMesh == false") != std::string::npos) {
                    isSequentiallyMatchedWithMesh = false;
                    formatExplicitlySet = true;

                    // 读取索引和数据值对
                    int idx;
                    double val;
                    while (file >> idx >> val) {
                        index.push_back(idx);
                        data.push_back(val);
                    }
                } else {
                    // 如果没有明确指定格式，尝试自动检测
                    if (!formatExplicitlySet) {
                        // 保存当前文件位置，以便稍后恢复
                        std::streampos currentPos = file.tellg();

                        // 尝试读取一行数据来判断格式
                        std::istringstream iss(line);
                        int idx;
                        double val;

                        // 尝试读取两个值（索引和数据）
                        if (iss >> idx >> val) {
                            // 成功读取两个值，说明是索引-值对格式
                            isSequentiallyMatchedWithMesh = false;
                            formatExplicitlySet = true;

                            // 处理当前行
                            index.push_back(idx);
                            data.push_back(val);

                            // 继续读取剩余的索引-值对
                            while (file >> idx >> val) {
                                index.push_back(idx);
                                data.push_back(val);
                            }
                        } else {
                            // 尝试读取单个值
                            iss.clear(); // 清除错误标志
                            iss.seekg(0); // 回到字符串开头

                            if (iss >> val) {
                                // 成功读取单个值，说明是纯数据格式
                                isSequentiallyMatchedWithMesh = true;
                                formatExplicitlySet = true;

                                // 处理当前行
                                data.push_back(val);
                                index.push_back(data.size()); // 自动生成索引

                                // 继续读取剩余的数据值
                                while (file >> val) {
                                    data.push_back(val);
                                    index.push_back(data.size()); // 自动生成索引
                                }
                            } else {
                                // 无法识别的格式，恢复文件位置继续解析
                                file.seekg(currentPos);
                            }
                        }
                    }
                }
            }

            file.close();

            // 设置正确的数据类型
            setDataType(DataType::CALCULATION_DATA);

            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error in loadFromFile: " << e.what() << std::endl;
            return false;
        }
    }

    //================ LocalControlData 实现 ================
    LocalControlData::LocalControlData()
        : LocalDataBase(),
        dt(0.01),
        t(0.0),
        isConverged(false),
        geometrySegmentTotalSize(0),
        geometrySegmentFreeSize(0),
        meshSegmentTotalSize(0),
        meshSegmentFreeSize(0),
        dataSegmentTotalSize(0),
        dataSegmentFreeSize(0),
        controlSegmentTotalSize(0),
        controlSegmentFreeSize(0),
        definitionSegmentTotalSize(0),
        definitionSegmentFreeSize(0)
    {
        setDataType(DataType::CONTROL_DATA);
    }

    LocalControlData::LocalControlData(const std::string& name_, const std::string& jsonConfig_)
        : LocalDataBase(name_),
        jsonConfig(jsonConfig_),
        dt(0.01),
        t(0.0),
        isConverged(false),
        geometrySegmentTotalSize(0),
        geometrySegmentFreeSize(0),
        meshSegmentTotalSize(0),
        meshSegmentFreeSize(0),
        dataSegmentTotalSize(0),
        dataSegmentFreeSize(0),
        controlSegmentTotalSize(0),
        controlSegmentFreeSize(0),
        definitionSegmentTotalSize(0),
        definitionSegmentFreeSize(0)
    {
        setDataType(DataType::CONTROL_DATA);
    }

    DataType LocalControlData::getDataType() const {
        return DataType::CONTROL_DATA;
    }

    //================ SharedException 实现 ================
    SharedException::SharedException(bip::managed_shared_memory::segment_manager* segment_manager)
        : exceptionMessage(SharedMemoryAllocator<char>(segment_manager))
    {
        // Initialize atomic types
        hasException.store(false);
        exceptionType.store(0);
        exceptionCode.store(0);
    }

    SharedException::SharedException(const SharedException& other)
        : hasException(other.hasException.load()),
        exceptionType(other.exceptionType.load()),
        exceptionCode(other.exceptionCode.load()),
        exceptionMessage(other.exceptionMessage)
    {
    }

    SharedException& SharedException::operator=(const SharedException& other)
    {
        hasException.store(other.hasException.load());
        exceptionType.store(other.exceptionType.load());
        exceptionCode.store(other.exceptionCode.load());
        exceptionMessage = other.exceptionMessage;
        return *this;
    }

    //================ SharedGeometry 实现 ================
    SharedGeometry::SharedGeometry(boost::interprocess::managed_shared_memory::segment_manager *segment_manager)
        : SharedDataBase(segment_manager, DataType::GEOMETRY_DATA),
          shapeNames(SharedMemoryAllocator<SharedMemoryString>(segment_manager)),
          shapeBrps(SharedMemoryAllocator<SharedMemoryString>(segment_manager))
    {
    }

    SharedGeometry::SharedGeometry(const SharedGeometry& other)
        : SharedDataBase(other),
        shapeNames(other.shapeNames),
        shapeBrps(other.shapeBrps)
    {
    }

    SharedGeometry& SharedGeometry::operator=(const SharedGeometry& other)
    {
        SharedDataBase::operator=(other);
        shapeNames = other.shapeNames;
        shapeBrps = other.shapeBrps;
        return *this;
    }

    void SharedGeometry::copyFromLocal(const LocalGeometry& local, SharedMemoryAllocator<char> allocator)
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        writing.store(true);
        version.store(version.load() + 1);

        // 设置基础属性
        name = SharedMemoryString(local.name.c_str(), allocator);
        sysTimeStamp = local.sysTimeStamp;

        // 处理所有几何体数据
        shapeNames.clear();
        shapeBrps.clear();

        for (size_t i = 0; i < local.shapeNames.size(); i++) {
            const std::string& localName = local.shapeNames[i];
            const std::string& localShapeBrp = local.shapeBrps[i];

            shapeNames.push_back(SharedMemoryString(localName.c_str(), allocator));
            shapeBrps.push_back(SharedMemoryString(localShapeBrp.c_str(), allocator));
        }

        dataRead.store(false); // 标记为未读
        writing.store(false);
    }

    void SharedGeometry::copyToLocal(LocalGeometry& local) const
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        // 设置基础属性
        local.name = std::string(name.c_str());
        local.sysTimeStamp = sysTimeStamp;
        local.version = version.load(); // 更新本地版本号

        // 清空目标向量，准备填充所有几何体数据
        local.shapeNames.clear();
        local.shapeBrps.clear();

        // 复制所有几何体数据
        for (size_t i = 0; i < shapeNames.size(); i++) {
            local.shapeNames.push_back(std::string(shapeNames[i].c_str()));
            local.shapeBrps.push_back(std::string(shapeBrps[i].c_str()));
        }

        dataRead.store(true); // 标记为已读
    }

    void SharedGeometry::getShapeNames(std::vector<std::string>& nameList) const
    {
        nameList.clear();
        for (const auto& name : shapeNames) {
            nameList.push_back(std::string(name.c_str()));
        }
    }

    std::string SharedGeometry::getShapeBrpByName(const std::string& name) const
    {
        for (size_t i = 0; i < shapeNames.size(); i++) {
            if (std::string(shapeNames[i].c_str()) == name) {
                return std::string(shapeBrps[i].c_str());
            }
        }
        return "";
    }

    //================ SharedDefinitionList 实现 ================
    SharedDefinitionList::SharedDefinitionList(bip::managed_shared_memory::segment_manager* segment_manager)
        : SharedDataBase(segment_manager),
        description(SharedMemoryAllocator<char>(segment_manager)),
        ids(SharedMemoryAllocator<int>(segment_manager)),
        parameterNames(SharedMemoryAllocator<SharedMemoryString>(segment_manager)),
        parameterValues(SharedMemoryAllocator<double>(segment_manager)),
        definitionStartIndices(SharedMemoryAllocator<int>(segment_manager)),
        definitionParameterCounts(SharedMemoryAllocator<int>(segment_manager))
    {
        setDataType(DataType::DEFINITION_DATA);
    }

    SharedDefinitionList::SharedDefinitionList(const SharedDefinitionList& other)
        : SharedDataBase(other),
        description(other.description),
        ids(other.ids),
        parameterNames(other.parameterNames),
        parameterValues(other.parameterValues),
        definitionStartIndices(other.definitionStartIndices),
        definitionParameterCounts(other.definitionParameterCounts)
    {
        setDataType(DataType::DEFINITION_DATA);
    }

    SharedDefinitionList& SharedDefinitionList::operator=(const SharedDefinitionList& other)
    {
        SharedDataBase::operator=(other);
        description = other.description;
        ids = other.ids;
        parameterNames = other.parameterNames;
        parameterValues = other.parameterValues;
        definitionStartIndices = other.definitionStartIndices;
        definitionParameterCounts = other.definitionParameterCounts;
        return *this;
    }

    void SharedDefinitionList::copyFromLocal(const LocalDefinitionList& local, SharedMemoryAllocator<char> allocator)
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        writing.store(true);
        version.store(version.load() + 1);

        // 设置基础属性
        name = SharedMemoryString(local.name.c_str(), allocator);
        sysTimeStamp = local.sysTimeStamp;
        dataType = local.dataType;
        description = SharedMemoryString(local.description.c_str(), allocator);

        // 清空所有数据
        ids.clear();
        parameterNames.clear();
        parameterValues.clear();
        definitionStartIndices.clear();
        definitionParameterCounts.clear();

        // 预分配空间
        ids.reserve(local.definitions.size());

        // 计算所有参数的总数
        size_t totalParameterCount = 0;
        for (const auto& def : local.definitions) {
            totalParameterCount += def.parameterNames.size();
        }

        parameterNames.reserve(totalParameterCount);
        parameterValues.reserve(totalParameterCount);
        definitionStartIndices.reserve(local.definitions.size());
        definitionParameterCounts.reserve(local.definitions.size());

        // 处理所有参数集
        size_t currentIndex = 0;
        for (const auto& def : local.definitions) {
            // 添加ID和开始索引
            ids.push_back(def.id);
            definitionStartIndices.push_back(currentIndex);
            definitionParameterCounts.push_back(def.parameterNames.size());

            // 添加所有参数
            for (size_t i = 0; i < def.parameterNames.size(); i++) {
                parameterNames.push_back(SharedMemoryString(def.parameterNames[i].c_str(), allocator));
                parameterValues.push_back(def.parameterValues[i]);
                currentIndex++;
            }
        }

        dataRead.store(false); // 标记为未读
        writing.store(false);
    }

    void SharedDefinitionList::copyToLocal(LocalDefinitionList& local) const
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        // 设置基础属性
        local.name = std::string(name.c_str());
        local.sysTimeStamp = sysTimeStamp;
        local.version = version.load(); // 更新本地版本号
        local.dataType = dataType;
        local.description = std::string(description.c_str());

        // 清空定义列表
        local.definitions.clear();

        // 重建所有定义
        for (size_t i = 0; i < ids.size(); i++) {
            Definition def;
            def.id = ids[i];

            // 获取当前定义的参数范围
            int startIndex = definitionStartIndices[i];
            int paramCount = definitionParameterCounts[i];

            // 复制参数
            for (int j = 0; j < paramCount; j++) {
                int paramIndex = startIndex + j;
                if (paramIndex < parameterNames.size() && paramIndex < parameterValues.size()) {
                    def.parameterNames.push_back(std::string(parameterNames[paramIndex].c_str()));
                    def.parameterValues.push_back(parameterValues[paramIndex]);
                }
            }

            // 添加到定义列表
            local.definitions.push_back(def);
        }

        dataRead.store(true); // 标记为已读
    }

    //================ SharedMesh 实现 ================
    SharedMesh::SharedMesh(bip::managed_shared_memory::segment_manager* segment_manager)
        : SharedDataBase(segment_manager, DataType::MESH_DATA),
        nodes(SharedMemoryAllocator<Node>(segment_manager)),
        Edges(SharedMemoryAllocator<Edge>(segment_manager)),
        Triangles(SharedMemoryAllocator<Triangle>(segment_manager)),
        Tetrahedrons(SharedMemoryAllocator<Tetrahedron>(segment_manager)),
        modelName(SharedMemoryAllocator<char>(segment_manager))
    {
        setDataType(DataType::MESH_DATA);
    }

    SharedMesh::SharedMesh(const SharedMesh& other)
        : SharedDataBase(other),
        modelName(other.modelName),
        nodes(other.nodes),
        Edges(other.Edges),
        Triangles(other.Triangles),
        Tetrahedrons(other.Tetrahedrons)
    {
        setDataType(DataType::MESH_DATA);
    }

    SharedMesh& SharedMesh::operator=(const SharedMesh& other)
    {
        SharedDataBase::operator=(other);
        modelName = other.modelName;
        nodes = other.nodes;
        Edges = other.Edges;
        Triangles = other.Triangles;
        Tetrahedrons = other.Tetrahedrons;
        return *this;
    }

    void SharedMesh::copyFromLocal(const LocalMesh& local, SharedMemoryAllocator<char> allocator)
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        writing.store(true);
        version.store(version.load() + 1);

        // 设置基础属性
        name = SharedMemoryString(local.name.c_str(), allocator);
        sysTimeStamp = local.sysTimeStamp;
        dataType = local.dataType;

        modelName = SharedMemoryString(local.modelName.c_str(), allocator);

        // 复制节点数据 - 优化：先预分配空间
        nodes.clear();
        nodes.reserve(local.nodes.size());
        for (const auto& node : local.nodes) {
            nodes.push_back(node);
        }

        // 复制边数据 - 优化：先预分配空间
        Edges.clear();
        Edges.reserve(local.edges.size());
        for (const auto& edge : local.edges) {
            Edges.push_back(edge);
        }

        // 复制三角形数据 - 优化：先预分配空间
        Triangles.clear();
        Triangles.reserve(local.triangles.size());
        for (const auto& triangle : local.triangles) {
            Triangles.push_back(triangle);
        }

        // 复制四面体数据 - 优化：先预分配空间
        Tetrahedrons.clear();
        Tetrahedrons.reserve(local.tetrahedrons.size());
        for (const auto& tetrahedron : local.tetrahedrons) {
            Tetrahedrons.push_back(tetrahedron);
        }

        dataRead.store(false); // 标记为未读
        writing.store(false);
    }

    void SharedMesh::copyToLocal(LocalMesh& local) const
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        // 设置基础属性
        local.name = std::string(name.c_str());
        local.sysTimeStamp = sysTimeStamp;
        local.version = version.load(); // 更新本地版本号
        local.dataType = dataType;
        local.modelName = std::string(modelName.c_str());

        // 复制节点数据 - 优化：先预分配空间
        local.nodes.clear();
        local.nodes.reserve(nodes.size());
        for (const auto& node : nodes) {
            local.nodes.push_back(node);
        }

        // 复制边数据 - 优化：先预分配空间
        local.edges.clear();
        local.edges.reserve(Edges.size());
        for (const auto& edge : Edges) {
            local.edges.push_back(edge);
        }

        // 复制三角形数据 - 优化：先预分配空间
        local.triangles.clear();
        local.triangles.reserve(Triangles.size());
        for (const auto& triangle : Triangles) {
            local.triangles.push_back(triangle);
        }

        // 复制四面体数据 - 优化：先预分配空间
        local.tetrahedrons.clear();
        local.tetrahedrons.reserve(Tetrahedrons.size());
        for (const auto& tetrahedron : Tetrahedrons) {
            local.tetrahedrons.push_back(tetrahedron);
        }

        dataRead.store(true); // 标记为已读
    }

    //================ SharedData 实现 ================
    SharedData::SharedData(bip::managed_shared_memory::segment_manager* segment_manager)
        : SharedDataBase(segment_manager, DataType::CALCULATION_DATA),
        meshName(SharedMemoryAllocator<char>(segment_manager)),
        index(SharedMemoryAllocator<int>(segment_manager)),
        data(SharedMemoryAllocator<double>(segment_manager)),
        dimtags(SharedMemoryAllocator<SharedMemoryPair>(segment_manager))
    {
        isFieldData = true;
        t = 0.0;
        type = VertexData;
        isSequentiallyMatchedWithMesh = true;
        setDataType(DataType::CALCULATION_DATA);
    }

    SharedData::SharedData(const SharedData& other)
        : SharedDataBase(other),
        t(other.t),
        type(other.type),
        isSequentiallyMatchedWithMesh(other.isSequentiallyMatchedWithMesh),
        meshName(other.meshName),
        index(other.index),
        data(other.data),
        dimtags(other.dimtags),
        isFieldData(other.isFieldData)
    {
        setDataType(DataType::CALCULATION_DATA);
    }

    SharedData& SharedData::operator=(const SharedData& other)
    {
        SharedDataBase::operator=(other);
        t = other.t;
        type = other.type;
        isSequentiallyMatchedWithMesh = other.isSequentiallyMatchedWithMesh;
        meshName = other.meshName;
        index = other.index;
        data = other.data;
        dimtags = other.dimtags;
        isFieldData = other.isFieldData;
        return *this;
    }

    void SharedData::copyFromLocal(const LocalData& local, SharedMemoryAllocator<char> allocator)
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        writing.store(true);
        version.store(version.load() + 1);

        // 设置基础属性
        name = SharedMemoryString(local.name.c_str(), allocator);
        sysTimeStamp = local.sysTimeStamp;
        dataType = local.dataType;
        meshName = SharedMemoryString(local.meshName.c_str(), allocator);
        isFieldData = local.isFieldData;
        type = local.type;
        isSequentiallyMatchedWithMesh = local.isSequentiallyMatchedWithMesh;
        t = local.t;

        // 复制索引数据 - 优化：先预分配空间
        index.clear();
        index.reserve(local.index.size());
        for (const auto& idx : local.index) {
            index.push_back(idx);
        }

        // 复制数值数据 - 优化：先预分配空间
        data.clear();
        data.reserve(local.data.size());
        for (const auto& val : local.data) {
            data.push_back(val);
        }

        // 复制几何位置数据 - 优化：先预分配空间
        dimtags.clear();
        dimtags.reserve(local.dimtags.size());
        for (const auto& dimtag : local.dimtags) {
            dimtags.push_back(SharedMemoryPair(dimtag.first, dimtag.second));
        }

        dataRead.store(false); // 标记为未读
        writing.store(false);
    }

    void SharedData::copyToLocal(LocalData& local) const
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        // 设置基础属性
        local.name = std::string(name.c_str());
        local.sysTimeStamp = sysTimeStamp;
        local.version = version.load(); // 更新本地版本号
        local.dataType = dataType;
        local.meshName = std::string(meshName.c_str());
        local.isFieldData = isFieldData;
        local.type = type;
        local.isSequentiallyMatchedWithMesh = isSequentiallyMatchedWithMesh;
        local.t = t;

        // 复制索引数据 - 优化：先预分配空间
        local.index.clear();
        local.index.reserve(index.size());
        for (const auto& idx : index) {
            local.index.push_back(idx);
        }

        // 复制数值数据 - 优化：先预分配空间
        local.data.clear();
        local.data.reserve(data.size());
        for (const auto& val : data) {
            local.data.push_back(val);
        }

        // 复制几何位置数据 - 优化：先预分配空间
        local.dimtags.clear();
        local.dimtags.reserve(dimtags.size());
        for (const auto& dimtag : dimtags) {
            local.dimtags.push_back(std::make_pair(dimtag.first, dimtag.second));
        }

        dataRead.store(true); // 标记为已读
    }

    //================ SharedControlData 实现 ================
    SharedControlData::SharedControlData(bip::managed_shared_memory::segment_manager* segment_manager)
        : SharedDataBase(segment_manager, DataType::CONTROL_DATA),
        jsonConfig(SharedMemoryAllocator<char>(segment_manager)),
        exception(segment_manager),
        sharedModelNames(SharedMemoryAllocator<SharedMemoryString>(segment_manager)),
        sharedModelMemorySizes(SharedMemoryAllocator<int>(segment_manager)),
        sharedMeshNames(SharedMemoryAllocator<SharedMemoryString>(segment_manager)),
        sharedMeshMemorySizes(SharedMemoryAllocator<int>(segment_manager)),
        sharedDataNames(SharedMemoryAllocator<SharedMemoryString>(segment_manager)),
        sharedDataMemorySizes(SharedMemoryAllocator<int>(segment_manager)),
        sharedDefinitionNames(SharedMemoryAllocator<SharedMemoryString>(segment_manager)),
        sharedDefinitionMemorySizes(SharedMemoryAllocator<int>(segment_manager))
    {
        // 初始化基本类型
        dt = 0.01;
        t = 0.0;
        isConverged = false;

        // 初始化内存段信息
        geometrySegmentTotalSize = 0;
        geometrySegmentFreeSize = 0;
        meshSegmentTotalSize = 0;
        meshSegmentFreeSize = 0;
        dataSegmentTotalSize = 0;
        dataSegmentFreeSize = 0;
        controlSegmentTotalSize = 0;
        controlSegmentFreeSize = 0;
        definitionSegmentTotalSize = 0;
        definitionSegmentFreeSize = 0;
    }

    SharedControlData::SharedControlData(const SharedControlData& other)
        : SharedDataBase(other),
        jsonConfig(other.jsonConfig),
        dt(other.dt),
        t(other.t),
        isConverged(other.isConverged),
        exception(other.exception),
        sharedModelNames(other.sharedModelNames),
        sharedModelMemorySizes(other.sharedModelMemorySizes),
        sharedMeshNames(other.sharedMeshNames),
        sharedMeshMemorySizes(other.sharedMeshMemorySizes),
        sharedDataNames(other.sharedDataNames),
        sharedDataMemorySizes(other.sharedDataMemorySizes),
        sharedDefinitionNames(other.sharedDefinitionNames),
        sharedDefinitionMemorySizes(other.sharedDefinitionMemorySizes),
        geometrySegmentTotalSize(other.geometrySegmentTotalSize),
        geometrySegmentFreeSize(other.geometrySegmentFreeSize),
        meshSegmentTotalSize(other.meshSegmentTotalSize),
        meshSegmentFreeSize(other.meshSegmentFreeSize),
        dataSegmentTotalSize(other.dataSegmentTotalSize),
        dataSegmentFreeSize(other.dataSegmentFreeSize),
        controlSegmentTotalSize(other.controlSegmentTotalSize),
        controlSegmentFreeSize(other.controlSegmentFreeSize),
        definitionSegmentTotalSize(other.definitionSegmentTotalSize),
        definitionSegmentFreeSize(other.definitionSegmentFreeSize)
    {
        setDataType(DataType::CONTROL_DATA);
    }

    SharedControlData& SharedControlData::operator=(const SharedControlData& other)
    {
        SharedDataBase::operator=(other);
        jsonConfig = other.jsonConfig;
        dt = other.dt;
        t = other.t;
        isConverged = other.isConverged;
        exception = other.exception;
        sharedModelNames = other.sharedModelNames;
        sharedModelMemorySizes = other.sharedModelMemorySizes;
        sharedMeshNames = other.sharedMeshNames;
        sharedMeshMemorySizes = other.sharedMeshMemorySizes;
        sharedDataNames = other.sharedDataNames;
        sharedDataMemorySizes = other.sharedDataMemorySizes;
        sharedDefinitionNames = other.sharedDefinitionNames;
        sharedDefinitionMemorySizes = other.sharedDefinitionMemorySizes;
        geometrySegmentTotalSize = other.geometrySegmentTotalSize;
        geometrySegmentFreeSize = other.geometrySegmentFreeSize;
        meshSegmentTotalSize = other.meshSegmentTotalSize;
        meshSegmentFreeSize = other.meshSegmentFreeSize;
        dataSegmentTotalSize = other.dataSegmentTotalSize;
        dataSegmentFreeSize = other.dataSegmentFreeSize;
        controlSegmentTotalSize = other.controlSegmentTotalSize;
        controlSegmentFreeSize = other.controlSegmentFreeSize;
        definitionSegmentTotalSize = other.definitionSegmentTotalSize;
        definitionSegmentFreeSize = other.definitionSegmentFreeSize;

        return *this;
    }

    void SharedControlData::copyFromLocal(const LocalControlData& local, SharedMemoryAllocator<char> allocator)
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        writing.store(true);
        version.store(version.load() + 1);

        // 设置基础属性
        name = SharedMemoryString(local.name.c_str(), allocator);
        sysTimeStamp = local.sysTimeStamp;
        dataType = local.dataType;
        jsonConfig = SharedMemoryString(local.jsonConfig.c_str(), allocator);
        dt = local.dt;
        t = local.t;
        isConverged = local.isConverged;

        // 复制内存段信息
        geometrySegmentTotalSize = local.geometrySegmentTotalSize;
        geometrySegmentFreeSize = local.geometrySegmentFreeSize;
        meshSegmentTotalSize = local.meshSegmentTotalSize;
        meshSegmentFreeSize = local.meshSegmentFreeSize;
        dataSegmentTotalSize = local.dataSegmentTotalSize;
        dataSegmentFreeSize = local.dataSegmentFreeSize;
        controlSegmentTotalSize = local.controlSegmentTotalSize;
        controlSegmentFreeSize = local.controlSegmentFreeSize;
        definitionSegmentTotalSize = local.definitionSegmentTotalSize;
        definitionSegmentFreeSize = local.definitionSegmentFreeSize;

        // 复制模型名称和内存大小列表
        sharedModelNames.clear();
        sharedModelMemorySizes.clear();
        sharedModelNames.reserve(local.modelNames.size());
        sharedModelMemorySizes.reserve(local.modelMemorySizes.size());
        for (size_t i = 0; i < local.modelNames.size(); ++i) {
            sharedModelNames.push_back(
                SharedMemoryString(local.modelNames[i].c_str(), allocator)
            );
            if (i < local.modelMemorySizes.size()) {
                sharedModelMemorySizes.push_back(local.modelMemorySizes[i]);
            }
            else {
                sharedModelMemorySizes.push_back(0); // 默认值
            }
        }

        // 复制网格名称和内存大小列表
        sharedMeshNames.clear();
        sharedMeshMemorySizes.clear();
        sharedMeshNames.reserve(local.meshNames.size());
        sharedMeshMemorySizes.reserve(local.meshMemorySizes.size());
        for (size_t i = 0; i < local.meshNames.size(); ++i) {
            sharedMeshNames.push_back(
                SharedMemoryString(local.meshNames[i].c_str(), allocator)
            );
            if (i < local.meshMemorySizes.size()) {
                sharedMeshMemorySizes.push_back(local.meshMemorySizes[i]);
            }
            else {
                sharedMeshMemorySizes.push_back(0); // 默认值
            }
        }

        // 复制数据名称和内存大小列表
        sharedDataNames.clear();
        sharedDataMemorySizes.clear();
        sharedDataNames.reserve(local.dataNames.size());
        sharedDataMemorySizes.reserve(local.dataMemorySizes.size());
        for (size_t i = 0; i < local.dataNames.size(); ++i) {
            sharedDataNames.push_back(
                SharedMemoryString(local.dataNames[i].c_str(), allocator)
            );
            if (i < local.dataMemorySizes.size()) {
                sharedDataMemorySizes.push_back(local.dataMemorySizes[i]);
            }
            else {
                sharedDataMemorySizes.push_back(0); // 默认值
            }
        }

        // 复制模型参数名称和内存大小列表
        sharedDefinitionNames.clear();
        sharedDefinitionMemorySizes.clear();
        sharedDefinitionNames.reserve(local.definitionNames.size());
        sharedDefinitionMemorySizes.reserve(local.definitionMemorySizes.size());
        for (size_t i = 0; i < local.definitionNames.size(); ++i) {
            sharedDefinitionNames.push_back(
                SharedMemoryString(local.definitionNames[i].c_str(), allocator)
            );
            if (i < local.definitionMemorySizes.size()) {
                sharedDefinitionMemorySizes.push_back(local.definitionMemorySizes[i]);
            }
            else {
                sharedDefinitionMemorySizes.push_back(0); // 默认值
            }
        }

        dataRead.store(false); // 标记为未读
        writing.store(false);
    }

    void SharedControlData::copyToLocal(LocalControlData& local) const
    {
        // 如果版本号相同，则不需要更新
        if (local.version == version.load()) {
            // 直接返回，不做任何更改
            return;
        }

        // 设置基础属性
        local.name = std::string(name.c_str());
        local.sysTimeStamp = sysTimeStamp;
        local.version = version.load(); // 更新本地版本号
        local.dataType = dataType;
        local.jsonConfig = std::string(jsonConfig.c_str());
        local.dt = dt;
        local.t = t;
        local.isConverged = isConverged;

        // 复制内存段信息
        local.geometrySegmentTotalSize = geometrySegmentTotalSize;
        local.geometrySegmentFreeSize = geometrySegmentFreeSize;
        local.meshSegmentTotalSize = meshSegmentTotalSize;
        local.meshSegmentFreeSize = meshSegmentFreeSize;
        local.dataSegmentTotalSize = dataSegmentTotalSize;
        local.dataSegmentFreeSize = dataSegmentFreeSize;
        local.controlSegmentTotalSize = controlSegmentTotalSize;
        local.controlSegmentFreeSize = controlSegmentFreeSize;
        local.definitionSegmentTotalSize = definitionSegmentTotalSize;
        local.definitionSegmentFreeSize = definitionSegmentFreeSize;

        // 复制模型名称和内存大小列表
        local.modelNames.clear();
        local.modelMemorySizes.clear();
        local.modelNames.reserve(sharedModelNames.size());
        local.modelMemorySizes.reserve(sharedModelMemorySizes.size());
        for (size_t i = 0; i < sharedModelNames.size(); ++i) {
            local.modelNames.push_back(std::string(sharedModelNames[i].c_str()));
            if (i < sharedModelMemorySizes.size()) {
                local.modelMemorySizes.push_back(sharedModelMemorySizes[i]);
            }
            else {
                local.modelMemorySizes.push_back(0); // 默认值
            }
        }

        // 复制网格名称和内存大小列表
        local.meshNames.clear();
        local.meshMemorySizes.clear();
        local.meshNames.reserve(sharedMeshNames.size());
        local.meshMemorySizes.reserve(sharedMeshMemorySizes.size());
        for (size_t i = 0; i < sharedMeshNames.size(); ++i) {
            local.meshNames.push_back(std::string(sharedMeshNames[i].c_str()));
            if (i < sharedMeshMemorySizes.size()) {
                local.meshMemorySizes.push_back(sharedMeshMemorySizes[i]);
            }
            else {
                local.meshMemorySizes.push_back(0); // 默认值
            }
        }

        // 复制数据名称和内存大小列表
        local.dataNames.clear();
        local.dataMemorySizes.clear();
        local.dataNames.reserve(sharedDataNames.size());
        local.dataMemorySizes.reserve(sharedDataMemorySizes.size());
        for (size_t i = 0; i < sharedDataNames.size(); ++i) {
            local.dataNames.push_back(std::string(sharedDataNames[i].c_str()));
            if (i < sharedDataMemorySizes.size()) {
                local.dataMemorySizes.push_back(sharedDataMemorySizes[i]);
            }
            else {
                local.dataMemorySizes.push_back(0); // 默认值
            }
        }

        // 复制模型参数名称和内存大小列表
        local.definitionNames.clear();
        local.definitionMemorySizes.clear();
        local.definitionNames.reserve(sharedDefinitionNames.size());
        local.definitionMemorySizes.reserve(sharedDefinitionMemorySizes.size());
        for (size_t i = 0; i < sharedDefinitionNames.size(); ++i) {
            local.definitionNames.push_back(std::string(sharedDefinitionNames[i].c_str()));
            if (i < sharedDefinitionMemorySizes.size()) {
                local.definitionMemorySizes.push_back(sharedDefinitionMemorySizes[i]);
            }
            else {
                local.definitionMemorySizes.push_back(0); // 默认值
            }
        }

        dataRead.store(true); // 标记为已读
    }
}