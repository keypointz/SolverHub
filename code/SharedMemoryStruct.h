#ifndef SHAREDMEMORYSTRUCT_H
#define SHAREDMEMORYSTRUCT_H

#include <string>
#include <deque>
#include <set>
#include <ctime>
#include <thread>
#include <chrono>
#include <iostream>
#include <atomic>
#include <Eigen/Dense>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/pair.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <Windows.h> // 包含 Windows.h
#include "SolverHubDef.h"

using namespace Eigen;

namespace bip = boost::interprocess;


// 自定义分配器，用于在共享内存中分配数据
template <typename T>
using SharedMemoryAllocator = bip::allocator<T, bip::managed_shared_memory::segment_manager>;

// 在共享内存中使用的 vector 类型
template <typename T>
using SharedMemoryVector = bip::vector<T, SharedMemoryAllocator<T>>;

using SharedMemoryString = bip::basic_string<char, std::char_traits<char>, SharedMemoryAllocator<char>>;

using SharedMemoryVectorString = bip::vector<SharedMemoryString, SharedMemoryAllocator<SharedMemoryString>>;

using SharedMemoryPair = bip::pair<int, int>;

using SharedMemoryVectorPair = bip::vector<SharedMemoryPair, SharedMemoryAllocator<SharedMemoryPair>>;

namespace EMP {
    // 前向声明所有共享内存结构
    struct SharedControlData;
    struct SharedGeometry;
    struct SharedMesh;
    struct SharedData;
    struct SharedDefinitionList;

	// 数据类型标识，用于动态类型识别
	enum DataType {
		UNKNOWN_DATA = 0,
		GEOMETRY_DATA = 1,
		MESH_DATA = 2,
		CALCULATION_DATA = 3,
		DEFINITION_DATA = 4,
		CONTROL_DATA = 5
	};

	enum DataGeoType {
		VertexData = 0, // 定义在网格结点上
		EdgeData, // 定义在网格边上
		FacetData, // 定义在网格面上
		BlockData // 定义在网格体上
	};

	// 本地数据基类，为所有本地数据结构提供统一接口
	class SOLVERHUB_API LocalDataBase {
	public:
		std::string name;                 // 数据名称
		time_t sysTimeStamp;              // 系统时间戳
		uint64_t version;                 // 版本号，用于与共享对象比较
		DataType dataType;                // 数据类型标识

		// 默认构造函数
		LocalDataBase();
		
		// 带参数的构造函数
		LocalDataBase(const std::string& name_, DataType dataType_ = DataType::UNKNOWN_DATA);
		
		// 虚析构函数
		virtual ~LocalDataBase() = default;
		
		// 获取数据名称
		std::string getName() const;
		
		// 获取版本号
		uint64_t getVersion() const;
		
		// 获取时间戳
		time_t getTimeStamp() const;
		
		// 获取数据类型
		virtual DataType getDataType() const;
		
		// 设置数据类型
		void setDataType(DataType type);
	};
	
	// 共享数据基类，为所有共享数据结构提供统一接口
	class SOLVERHUB_API SharedDataBase {
	public:
		std::atomic<uint64_t> version;    // 版本号，原子类型确保多进程并发安全
		std::atomic<bool> writing;        // 写入锁标志
		mutable std::atomic<bool> dataRead;       // 数据是否已被读取标志
		time_t sysTimeStamp;              // 系统时间戳
		SharedMemoryString name;          // 数据名称
		bip::interprocess_mutex mutex;    // 用于保护共享数据的互斥锁
		DataType dataType;                // 数据类型标识
		
		// 构造函数
		SharedDataBase(bip::managed_shared_memory::segment_manager* segment_manager, DataType dataType_ = DataType::UNKNOWN_DATA);
		
		// 拷贝构造函数
		SharedDataBase(const SharedDataBase& other);
		
		// 赋值运算符
		SharedDataBase& operator=(const SharedDataBase& other);
		
		// 虚析构函数
		virtual ~SharedDataBase() = default;
		
		// 获取数据类型
		DataType getDataType() const;
		
		// 设置数据类型
		void setDataType(DataType type);
	};

	// 定义模型的一组参数
	struct SOLVERHUB_API Definition {
		int id;
		// 参数名称列表
		std::vector<std::string> parameterNames;
		// 参数值列表
		std::vector<double> parameterValues;
	};

