#ifndef SHAREDMEMORYMANAGER_H
#define SHAREDMEMORYMANAGER_H

#include "SharedMemoryStruct.h"
#include "SharedMemoryLogger.h"
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <sstream>
#include <cstring>
#include <vector>
#include <memory>
#include "yExceptionBase.h"
#include <cstdint>
#include <fstream>
#include <functional>

namespace EMP {
    // 统一的后缀定义
    namespace SharedMemorySuffix {
        // 内存段后缀
        constexpr char CONTROL_SEGMENT[] = "_ctrl_segment";
        constexpr char GEOMETRY_SEGMENT[] = "_geo_segment";
        constexpr char MESH_SEGMENT[] = "_mesh_segment";
        constexpr char DATA_SEGMENT[] = "_data_segment";
        constexpr char DEFINITION_SEGMENT[] = "_def_segment";

        // 对象后缀
        constexpr char CONTROL[] = "_ctrl";
        constexpr char GEOMETRY[] = "_geo";
        constexpr char MESH[] = "_mesh";
        constexpr char DATA[] = "_data";
        constexpr char DEFINITION[] = "_def";
    }

    // 删除所有的 #define 后缀定义
    // 删除重复的 constexpr 后缀定义

    // Concrete exception class that extends yExceptionBase
    class SharedMemoryException : public yExceptionBase {
    public:
        SharedMemoryException(int type_, int code_, std::string msg_)
            : yExceptionBase(type_, code_, msg_) {}

        void message() override {
            std::cerr << "SharedMemory Exception - Type: " << type << ", Code: " << code
                << ", Message: " << msg << std::endl;
        }
    };

    // 用于创建和管理共享内存的辅助类
    class SOLVERHUB_API SharedMemoryManager {
    public:
        // 构造函数，增加前缀参数用于保证名称唯一性
        // 增加日志文件路径参数，如果为空则不创建日志
        SharedMemoryManager(const std::string& memoryName, bool isCreator = false,
            const std::string& prefix = "", size_t control_memory_size = 1024 * 1024,
            const std::string& logFilePath = "");

        ~SharedMemoryManager();

        // 获取控制数据的指针
        SharedControlData* getControlData();

        // 初始化控制数据
        void initControlData(const std::string& name, const std::string& jsonConfig = "",
            double dt = 0.01, double t = 0.0, bool isConverged = false);

        // 创建几何对象共享内存段和对象
        void createGeometrySegmentAndObjects();

        // 创建网格对象共享内存段和对象
        void createMeshSegmentAndObjects();

        // 创建计算数据对象共享内存段和对象
        void createDataSegmentAndObjects();

        // 创建模型参数对象共享内存段和对象
        void createDefinitionSegmentAndObjects();

        // 获取几何数据的指针
        const std::vector<SharedGeometry*>& getGeometry() const;

        // 获取网格数据的指针
        const std::vector<SharedMesh*>& getMesh() const;

        // 获取计算数据结构的指针
        const std::vector<SharedData*>& getData() const;

        // 获取模型参数结构的指针
        const std::vector<SharedDefinitionList*>& getDefinition() const;

        // 根据名称查找几何对象
        SharedGeometry* findGeometryByName(const std::string& name);

        // 根据名称查找网格对象
        SharedMesh* findMeshByName(const std::string& name);

        // 根据名称查找计算数据对象
        SharedData* findDataByName(const std::string& name);

        // 根据名称查找模型参数对象
        SharedDefinitionList* findDefinitionByName(const std::string& name);

        // 获取分配器 - 根据类型选择不同的共享内存段
        template <typename T>
        SharedMemoryAllocator<T> getAllocator(const std::string& type = "") {
            if (type == "geometry" && geometrySegment_) {
                return SharedMemoryAllocator<T>(geometrySegment_->get_segment_manager());
            }
            else if (type == "mesh" && meshSegment_) {
                return SharedMemoryAllocator<T>(meshSegment_->get_segment_manager());
            }
            else if (type == "data" && dataSegment_) {
                return SharedMemoryAllocator<T>(dataSegment_->get_segment_manager());
            }
            else if (type == "definition" && definitionSegment_) {
                return SharedMemoryAllocator<T>(definitionSegment_->get_segment_manager());
            }
            else {
                return SharedMemoryAllocator<T>(controlSegment_->get_segment_manager());
            }
        }

