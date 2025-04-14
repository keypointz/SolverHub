#ifndef COUPLINGPART_H
#define COUPLINGPART_H

#include "SharedMemoryStruct.h"
#include "SharedMemoryManager.h"
#include "GModel.h"
#include "UniMesh.h"
#include "mesh_point.h"
#include "mesh_edge.h"
#include "mesh_facet.h"
#include "mesh_block.h"

namespace EMP {

	// 求解器类型
	enum CouplingType {
		UnknownCoupling = 0,
		TransientCoupling = 1,
		SteadyCoupling = 2,
		MeshCoupling = 3,
		PostCoupling = 4
	};

	/// 定义参与耦合计算的部件接口
	class SOLVERHUB_API CouplingPart {
	public:
		std::string name;                             // 耦合部件名称
		CouplingType couplingType;					  // 耦合类型
		int _index;                                   // 耦合顺序号，表示在耦合序列中的位置
		std::vector<GModel*> modelList;               // 几何模型列表，用于获取几何信息和网格信息
		std::vector<UniMesh*> meshList;               // 网格模型列表，用于存储网格信息
		LocalControlData localCtrlData;               // 本地控制数据，包含时间步长、当前时间等信息
		LocalDefinitionList localDefinitionList;       // 本地模型参数定义列表
		std::shared_ptr<SharedMemoryManager> sharedMemoryManager; // 共享内存管理器，用于与其他部件通信

		std::string workingPath;                      // 工作目录路径
		std::string inputFileName;                    // 输入文件名
		std::vector<std::string> outputFileNames;     // 输出文件名列表
		
		// 输入输出定义
		struct IODefinition {
			std::string name;              // 输入/输出名称
			DataType type; // 数据类型
			bool isRequired;               // 是否必需
			bool isList;                   // 是否是列表（多个同类型数据）
			std::vector<std::string> tags; // 标签，用于进一步描述数据特征
			
			IODefinition(const std::string& name_, DataType type_, 
				bool isRequired_ = true, bool isList_ = false)
				: name(name_), type(type_), isRequired(isRequired_), isList(isList_) {}
				
			// 添加标签
			void addTag(const std::string& tag) {
				tags.push_back(tag);
			}
		};
		
		std::vector<IODefinition> inputs;   // 输入定义列表
		std::vector<IODefinition> outputs;  // 输出定义列表

	public:
		/**
		 * @brief 构造函数
		 * @param _name 部件名称
		 */
		CouplingPart(std::string _name = "part");
		
		/**
		 * @brief 析构函数
		 */
		virtual ~CouplingPart();

		/**
		 * @brief 初始化部件
		 * 读取输入文件，初始化共享数据名称，读入初始控制字段
		 * @return 返回初始化状态码，0表示成功
		 */
		virtual int init();
		
		/**
		 * @brief 执行单步计算
		 * @return 返回计算状态码，0表示成功
		 */
		virtual int step();

		/**
		 * @brief 停止计算并清理资源
		 * @return 返回停止状态码，0表示成功
		 */
		virtual int stop();
		
		/**
		 * @brief 添加输入定义
		 * @param name 输入名称
		 * @param type 数据类型
		 * @param isRequired 是否必需
		 * @param isList 是否是列表
		 * @return 返回添加的输入定义引用
		 */
		IODefinition& addInput(const std::string& name, DataType type, 
			bool isRequired = true, bool isList = false);
			
		/**
		 * @brief 添加输出定义
		 * @param name 输出名称
		 * @param type 数据类型
		 * @param isRequired 是否必需
		 * @param isList 是否是列表
		 * @return 返回添加的输出定义引用
		 */
		IODefinition& addOutput(const std::string& name, DataType type, 
			bool isRequired = true, bool isList = false);
			
		/**
		 * @brief 获取输入定义
		 * @param name 输入名称
		 * @return 返回找到的输入定义指针，未找到则返回nullptr
		 */
		IODefinition* getInputDefinition(const std::string& name);
		
		/**
		 * @brief 获取输出定义
		 * @param name 输出名称
		 * @return 返回找到的输出定义指针，未找到则返回nullptr
		 */
		IODefinition* getOutputDefinition(const std::string& name);
		
		/**
		 * @brief 验证输入输出定义是否满足要求
		 * @return 返回验证状态码，0表示成功
		 */
		int validateIODefinitions();
		
		/**
		 * @brief 从共享控制数据中读取控制数据
		 * @return 返回读取状态码，0表示成功
		 */
		int readControlDataFromSharedControlData();
		
		/**
		 * @brief 从共享几何数据生成几何模型
		 * @return 返回生成状态码，0表示成功
		 */
		int generateGModelFromSharedGeometry();
		
		/**
		 * @brief 从共享网格数据生成统一网格
		 * @return 返回生成状态码，0表示成功
		 */
		int generateUniMeshFromSharedMesh();
		
		/**
		 * @brief 从共享定义数据读取模型参数
		 * @param definitionName 定义名称
		 * @return 返回读取状态码，0表示成功
		 */
		int readDefinitionFromSharedDefinition(const std::string& definitionName);
		