	// 定义模型的多组参数，用于参数扫描或优化等
	struct SOLVERHUB_API LocalDefinitionList : public LocalDataBase
	{
		std::string description;
		std::vector<Definition> definitions;
		
		// 默认构造函数
		LocalDefinitionList();
		
		// 带参数的构造函数
		LocalDefinitionList(const std::string& name_, const std::string& description_);
		
		// 添加定义
		void addDefinition(const Definition& def);
		
		// 根据ID查找定义
		Definition* findDefinitionById(int id);
		
		// 获取定义数量
		size_t getDefinitionCount() const;
		
		// 实现LocalDataBase接口
		DataType getDataType() const override;
	};
	
	struct SOLVERHUB_API Node {
		int id;
		int ref;
		double x, y, z;
	};

	struct SOLVERHUB_API Edge
	{
		int id;
		int ref;
		int nodes[2];
	};

	struct SOLVERHUB_API Triangle
	{
		int id;
		int ref;
		int nodes[3];
		int edge_ref[3];
	};

	struct SOLVERHUB_API Tetrahedron
	{
		int id;
		int ref;
		int nodes[4];
		int edge_ref[6];
		int facet_ref[4];
	};

	// 网格信息
	struct SOLVERHUB_API MeshInfo {
		int myNbNodes;
		int myNbEdges;
		int myNbTriangles;
		int myNbTetras;
		
		//初始化函数
		MeshInfo();

		// 复制构造函数
		MeshInfo(const MeshInfo& other);

		// 赋值运算符
		MeshInfo& operator=(const MeshInfo& other);
	};

	// 一组几何模型形状，用于几何编号定位和多个几何共同处理
	struct SOLVERHUB_API LocalGeometry : public LocalDataBase
	{
		std::vector<std::string> shapeNames;    // 几何模型名列表
		std::vector<std::string> shapeBrps;     // 共享的几何文件列表

		// 默认构造函数
		LocalGeometry();

		// 带参数的构造函数 - 单个几何体
		LocalGeometry(const std::string& name_, const std::string& shapeBrp_);
		
		// 添加几何体
		void addGeometry(const std::string& name_, const std::string& shapeBrp_);
		
		// 根据名称获取几何文件
		std::string getShapeBrpByName(const std::string& name_) const;
		
		// 获取几何体数量
		size_t getGeometryCount() const;
		
		// 获取主要几何体名称（第一个）
		std::string getPrimaryName() const;
		
		// 获取主要几何体文件（第一个）
		std::string getPrimaryShapeBrp() const;
		
		// 实现LocalDataBase接口
		DataType getDataType() const override;
	};

	// 非共享的网格数据结构
	struct SOLVERHUB_API LocalMesh : public LocalDataBase
	{
		std::string modelName;		      // 对应的几何模型的名称
		std::vector<Node> nodes;
		std::vector<Edge> edges;
		std::vector<Triangle> triangles;
		std::vector<Tetrahedron> tetrahedrons;

		// 默认构造函数
		LocalMesh();

		// 带参数的构造函数
		LocalMesh(const std::string& name_, const std::string& modelName_);

		// 实现LocalDataBase接口
		DataType getDataType() const override;
	};

	// 非共享的计算数据结构
	struct SOLVERHUB_API LocalData : public LocalDataBase
	{
		std::string meshName;            // 网格名
		bool isFieldData;                // 是否是场数据
		DataGeoType type;                // 数据类型
		bool isSequentiallyMatchedWithMesh; // 数据是否与网格顺序匹配
		double t;                        // 数据对应的耦合计算时刻
		std::vector<std::pair<int, int>> dimtags; // 数据对应的几何位置 (dimension, tag)
		std::vector<int> index;          // 数据索引
		std::vector<double> data;        // 数据值

		// 默认构造函数
		LocalData();

		// 带参数的构造函数
		LocalData(const std::string& name_, const std::string& meshName_);

		// 实现LocalDataBase接口
		DataType getDataType() const override;
		
		// 文件读写功能
		bool saveToFile(const std::string& filePath) const;
		bool loadFromFile(const std::string& filePath);
	};

	// 共享的几何文件类
	struct SOLVERHUB_API SharedGeometry : public SharedDataBase
	{
		SharedMemoryVectorString shapeNames;     // 几何模型名
		SharedMemoryVectorString shapeBrps;      // 共享的几何文件

		SharedGeometry(bip::managed_shared_memory::segment_manager* segment_manager);
		
