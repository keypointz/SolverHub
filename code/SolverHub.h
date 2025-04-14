#ifndef SOLVERHUB_H
#define SOLVERHUB_H

#include "SolverHubDef.h"
#include "CouplingPart.h"
#include "GModel.h"
#include "ISolverContext.h"
#include "ISolverProgress.h"
#include "ILogger.h"

namespace EMP {

	/// 存储耦合界面历史数据的数据结构
	struct Tdata
	{
		std::string name;
		std::string meshName;
		DataGeoType type;			   // 数据类型
		std::deque<ArrayXd> data;
		std::deque<double> time; // 数据对应的求解时刻
		ArrayXi pos;   // 数据对应的网格位置
		bool isSequentiallyMatchedWithMesh; // 数据是否与网格顺序匹配
		int size;
	};

	// 作为 SharedMemoryManager 的 creator 
	// 开辟共享内存，用于存储耦合数据
	// 与某个 CouplingPart 进行耦合数据 SharedMemoryData 交换的界面
	// 存储历史数据，用于耦合计算和收敛性判断
	// CouplingPart 与 Interface 一一对应
	// Solverhub 通过 Interface 调用基于 CouplingPart 编译的可执行文件并进行数据交换
	class SOLVERHUB_API Interface : public CouplingPart {
	public:
		std::string name;
		std::string path; /// solver的工作路径
		std::string solver; /// solver 的可执行文件名
		// vector of strings containing command line arguments to be used to launch the solver
		std::vector<std::string> strargs; /// 调用外部求解器的命令行

		std::vector<std::pair<int, int>> dimtag; // (dimension, tag) 标记耦合界面的几何维度和几何元素编号

		std::vector<Tdata> dataPool; // 存储该界面上的数据的数据池
		std::map<std::string, Tdata*> dataMap; /// 用于通过名字查找 data
		std::map<std::string, std::string> dataWriter; /// 记录数据的 writer, 一个数据只有一个 writer
		std::map<std::string, std::set<std::string>> dataReader; /// 记录数据的 reader, 一个数据可以有多个 reader 
		
		std::map<std::string, int> memorySize; /// 记录共享数据的大小

		bool isDebug;
	public:
		Interface();
		virtual ~Interface();
		int addData(std::string dname, int storesize = 2);
		int setData(std::string dname, double t, ArrayXd& data);
		int setData(std::string dname, double t, std::vector<double>& data);
		int getData(std::string dname, double t, ArrayXd& data);
		int getData(std::string dname, double t, std::vector<double>& data);
		int getDataByPos(std::string dname, int i, ArrayXd& data);

		int GenerateSharedControlData();
		int GenerateSharedGeometry();
		int GenerateSharedMesh();
		int GenerateSharedData();
		int GenerateSharedData(std::string dataName, double t, std::string meshName, DataGeoType type, bool isSequentiallyMatchedWithMesh);
	};

	// 耦合管理器，用于管理耦合计算的数据交换和时间同步
	// 负责不同 interface 之间的数据交换（不同网格之间数据映射）
	class SOLVERHUB_API SolverHub {
	public:
		std::string _name;
		std::string _workingPath;

		std::map<std::string, Interface *> interfaces; /// Interface 与其名字的对应表

		std::string _fileName;
		std::set<std::string> _fileNames;

		static int _current;
		bool _destroying;
		double timestamp;
		double t_final;
		ISolverContext* context;
		ISolverProgress* progbar;
		EMP::ILogger * logger;
	public:
		SolverHub(const std::string &name = "");
		virtual ~SolverHub();
		
		int init(ISolverContext* context_ = nullptr,
			ISolverProgress* progbar_ = nullptr,
			EMP::ILogger * logger_ = nullptr);

		void destroy(bool keepName = false);
		bool isBeingDestroyed() const { return _destroying; }
		static std::vector<SolverHub *> list;
		static SolverHub *current(int index = -1);
		//static int setCurrent(SolverHub *m);
		//int setAsCurrent() { return setCurrent(this); }
		static SolverHub *findByName(const std::string name,
			const std::string fileName = "");
		static void deleteCouplingSystems();

		void setName(const std::string &name) { _name = name; }
		std::string getName() const { return _name; }

		// get/set the model file name
		void setFileName(const std::string &fileName);
		std::string getFileName() const { return _fileName; }
		bool hasFileName(const std::string &name) const
		{
			return _fileNames.find(name) != _fileNames.end();
		}

		Interface* getInterfaceByName(std::string name) { return interfaces[name]; }

		void add(Interface *loc);
		void remove(Interface *r);
		bool empty() const;

		// 在界面上交互数据
		int exchangedata(std::string interfaceName, std::string dataName);

		// 耦合计算的主循环。依次调用 interface 对应的求解器。
		int run();
		bool continueRun();
		void updateSystemTime();

		/// 通过读入.json 耦合配置文件，建立与求解器对接的耦合界面 interface.
		int readConfig(std::string fileName);
	};
}
#endif
