#ifndef SOLVERHUB_H
#define SOLVERHUB_H

#include "SolverHubDef.h"
#include "CouplingPart.h"
#include "GModel.h"
#include "ISolverContext.h"
#include "ISolverProgress.h"
#include "ILogger.h"

namespace EMP {

	/// �洢��Ͻ�����ʷ���ݵ����ݽṹ
	struct Tdata
	{
		std::string name;
		std::string meshName;
		DataGeoType type;			   // ��������
		std::deque<ArrayXd> data;
		std::deque<double> time; // ���ݶ�Ӧ�����ʱ��
		ArrayXi pos;   // ���ݶ�Ӧ������λ��
		bool isSequentiallyMatchedWithMesh; // �����Ƿ�������˳��ƥ��
		int size;
	};

	// ��Ϊ SharedMemoryManager �� creator 
	// ���ٹ����ڴ棬���ڴ洢�������
	// ��ĳ�� CouplingPart ����������� SharedMemoryData �����Ľ���
	// �洢��ʷ���ݣ�������ϼ�����������ж�
	// CouplingPart �� Interface һһ��Ӧ
	// Solverhub ͨ�� Interface ���û��� CouplingPart ����Ŀ�ִ���ļ����������ݽ���
	class SOLVERHUB_API Interface : public CouplingPart {
	public:
		std::string name;
		std::string path; /// solver�Ĺ���·��
		std::string solver; /// solver �Ŀ�ִ���ļ���
		// vector of strings containing command line arguments to be used to launch the solver
		std::vector<std::string> strargs; /// �����ⲿ�������������

		std::vector<std::pair<int, int>> dimtag; // (dimension, tag) �����Ͻ���ļ���ά�Ⱥͼ���Ԫ�ر��

		std::vector<Tdata> dataPool; // �洢�ý����ϵ����ݵ����ݳ�
		std::map<std::string, Tdata*> dataMap; /// ����ͨ�����ֲ��� data
		std::map<std::string, std::string> dataWriter; /// ��¼���ݵ� writer, һ������ֻ��һ�� writer
		std::map<std::string, std::set<std::string>> dataReader; /// ��¼���ݵ� reader, һ�����ݿ����ж�� reader 
		
		std::map<std::string, int> memorySize; /// ��¼�������ݵĴ�С

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

	// ��Ϲ����������ڹ�����ϼ�������ݽ�����ʱ��ͬ��
	// ����ͬ interface ֮������ݽ�������ͬ����֮������ӳ�䣩
	class SOLVERHUB_API SolverHub {
	public:
		std::string _name;
		std::string _workingPath;

		std::map<std::string, Interface *> interfaces; /// Interface �������ֵĶ�Ӧ��

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

		// �ڽ����Ͻ�������
		int exchangedata(std::string interfaceName, std::string dataName);

		// ��ϼ������ѭ�������ε��� interface ��Ӧ���������
		int run();
		bool continueRun();
		void updateSystemTime();

		/// ͨ������.json ��������ļ���������������Խӵ���Ͻ��� interface.
		int readConfig(std::string fileName);
	};
}
#endif