        // 拷贝构造函数
		SharedGeometry(const SharedGeometry& other);
		
		// operator=()
		SharedGeometry& operator=(const SharedGeometry& other);
		
		// 从LocalGeometry复制数据到共享对象
		void copyFromLocal(const LocalGeometry& local, SharedMemoryAllocator<char> allocator);
		
		// 复制数据到LocalGeometry
		void copyToLocal(LocalGeometry& local) const;
		
		// 获取所有几何模型的名称列表
		void getShapeNames(std::vector<std::string>& nameList) const;
		
		// 根据名称获取对应的几何文件
		std::string getShapeBrpByName(const std::string& name) const;
	};

	// 共享的模型参数类
	struct SOLVERHUB_API SharedDefinitionList : public SharedDataBase
	{
		SharedMemoryString description;   // 参数集描述
		SharedMemoryVector<int> ids;      // 参数定义ID列表
		SharedMemoryVectorString parameterNames;  // 所有参数名称列表
		SharedMemoryVector<double> parameterValues; // 所有参数值列表
		SharedMemoryVector<int> definitionStartIndices; // 每个定义的起始索引
		SharedMemoryVector<int> definitionParameterCounts; // 每个定义的参数数量

		SharedDefinitionList(bip::managed_shared_memory::segment_manager* segment_manager);
		
		// 拷贝构造函数
		SharedDefinitionList(const SharedDefinitionList& other);
		
		// operator=()
		SharedDefinitionList& operator=(const SharedDefinitionList& other);
		
		// 从LocalDefinitionList复制数据到共享对象
		void copyFromLocal(const LocalDefinitionList& local, SharedMemoryAllocator<char> allocator);
		
		// 复制数据到LocalDefinitionList
		void copyToLocal(LocalDefinitionList& local) const;
	};

	// 共享的网格类
	struct SOLVERHUB_API SharedMesh : public SharedDataBase
	{
		SharedMemoryString modelName;	// 对应的几何模型的名称
		SharedMemoryVector<Node> nodes;
		SharedMemoryVector<Edge> Edges;
		SharedMemoryVector<Triangle> Triangles;
		SharedMemoryVector<Tetrahedron> Tetrahedrons;

		SharedMesh(bip::managed_shared_memory::segment_manager* segment_manager);

		// 拷贝构造函数
		SharedMesh(const SharedMesh& other);

		// operator=()
		SharedMesh& operator=(const SharedMesh& other);

		// 从LocalMesh复制数据到共享对象
		void copyFromLocal(const LocalMesh& local, SharedMemoryAllocator<char> allocator);
		
		// 复制数据到LocalMesh
		void copyToLocal(LocalMesh& local) const;
	};

	///共享的场和全局量的计算数据
	struct SOLVERHUB_API SharedData : public SharedDataBase
	{
		bool isFieldData;			    // 是否是场数据
		double t;                       // 数据对应的耦合计算时刻
		DataGeoType type;			    // 数据类型
		bool isSequentiallyMatchedWithMesh; // 数据是否与网格顺序匹配
		SharedMemoryString meshName;         // 网格名
		SharedMemoryVectorPair dimtags;	     // 数据对应的几何位置 (dimension, tag)
		SharedMemoryVector<int> index;	     // 数据索引, 指定数据对应的网格结点、边、面或体 element 的 id
		SharedMemoryVector<double> data;	 // 数据值

		SharedData(bip::managed_shared_memory::segment_manager* segment_manager);

		// 拷贝构造函数
		SharedData(const SharedData& other);

		// operator=()
		SharedData& operator=(const SharedData& other);

		// 从LocalData复制数据到共享对象
		void copyFromLocal(const LocalData& local, SharedMemoryAllocator<char> allocator);
		
		// 复制数据到LocalData
		void copyToLocal(LocalData& local) const;
	};

	// Exception structure for inter-process exception handling
	struct SOLVERHUB_API SharedException {
		std::atomic<bool> hasException;
		std::atomic<int> exceptionType;
		std::atomic<int> exceptionCode;
		SharedMemoryString exceptionMessage;

		SharedException(bip::managed_shared_memory::segment_manager* segment_manager);
		
		// Copy constructor
		SharedException(const SharedException& other);
		
		// operator=()
		SharedException& operator=(const SharedException& other);
	};