        // ========== 完整的数据存取接口 ==========

        // 更新控制数据 - 使用完整的LocalControlData
        void updateControlData(const LocalControlData& localData);

        // 获取控制数据 - 使用完整的LocalControlData
        void getControlData(LocalControlData& localData);

        // 更新几何对象 - 使用完整的LocalGeometry
        void updateGeometry(SharedGeometry* geo, const LocalGeometry& localGeo);

        // 获取几何对象 - 使用完整的LocalGeometry
        void getGeometry(SharedGeometry* geo, LocalGeometry& localGeo);

        // 更新网格对象 - 使用完整的LocalMesh
        void updateMesh(SharedMesh* mesh, const LocalMesh& localMesh);

        // 获取网格对象 - 使用完整的LocalMesh
        void getMesh(SharedMesh* mesh, LocalMesh& localMesh);

        // 更新计算数据对象 - 使用完整的LocalData
        void updateData(SharedData* data, const LocalData& localData);

        // 获取计算数据对象 - 使用完整的LocalData
        void getData(SharedData* data, LocalData& localData);

        // 更新模型参数对象 - 使用完整的LocalDefinitionList
        void updateDefinition(SharedDefinitionList* def, const LocalDefinitionList& localDef);

        // 获取模型参数对象 - 使用完整的LocalDefinitionList
        void getDefinition(SharedDefinitionList* def, LocalDefinitionList& localDef);

        // ========== 内存大小估算函数 ==========

        // 估算几何对象所需的共享内存大小
        static size_t estimateGeometryMemorySize(const LocalGeometry& localGeo);

        // 估算网格对象所需的共享内存大小
        static size_t estimateMeshMemorySize(const LocalMesh& localMesh);

        // 估算计算数据对象所需的共享内存大小
        static size_t estimateDataMemorySize(const LocalData& localData);

        // 估算模型参数对象所需的共享内存大小
        static size_t estimateDefinitionMemorySize(const LocalDefinitionList& localDef);

        // ========== 共享内存段管理函数 ==========

        // 返回控制共享内存段的总大小和已使用大小
        std::pair<size_t, size_t> getControlMemoryUsage() const;

        // 返回几何共享内存段的总大小和已使用大小
        std::pair<size_t, size_t> getGeometryMemoryUsage() const;

        // 返回网格共享内存段的总大小和已使用大小
        std::pair<size_t, size_t> getMeshMemoryUsage() const;

        // 返回数据共享内存段的总大小和已使用大小
        std::pair<size_t, size_t> getDataMemoryUsage() const;

        // 返回模型参数共享内存段的总大小和已使用大小
        std::pair<size_t, size_t> getDefinitionMemoryUsage() const;

        // 更新所有内存段大小信息到控制数据
        void updateMemorySegmentInfo();

        // 从控制数据中获取几何模型名称列表
        void getControlDataModelNames(std::vector<std::string>& modelNames);

        // 从控制数据中获取网格名称列表
        void getControlDataMeshNames(std::vector<std::string>& meshNames);

        // 获取数据类型信息
        void getDataTypeInfo(SharedData* data, bool& isFieldData, DataGeoType& type);

        // 获取数据关联的网格名称
        void getDataMeshName(SharedData* data, std::string& meshName);

        // 更新控制数据中的时间步长
        void updateControlDataDt(double dt);

        // 更新控制数据中的当前时间
        void updateControlDataTime(double t);

        // 检查几何对象所需内存空间是否足够，不足则设置异常
        bool checkAndUpdateGeometryMemorySize(const std::string& name, const LocalGeometry& localGeo);

        // 检查网格对象所需内存空间是否足够，不足则设置异常
        bool checkAndUpdateMeshMemorySize(const std::string& name, const LocalMesh& localMesh);

        // 检查计算数据对象所需内存空间是否足够，不足则设置异常
        bool checkAndUpdateDataMemorySize(const std::string& name, const LocalData& localData);

        // 检查模型参数对象所需内存空间是否足够，不足则设置异常
        bool checkAndUpdateDefinitionMemorySize(const std::string& name, const LocalDefinitionList& localDef);

        // 重新创建几何内存段，增加大小
        bool recreateGeometrySegment(size_t newSize);

