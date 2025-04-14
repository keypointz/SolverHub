#ifndef SOLVER_H
#define SOLVER_H

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
	enum SolverType {
		UnknownSolver = 0,
		TransientSolver = 1,
		SteadySolver = 2,
		MeshSolver = 3,
		PostSolver = 4
	};

	/// 定义参与耦合计算的求解器接口
	class SOLVERHUB_API Solver {
	public:
		std::string name;                             // 耦合部件名称
		SolverType solverType;					      // 求解器类型
		int _index;                                   // 耦合顺序号，表示在耦合序列中的位置
		std::vector<GModel*> modelList;               // 几何模型列表，用于获取几何信息和网格信息
		std::vector<UniMesh*> meshList;               // 网格模型列表，用于存储网格信息
		LocalControlData localCtrlData;               // 本地控制数据，包含时间步长、当前时间等信息
		LocalDefinitionList localDefinitionList;       // 本地模型参数定义列表
		std::shared_ptr<SharedMemoryManager> sharedMemoryManager; // 共享内存管理器，用于与其他部件通信

		std::string workingPath;                      // 工作目录路径
		std::string inputFileName;                    // 输入文件名
		std::vector<std::string> outputFileNames;     // 输出文件名列表

	public:
		/**
		 * @brief 构造函数
		 * @param _name 部件名称
		 */
		Solver(std::string _name = "part");
		
		/**
		 * @brief 析构函数
		 */
		virtual ~Solver();

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
	};

}
#endif