	// 非共享的控制数据结构
	struct SOLVERHUB_API LocalControlData : public LocalDataBase
	{
		std::string jsonConfig;             // json配置字段
		double dt;                          // 时间步长
		double t;                           // 当前时刻
		bool isConverged;                   // 是否收敛
		std::vector<std::string> modelNames; // 几何共享数据名列表
		std::vector<int> modelMemorySizes;		 // 几何共享数据所需内存大小列表
		std::vector<std::string> meshNames;  // 网格共享数据名列表
		std::vector<int> meshMemorySizes;     // 网格共享数据所需内存大小列表
		std::vector<std::string> dataNames;  // 数据共享数据名列表
		std::vector<int> dataMemorySizes;			 // 数据共享数据所需内存大小列表
		std::vector<std::string> definitionNames;  // 模型参数共享数据名列表
		std::vector<int> definitionMemorySizes;    // 模型参数共享数据所需内存大小列表

		// 添加内存段信息
		size_t geometrySegmentTotalSize;    // 几何共享内存段总大小
		size_t geometrySegmentFreeSize;     // 几何共享内存段可用空间大小
		
		size_t meshSegmentTotalSize;        // 网格共享内存段总大小
		size_t meshSegmentFreeSize;         // 网格共享内存段可用空间大小
		
		size_t dataSegmentTotalSize;        // 计算数据共享内存段总大小
		size_t dataSegmentFreeSize;         // 计算数据共享内存段可用空间大小
		
		size_t controlSegmentTotalSize;     // 控制数据共享内存段总大小
		size_t controlSegmentFreeSize;      // 控制数据共享内存段可用空间大小
		
		size_t definitionSegmentTotalSize;  // 模型参数共享内存段总大小
		size_t definitionSegmentFreeSize;   // 模型参数共享内存段可用空间大小

		// 默认构造函数
		LocalControlData();

		// 带参数的构造函数
		LocalControlData(const std::string& name_, const std::string& jsonConfig_);
			
		// 实现LocalDataBase接口
		DataType getDataType() const override;
	};

	/// 共享耦合控制数据
	struct SOLVERHUB_API SharedControlData : public SharedDataBase
	{
		SharedMemoryString jsonConfig;  // json配置字段
		double dt;                      // 时间步长
		double t;                       // 当前时刻
		bool isConverged;               // 是否收敛
		
		SharedException exception;	    // 异常信息

		SharedMemoryVectorString sharedModelNames;  // 几何共享数据名
		SharedMemoryVector<int> sharedModelMemorySizes; // 几何共享数据所需内存大小

		SharedMemoryVectorString sharedMeshNames;   // 网格共享数据名
		SharedMemoryVector<int> sharedMeshMemorySizes; // 网格共享数据所需内存大小

		SharedMemoryVectorString sharedDataNames;   // 数据共享数据名
		SharedMemoryVector<int> sharedDataMemorySizes; // 数据共享数据所需内存大小
		
		SharedMemoryVectorString sharedDefinitionNames;   // 模型参数共享数据名
		SharedMemoryVector<int> sharedDefinitionMemorySizes; // 模型参数共享数据所需内存大小

		// 添加共享内存段大小和可用空间信息
		size_t geometrySegmentTotalSize;    // 几何共享内存段总大小
		size_t geometrySegmentFreeSize;     // 几何共享内存段可用空间大小
		
		size_t meshSegmentTotalSize;        // 网格共享内存段总大小
		size_t meshSegmentFreeSize;         // 网格共享内存段可用空间大小
		
		size_t dataSegmentTotalSize;        // 计算数据共享内存段总大小
		size_t dataSegmentFreeSize;         // 计算数据共享内存段可用空间大小
		
		size_t controlSegmentTotalSize;     // 控制数据共享内存段总大小
		size_t controlSegmentFreeSize;      // 控制数据共享内存段可用空间大小
		
		size_t definitionSegmentTotalSize;  // 模型参数共享内存段总大小
		size_t definitionSegmentFreeSize;   // 模型参数共享内存段可用空间大小

		SharedControlData(bip::managed_shared_memory::segment_manager* segment_manager);

		// 拷贝构造函数
		SharedControlData(const SharedControlData& other);

		// operator=()
		SharedControlData& operator=(const SharedControlData& other);

		// 从LocalControlData复制数据到共享对象
		void copyFromLocal(const LocalControlData& local, SharedMemoryAllocator<char> allocator);
		
		// 复制数据到LocalControlData
		void copyToLocal(LocalControlData& local) const;
	};
}
#endif