        // 重新创建网格内存段，增加大小
        bool recreateMeshSegment(size_t newSize);

        // 重新创建计算数据内存段，增加大小
        bool recreateDataSegment(size_t newSize);

        // 重新创建模型参数内存段，增加大小
        bool recreateDefinitionSegment(size_t newSize);

        // Creator根据控制数据中的信息，自动调整内存段大小
        void autoAdjustMemorySegments();

        // ========== 异常处理函数 ==========

	    // 设置异常信息
		void setException(int type, int code, const std::string& message);

		// 检查是否有异常
		bool hasException() const;

		// 获取异常详情并清除异常标志
		std::tuple<int, int, std::string> getAndClearException();

		// 创建异常对象
		std::unique_ptr<yExceptionBase> createExceptionObject();

        // ========== 日志相关函数 ==========

        // 获取日志记录器，用于外部记录
        SharedMemoryLogger* getLogger() const { return logger_.get(); }

        // 设置日志级别
        void setLogLevel(LogLevel level);

        // ========== 共享内存与磁盘文件的读写转换接口 ==========

        // 将共享内存中的数据保存到磁盘文件
        bool saveToFile(const std::string& filePath, bool binaryFormat = false);

        // 将特定类型的共享内存段保存到文件
        bool saveSegmentToFile(const std::string& filePath, const std::string& segmentType, bool binaryFormat = false);

        // 将指定的共享对象保存到文件
        bool saveObjectToFile(const std::string& filePath, const std::string& objectName, bool binaryFormat = false);

        // 从磁盘文件加载数据到共享内存(仅creator调用)
        bool loadFromFile(const std::string& filePath, bool binaryFormat = false);

        // 从文件加载数据到特定类型的共享内存段(仅creator调用)
        bool loadSegmentFromFile(const std::string& filePath, const std::string& segmentType, bool binaryFormat = false);

        // 从文件加载数据到指定的共享对象(仅creator调用)
        bool loadObjectFromFile(const std::string& filePath, const std::string& objectName, bool binaryFormat = false);

        // 创建共享内存快照(保存所有共享内存段)用于undo/redo操作
        bool createSnapshot(const std::string& snapshotDir);

        // 恢复共享内存快照(仅creator调用)
        bool restoreSnapshot(const std::string& snapshotDir);

	private:
		// 生成唯一的对象名称
		std::string GenerateUniqueObjectName(const std::string& baseName);

        // 生成共享内存段名称
        std::string GenerateSegmentName(const std::string& suffix);

        // 加载已存在的控制对象
        void LoadExistingControlData();

        // 加载已存在的几何对象
        void LoadExistingGeometryObjects();

        // 加载已存在的网格对象
        void LoadExistingMeshObjects();

        // 加载已存在的数据对象
        void LoadExistingDataObjects();

        // 加载已存在的模型参数对象
        void LoadExistingDefinitionObjects();

        // 记录信息到日志
        void log(LogLevel level, const std::string& message);

        // 共用基础变量
        std::string memoryName_;
        std::shared_ptr<bip::named_mutex> sharedMutex_;
        bool isCreator_;
        std::string prefix_;
        std::unique_ptr<SharedMemoryLogger> logger_;

        // 单独的共享内存段
        std::shared_ptr<bip::managed_shared_memory> controlSegment_;
        std::shared_ptr<bip::managed_shared_memory> geometrySegment_;
        std::shared_ptr<bip::managed_shared_memory> meshSegment_;
        std::shared_ptr<bip::managed_shared_memory> dataSegment_;
        std::shared_ptr<bip::managed_shared_memory> definitionSegment_;

        // 共享对象指针
        SharedControlData* controlData_ = nullptr;
        std::vector<SharedGeometry*> geos_;
        std::vector<SharedMesh*> meshs_;
        std::vector<SharedData*> datas_;
        std::vector<SharedDefinitionList*> defs_;

        // 内部辅助函数：将对象写入已打开的文件流
        bool saveObjectToFile(std::ofstream& file, std::shared_ptr<bip::managed_shared_memory> segment,
                             const std::string& objectName, bool binaryFormat);

        // 内部辅助函数：从已打开的文件流读取对象
        bool loadObjectFromFile(std::ifstream& file, std::shared_ptr<bip::managed_shared_memory> segment,
                               bool binaryFormat);
    };
}

#endif