		/**
		 * @brief 将模型参数写入共享定义数据
		 * @param definitionName 定义名称
		 * @return 返回写入状态码，0表示成功
		 */
		int writeDefinitionToSharedDefinition(const std::string& definitionName);
		
		/**
		 * @brief 将统一网格写入共享网格数据
		 * @param meshname 网格名称
		 * @return 返回写入状态码，0表示成功
		 */
		int writeUniMeshToSharedMesh(std::string meshname);
		
		/**
		 * @brief 将本地网格转换为统一网格
		 * @param localMesh 本地网格指针
		 * @param uniMesh 统一网格指针
		 * @return 返回转换状态码，0表示成功
		 */
		int localMesh2UniMesh(LocalMesh* localMesh, UniMesh* uniMesh);

		int localMeshNodes2mesh_PointList(LocalMesh* localMesh, mesh_PointList* pointList);

        int localMeshFacets2mesh_FacetList(LocalMesh* localMesh, mesh_PointList* pointList, mesh_FacetList* facetList,
			int** edge_ref);

		int localMeshBlocks2mesh_BlockList(LocalMesh* localMesh, mesh_PointList* pointList, mesh_BlockList* blockList);

		void assign_ref2mesh_EdgeList(LocalMesh* localMesh, mesh_EdgeList* edgeList);
		
		/**
		 * @brief 将统一网格转换为本地网格
		 * @param uniMesh 统一网格指针
		 * @param localMesh 本地网格指针
		 * @return 返回转换状态码，0表示成功
		 */
		int uniMesh2LocalMesh(UniMesh* uniMesh, LocalMesh* localMesh, bool onlyOnGEntity = true);
		
		/**
		 * @brief 从共享数据中读取数据
		 * @param dataName 数据名称
		 * @param t 时间变量，用于存储读取的时间
		 * @param data 数据数组，用于存储读取的数据
		 * @param pos 位置索引数组
		 * @return 返回读取状态码，0表示成功
		 */
		int readDataFromSharedDatas(std::string dataName, double& t, ArrayXd& data, ArrayXi& pos);
		
		/**
		 * @brief 从共享数据中读取数据
		 * @param dataName 数据名称
		 * @param data 本地数据结构，用于存储读取的数据
		 * @return 返回读取状态码，0表示成功
		 */
		int readDataFromSharedDatas(std::string dataName, LocalData& data);
		
		/**
		 * @brief 将数据写入共享数据
		 * @param dataName 数据名称
		 * @param t 时间变量
		 * @param data 要写入的数据数组
		 * @param pos 位置索引数组
		 * @return 返回写入状态码，0表示成功
		 */
		int writeDataToSharedDatas(std::string dataName, double& t, const ArrayXd& data, const ArrayXi& pos);
		
		/**
		 * @brief 将数据写入共享数据
		 * @param dataName 数据名称
		 * @param data 要写入的本地数据结构
		 * @return 返回写入状态码，0表示成功
		 */
		int writeDataToSharedDatas(std::string dataName, LocalData& data);
		
		/**
		 * @brief 通过名称获取网格
		 * @param name 网格名称
		 * @return 返回网格指针，找不到则返回nullptr
		 */
		UniMesh* getMeshByName(std::string name);
		
		/**
		 * @brief 通过名称获取几何模型
		 * @param name 模型名称
		 * @return 返回几何模型指针，找不到则返回nullptr
		 */
		GModel* getModelByName(std::string name);
		
		/**
		 * @brief 设置异常信息
		 * @param type 异常类型
		 * @param code 异常代码
		 * @param message 异常消息
		 */
		void setException(int type, int code, const std::string& message);
		
		/**
		 * @brief 获取并清除异常信息
		 * @return 返回异常信息元组（类型、代码、消息）
		 */
		std::tuple<int, int, std::string> getAndClearException();
		
		/**
		 * @brief 获取耦合顺序号
		 * @return 返回耦合顺序号
		 */
		int index() const { return _index; }
		
		/**
		 * @brief 获取时间步长
		 * @return 返回时间步长
		 */
		double getTimeStep();
		
		/**
		 * @brief 获取当前时间
		 * @return 返回当前时间
		 */
		double getTime();
		
		/**
		 * @brief 设置时间步长
		 * @param dt_ 新的时间步长
		 */
		void setTimeStep(double dt_);
		
		/**
		 * @brief 设置当前时间
		 * @param t_ 新的当前时间
		 */
		void setTime(double t_);

		/**
		 * @brief 从JSON文件加载输入输出定义
		 * @param jsonFilePath JSON文件路径
		 * @return 返回读取状态码，0表示成功
		 */
		int loadIODefinitionsFromJson(const std::string& jsonFilePath);
		
		/**
		 * @brief 将输入输出定义导出为JSON字符串
		 * @return 返回包含输入输出定义的JSON字符串
		 */
		std::string exportIODefinitionsToJson() const;
	};

}
#endif
