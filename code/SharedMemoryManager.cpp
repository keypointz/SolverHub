#include "SharedMemoryManager.h"
#include <iostream> // For std::cerr
#include <fstream>  // For file operations
#include <iomanip>  // For std::hex, std::setw, etc.
#include <cstdint>  // For uint32_t, uint64_t
#include <algorithm> // For std::max
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <filesystem> // For directory operations

namespace bip = boost::interprocess;
namespace fs = std::filesystem;

using namespace EMP;

SharedMemoryManager::SharedMemoryManager(const std::string& memoryName, bool isCreator, const std::string& prefix, size_t control_memory_size, const std::string& logFilePath)
	: memoryName_(prefix.empty() ? memoryName : prefix + "_" + memoryName),
	isCreator_(isCreator),
	prefix_(prefix)
{
	// 创建日志记录器（如果提供了日志文件路径）
	if (!logFilePath.empty()) {
		if (isCreator) {
			logger_ = std::make_unique<CreatorLogger>(logFilePath);
		} else {
			logger_ = std::make_unique<ClientLogger>(logFilePath);
		}
	}

	try {
		// 创建或打开共享互斥锁，用于同步访问
		std::string mutexName = prefix_.empty() ? memoryName_ + "_mutex" : prefix_ + "_" + memoryName_ + "_mutex";
		sharedMutex_ = std::make_shared<bip::named_mutex>(bip::open_or_create, mutexName.c_str());

		// 如果是创建者，则清除已存在的共享内存（如果有）
		if (isCreator_) {
			// 只创建控制数据内存段
			std::string ctrlSegmentName = GenerateSegmentName(SharedMemorySuffix::CONTROL_SEGMENT);
			bip::shared_memory_object::remove(ctrlSegmentName.c_str());

			// 创建控制数据内存段
			controlSegment_ = std::make_shared<bip::managed_shared_memory>(
				bip::create_only, ctrlSegmentName.c_str(), control_memory_size);

			// 创建控制数据对象
			controlData_ = controlSegment_->construct<SharedControlData>("ControlData")
				(controlSegment_->get_segment_manager());

			// 初始化控制数据内存段信息
			auto usage = getControlMemoryUsage();
			controlData_->controlSegmentTotalSize = usage.first;
			controlData_->controlSegmentFreeSize = usage.first - usage.second;

			log(LogLevel::Info, "创建控制数据内存段: " + ctrlSegmentName + ", 大小: " + std::to_string(control_memory_size) + " 字节");
		}
		else {
			// 如果不是创建者，则连接到已存在的共享内存控制段
			std::string ctrlSegmentName = GenerateSegmentName(SharedMemorySuffix::CONTROL_SEGMENT);
			try {
				controlSegment_ = std::make_shared<bip::managed_shared_memory>(
					bip::open_only, ctrlSegmentName.c_str());

				// 查找控制数据对象
				std::pair<SharedControlData*, bip::managed_shared_memory::size_type> res =
					controlSegment_->find<SharedControlData>("ControlData");
				if (res.first) {
					controlData_ = res.first;
					log(LogLevel::Info, "连接到控制数据内存段: " + ctrlSegmentName);
				}
				else {
					log(LogLevel::Error, "控制数据内存段存在但找不到控制数据对象");
					throw std::runtime_error("无法找到控制数据对象");
				}
			}
			catch (const bip::interprocess_exception& ex) {
				log(LogLevel::Error, "连接控制数据内存段失败: " + std::string(ex.what()));
				throw;
			}
		}
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "初始化共享内存管理器失败: " + std::string(e.what()));
		throw;
	}
}

// 生成共享内存段名称
std::string SharedMemoryManager::GenerateSegmentName(const std::string& suffix) {
	return prefix_.empty() ? memoryName_ + suffix : prefix_ + "_" + memoryName_ + suffix;
}

// 析构函数
SharedMemoryManager::~SharedMemoryManager() {
	try {
		// 释放资源
		geos_.clear();
		meshs_.clear();
		datas_.clear();
		defs_.clear();
		controlData_ = nullptr;

		// 关闭所有内存段
		controlSegment_.reset();
		geometrySegment_.reset();
		meshSegment_.reset();
		dataSegment_.reset();
		definitionSegment_.reset();

		// 如果是创建者，则删除共享内存段
		if (isCreator_) {
			std::string ctrlSegmentName = GenerateSegmentName(SharedMemorySuffix::CONTROL_SEGMENT);
			bip::shared_memory_object::remove(ctrlSegmentName.c_str());

			// 尝试删除其他内存段（如果存在）
			std::string geoSegmentName = GenerateSegmentName(SharedMemorySuffix::GEOMETRY_SEGMENT);
			bip::shared_memory_object::remove(geoSegmentName.c_str());

			std::string meshSegmentName = GenerateSegmentName(SharedMemorySuffix::MESH_SEGMENT);
			bip::shared_memory_object::remove(meshSegmentName.c_str());

			std::string dataSegmentName = GenerateSegmentName(SharedMemorySuffix::DATA_SEGMENT);
			bip::shared_memory_object::remove(dataSegmentName.c_str());

			std::string defSegmentName = GenerateSegmentName(SharedMemorySuffix::DEFINITION_SEGMENT);
			bip::shared_memory_object::remove(defSegmentName.c_str());

			// 删除互斥锁
			std::string mutexName = prefix_.empty() ? memoryName_ + "_mutex" : prefix_ + "_" + memoryName_ + "_mutex";
			bip::named_mutex::remove(mutexName.c_str());

			log(LogLevel::Info, "所有共享内存段和互斥锁已清理");
		}
	}
	catch (const std::exception& e) {
		if (logger_) {
			log(LogLevel::Error, "析构共享内存管理器时出错: " + std::string(e.what()));
		}
	}
}

// 记录日志的辅助函数
void SharedMemoryManager::log(LogLevel level, const std::string& message) {
	if (logger_) {
		logger_->log(level, message);
	}
}

// 设置日志级别
void SharedMemoryManager::setLogLevel(LogLevel level) {
	if (logger_) {
		logger_->setLogLevel(level);
	}
}

// 创建几何对象共享内存段和对象
void SharedMemoryManager::createGeometrySegmentAndObjects() {
	if (!isCreator_) {
		log(LogLevel::Warning, "非创建者不能创建几何对象内存段");
		return;
	}

	try {
		// 获取几何数据所需内存大小
		size_t totalSize = 0;
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 检查是否有内存大小信息
		if (localCtrl.modelNames.size() != localCtrl.modelMemorySizes.size()) {
			log(LogLevel::Warning, "几何数据名称数量与内存大小数量不匹配");
			return;
		}

		// 计算总大小，增加10%作为额外空间
		for (auto size : localCtrl.modelMemorySizes) {
			totalSize += size;
		}
		totalSize = static_cast<size_t>(totalSize * 1.1);

		// 如果总大小太小，使用默认大小
		if (totalSize < 1024 * 1024) {
			totalSize = 1024 * 1024;
		}

		// 创建几何数据内存段
		std::string geoSegmentName = GenerateSegmentName(SharedMemorySuffix::GEOMETRY_SEGMENT);
		bip::shared_memory_object::remove(geoSegmentName.c_str()); // 先移除现有的（如果存在）

		geometrySegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, geoSegmentName.c_str(), totalSize);

		// 清空当前几何对象列表
		geos_.clear();

		// 创建几何对象
		for (const auto& name : localCtrl.modelNames) {
			std::string objName = name + SharedMemorySuffix::GEOMETRY;
			SharedGeometry* geo = geometrySegment_->construct<SharedGeometry>(objName.c_str())
				(geometrySegment_->get_segment_manager());
			geos_.push_back(geo);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "创建几何数据内存段: " + geoSegmentName + ", 大小: " + std::to_string(totalSize) + " 字节");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "创建几何数据内存段失败: " + std::string(e.what()));
		throw;
	}
}

// 创建网格对象共享内存段和对象
void SharedMemoryManager::createMeshSegmentAndObjects() {
	if (!isCreator_) {
		log(LogLevel::Warning, "非创建者不能创建网格对象内存段");
		return;
	}

	try {
		// 获取网格数据所需内存大小
		size_t totalSize = 0;
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 检查是否有内存大小信息
		if (localCtrl.meshNames.size() != localCtrl.meshMemorySizes.size()) {
			log(LogLevel::Warning, "网格数据名称数量与内存大小数量不匹配");
			return;
		}

		// 计算总大小，增加10%作为额外空间
		for (auto size : localCtrl.meshMemorySizes) {
			totalSize += size;
		}
		totalSize = static_cast<size_t>(totalSize * 1.1);

		// 如果总大小太小，使用默认大小
		if (totalSize < 1024 * 1024) {
			totalSize = 1024 * 1024;
		}

		// 创建网格数据内存段
		std::string meshSegmentName = GenerateSegmentName(SharedMemorySuffix::MESH_SEGMENT);
		bip::shared_memory_object::remove(meshSegmentName.c_str()); // 先移除现有的（如果存在）

		meshSegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, meshSegmentName.c_str(), totalSize);

		// 清空当前网格对象列表
		meshs_.clear();

		// 创建网格对象
		for (const auto& name : localCtrl.meshNames) {
			std::string objName = name + SharedMemorySuffix::MESH;
			SharedMesh* mesh = meshSegment_->construct<SharedMesh>(objName.c_str())
				(meshSegment_->get_segment_manager());
			meshs_.push_back(mesh);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "创建网格数据内存段: " + meshSegmentName + ", 大小: " + std::to_string(totalSize) + " 字节");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "创建网格数据内存段失败: " + std::string(e.what()));
		throw;
	}
}

// 创建计算数据对象共享内存段和对象
void SharedMemoryManager::createDataSegmentAndObjects() {
	if (!isCreator_) {
		log(LogLevel::Warning, "非创建者不能创建计算数据内存段");
		return;
	}

	try {
		// 获取计算数据所需内存大小
		size_t totalSize = 0;
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 检查是否有内存大小信息
		if (localCtrl.dataNames.size() != localCtrl.dataMemorySizes.size()) {
			log(LogLevel::Warning, "计算数据名称数量与内存大小数量不匹配");
			return;
		}

		// 计算总大小，增加10%作为额外空间
		for (auto size : localCtrl.dataMemorySizes) {
			totalSize += size;
		}
		totalSize = static_cast<size_t>(totalSize * 1.1);

		// 如果总大小太小，使用默认大小
		if (totalSize < 1024 * 1024) {
			totalSize = 1024 * 1024;
		}

		// 创建计算数据内存段
		std::string dataSegmentName = GenerateSegmentName(SharedMemorySuffix::DATA_SEGMENT);
		bip::shared_memory_object::remove(dataSegmentName.c_str()); // 先移除现有的（如果存在）

		dataSegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, dataSegmentName.c_str(), totalSize);

		// 清空当前计算数据对象列表
		datas_.clear();

		// 创建计算数据对象
		for (const auto& name : localCtrl.dataNames) {
			std::string objName = name + SharedMemorySuffix::DATA;
			SharedData* data = dataSegment_->construct<SharedData>(objName.c_str())
				(dataSegment_->get_segment_manager());
			datas_.push_back(data);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "创建计算数据内存段: " + dataSegmentName + ", 大小: " + std::to_string(totalSize) + " 字节");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "创建计算数据内存段失败: " + std::string(e.what()));
		throw;
	}
}

// 内存大小估算函数

// 估算几何对象所需的共享内存大小
size_t SharedMemoryManager::estimateGeometryMemorySize(const LocalGeometry& localGeo) {
	// 基础大小
	size_t baseSize = sizeof(SharedGeometry);

	// 向量开销
	size_t vectorOverhead = 2 * sizeof(SharedMemoryVectorString); // 两个向量对象的开销

	// 计算所有几何体的内存需求
	size_t totalElementsSize = 0;

	// 处理 shapeNames 和 shapeBrps 向量中的数据
	for (size_t i = 0; i < localGeo.shapeNames.size(); i++) {
		size_t nameSize = localGeo.shapeNames[i].size() + 1; // +1 for null terminator
		size_t shapeBrpSize = localGeo.shapeBrps[i].size() + 1;
		size_t stringOverhead = 2 * sizeof(SharedMemoryString); // 两个字符串对象的开销

		totalElementsSize += nameSize + shapeBrpSize + stringOverhead;
	}

	// 总大小 = 基础大小 + 向量开销 + 元素大小
	size_t totalSize = baseSize + vectorOverhead + totalElementsSize;

	// 添加额外的50%作为安全边际，因为多个几何体可能会导致内存分配更加频繁
	return static_cast<size_t>(totalSize * 1.5);
}

// 估算网格对象所需的共享内存大小
size_t SharedMemoryManager::estimateMeshMemorySize(const LocalMesh& localMesh) {
	// 基础大小
	size_t baseSize = sizeof(SharedMesh);

	// 名称大小
	size_t nameSize = localMesh.name.size() + 1;
	size_t modelNameSize = localMesh.modelName.size() + 1;

	// 网格元素数据大小
	size_t nodesSize = localMesh.nodes.size() * sizeof(Node);
	size_t edgesSize = localMesh.edges.size() * sizeof(Edge);
	size_t trianglesSize = localMesh.triangles.size() * sizeof(Triangle);
	size_t tetrahedronsSize = localMesh.tetrahedrons.size() * sizeof(Tetrahedron);

	// 考虑共享内存管理开销
	size_t totalSize = baseSize + nameSize + modelNameSize +
					   nodesSize + edgesSize + trianglesSize + tetrahedronsSize;

	// 添加额外的20%作为安全边际
	return static_cast<size_t>(totalSize * 1.2);
}

// 估算计算数据对象所需的共享内存大小
size_t SharedMemoryManager::estimateDataMemorySize(const LocalData& localData) {
	// 基础大小
	size_t baseSize = sizeof(SharedData);

	// 名称大小
	size_t nameSize = localData.name.size() + 1;
	size_t meshNameSize = localData.meshName.size() + 1;

	// 数据值大小
	size_t indexSize = localData.index.size() * sizeof(int);
	size_t dataSize = localData.data.size() * sizeof(double);
	size_t dimtagsSize = localData.dimtags.size() * sizeof(SharedMemoryPair);

	// 考虑共享内存管理开销
	size_t totalSize = baseSize + nameSize + meshNameSize +
					   indexSize + dataSize + dimtagsSize;

	// 添加额外的20%作为安全边际
	return static_cast<size_t>(totalSize * 1.2);
}

// 获取各个内存段的使用情况

// 返回控制共享内存段的总大小和已使用大小
std::pair<size_t, size_t> SharedMemoryManager::getControlMemoryUsage() const {
	if (controlSegment_) {
		return std::make_pair(controlSegment_->get_size(), controlSegment_->get_size() - controlSegment_->get_free_memory());
	}
	return std::make_pair(0, 0);
}

// 返回几何共享内存段的总大小和已使用大小
std::pair<size_t, size_t> SharedMemoryManager::getGeometryMemoryUsage() const {
	if (geometrySegment_) {
		return std::make_pair(geometrySegment_->get_size(), geometrySegment_->get_size() - geometrySegment_->get_free_memory());
	}
	return std::make_pair(0, 0);
}

// 返回网格共享内存段的总大小和已使用大小
std::pair<size_t, size_t> SharedMemoryManager::getMeshMemoryUsage() const {
	if (meshSegment_) {
		return std::make_pair(meshSegment_->get_size(), meshSegment_->get_size() - meshSegment_->get_free_memory());
	}
	return std::make_pair(0, 0);
}

// 返回数据共享内存段的总大小和已使用大小
std::pair<size_t, size_t> SharedMemoryManager::getDataMemoryUsage() const {
	if (dataSegment_) {
		return std::make_pair(dataSegment_->get_size(), dataSegment_->get_size() - dataSegment_->get_free_memory());
	}
	return std::make_pair(0, 0);
}

// 加载已存在的控制对象
void SharedMemoryManager::LoadExistingControlData() {
	if (!controlSegment_) {
		log(LogLevel::Error, "控制数据内存段未初始化");
		return;
	}

	// 尝试查找控制数据对象
	std::pair<SharedControlData*, bip::managed_shared_memory::size_type> res =
		controlSegment_->find<SharedControlData>("ControlData");

	if (res.first) {
		controlData_ = res.first;
		log(LogLevel::Info, "加载控制数据对象成功");
	}
	else {
		log(LogLevel::Warning, "找不到控制数据对象");
	}
}

// 加载已存在的几何对象
void SharedMemoryManager::LoadExistingGeometryObjects() {
	if (!geometrySegment_) {
		try {
			// 尝试连接到几何数据内存段
			std::string geoSegmentName = GenerateSegmentName(SharedMemorySuffix::GEOMETRY_SEGMENT);
			geometrySegment_ = std::make_shared<bip::managed_shared_memory>(
				bip::open_only, geoSegmentName.c_str());
			log(LogLevel::Info, "连接到几何数据内存段: " + geoSegmentName);
		}
		catch (const bip::interprocess_exception& ex) {
			log(LogLevel::Warning, "连接几何数据内存段失败: " + std::string(ex.what()));
			return;
		}
	}

	// 清空几何对象列表
	geos_.clear();

	// 获取控制数据中的几何对象名称列表
	LocalControlData localCtrl;
	getControlData(localCtrl);

	// 遍历所有几何对象名称，尝试加载
	for (const auto& name : localCtrl.modelNames) {
		std::string objName = name + SharedMemorySuffix::GEOMETRY;
		std::pair<SharedGeometry*, bip::managed_shared_memory::size_type> res =
			geometrySegment_->find<SharedGeometry>(objName.c_str());

		if (res.first) {
			geos_.push_back(res.first);
			log(LogLevel::Info, "加载几何对象成功: " + name);
		}
		else {
			log(LogLevel::Warning, "找不到几何对象: " + name);
		}
	}
}

// 加载已存在的网格对象
void SharedMemoryManager::LoadExistingMeshObjects() {
	if (!meshSegment_) {
		try {
			// 尝试连接到网格数据内存段
			std::string meshSegmentName = GenerateSegmentName(SharedMemorySuffix::MESH_SEGMENT);
			meshSegment_ = std::make_shared<bip::managed_shared_memory>(
				bip::open_only, meshSegmentName.c_str());
			log(LogLevel::Info, "连接到网格数据内存段: " + meshSegmentName);
		}
		catch (const bip::interprocess_exception& ex) {
			log(LogLevel::Warning, "连接网格数据内存段失败: " + std::string(ex.what()));
			return;
		}
	}

	// 清空网格对象列表
	meshs_.clear();

	// 获取控制数据中的网格对象名称列表
	LocalControlData localCtrl;
	getControlData(localCtrl);

	// 遍历所有网格对象名称，尝试加载
	for (const auto& name : localCtrl.meshNames) {
		std::string objName = name + SharedMemorySuffix::MESH;
		std::pair<SharedMesh*, bip::managed_shared_memory::size_type> res =
			meshSegment_->find<SharedMesh>(objName.c_str());

		if (res.first) {
			meshs_.push_back(res.first);
			log(LogLevel::Info, "加载网格对象成功: " + name);
		}
		else {
			log(LogLevel::Warning, "找不到网格对象: " + name);
		}
	}
}

// 加载已存在的数据对象
void SharedMemoryManager::LoadExistingDataObjects() {
	if (!dataSegment_) {
		try {
			// 尝试连接到计算数据内存段
			std::string dataSegmentName = GenerateSegmentName(SharedMemorySuffix::DATA_SEGMENT);
			dataSegment_ = std::make_shared<bip::managed_shared_memory>(
				bip::open_only, dataSegmentName.c_str());
			log(LogLevel::Info, "连接到计算数据内存段: " + dataSegmentName);
		}
		catch (const bip::interprocess_exception& ex) {
			log(LogLevel::Warning, "连接计算数据内存段失败: " + std::string(ex.what()));
			return;
		}
	}

	// 清空数据对象列表
	datas_.clear();

	// 获取控制数据中的计算数据对象名称列表
	LocalControlData localCtrl;
	getControlData(localCtrl);

	// 遍历所有计算数据对象名称，尝试加载
	for (const auto& name : localCtrl.dataNames) {
		std::string objName = name + SharedMemorySuffix::DATA;
		std::pair<SharedData*, bip::managed_shared_memory::size_type> res =
			dataSegment_->find<SharedData>(objName.c_str());

		if (res.first) {
			datas_.push_back(res.first);
			log(LogLevel::Info, "加载计算数据对象成功: " + name);
		}
		else {
			log(LogLevel::Warning, "找不到计算数据对象: " + name);
		}
	}
}

// 初始化控制数据
void SharedMemoryManager::initControlData(const std::string& name, const std::string& jsonConfig,
	double dt, double t, bool isConverged) {

	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return;
	}

	try {
		// 使用本地控制数据作为中间体
		LocalControlData localData(name, jsonConfig);
		localData.dt = dt;
		localData.t = t;
		localData.isConverged = isConverged;

		// 更新到共享内存
		updateControlData(localData);
		log(LogLevel::Info, "初始化控制数据成功: " + name);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "初始化控制数据失败: " + std::string(e.what()));
		throw;
	}
}

// 获取控制数据的指针
SharedControlData* SharedMemoryManager::getControlData() {
	return controlData_;
}

// 更新控制数据 - 使用完整的LocalControlData
void SharedMemoryManager::updateControlData(const LocalControlData& localData) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return;
	}

	try {
		// 获取控制数据段的分配器
		SharedMemoryAllocator<char> allocator = getAllocator<char>();

		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

		// 使用共享对象的copyFromLocal方法
		controlData_->copyFromLocal(localData, allocator);

		log(LogLevel::Debug, "更新控制数据成功: " + localData.name);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "更新控制数据失败: " + std::string(e.what()));
		throw;
	}
}

// 获取控制数据 - 使用完整的LocalControlData
void SharedMemoryManager::getControlData(LocalControlData& localData) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return;
	}

	try {
		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

		// 使用共享对象的copyToLocal方法
		controlData_->copyToLocal(localData);

		log(LogLevel::Debug, "获取控制数据成功");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "获取控制数据失败: " + std::string(e.what()));
		throw;
	}
}

// 生成唯一的对象名称
std::string SharedMemoryManager::GenerateUniqueObjectName(const std::string& baseName) {
	return memoryName_ + "_" + baseName;
}

// 根据名称查找几何对象
SharedGeometry* SharedMemoryManager::findGeometryByName(const std::string& name) {
	for (auto geo : geos_) {
		// 检查每个几何对象的名称向量
		for (const auto& geoName : geo->shapeNames) {
			if (std::string(geoName.c_str()) == name) {
				return geo;
			}
		}
	}
	return nullptr;
}

// 根据名称查找网格对象
SharedMesh* SharedMemoryManager::findMeshByName(const std::string& name) {
	for (auto mesh : meshs_) {
		if (std::string(mesh->name.c_str()) == name) {
			return mesh;
		}
	}
	return nullptr;
}

// 根据名称查找计算数据对象
SharedData* SharedMemoryManager::findDataByName(const std::string& name) {
	for (auto data : datas_) {
		if (std::string(data->name.c_str()) == name) {
			return data;
		}
	}
	return nullptr;
}

// 获取几何数据的指针列表
const std::vector<SharedGeometry*>& SharedMemoryManager::getGeometry() const {
	return geos_;
}

// 获取网格数据的指针列表
const std::vector<SharedMesh*>& SharedMemoryManager::getMesh() const {
	return meshs_;
}

// 获取计算数据结构的指针列表
const std::vector<SharedData*>& SharedMemoryManager::getData() const {
	return datas_;
}

// 获取几何对象 - 使用完整的LocalGeometry
void SharedMemoryManager::getGeometry(SharedGeometry* geo, LocalGeometry& localGeo) {
	if (!geo) {
		log(LogLevel::Error, "几何对象未初始化");
		return;
	}

	try {
		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(geo->mutex);

		// 使用共享对象的copyToLocal方法
		geo->copyToLocal(localGeo);

		// 更新日志消息，显示获取了多少个几何体
		if (localGeo.shapeNames.empty()) {
			log(LogLevel::Debug, "获取几何对象成功: " + localGeo.name);
		} else {
			log(LogLevel::Debug, "获取几何对象成功: " + std::to_string(localGeo.shapeNames.size()) +
				" 个几何体, 主要名称: " + localGeo.name);
		}
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "获取几何对象失败: " + std::string(e.what()));
		throw;
	}
}

// 更新网格对象 - 使用完整的LocalMesh
void SharedMemoryManager::updateMesh(SharedMesh* mesh, const LocalMesh& localMesh) {
	if (!mesh || !meshSegment_) {
		log(LogLevel::Error, "网格对象或内存段未初始化");
		return;
	}

	try {
		// 检查内存空间是否足够
		if (!checkAndUpdateMeshMemorySize(localMesh.name, localMesh)) {
			log(LogLevel::Warning, "内存空间不足，无法更新网格对象: " + localMesh.name);
			return;
		}

		// 获取网格数据段的分配器
		SharedMemoryAllocator<char> allocator = getAllocator<char>("mesh");

		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(mesh->mutex);

		// 使用共享对象的copyFromLocal方法
		mesh->copyFromLocal(localMesh, allocator);

		// 更新内存使用信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Debug, "更新网格对象成功: " + localMesh.name);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "更新网格对象失败: " + std::string(e.what()));
		throw;
	}
}

// 获取网格对象 - 使用完整的LocalMesh
void SharedMemoryManager::getMesh(SharedMesh* mesh, LocalMesh& localMesh) {
	if (!mesh) {
		log(LogLevel::Error, "网格对象未初始化");
		return;
	}

	try {
		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(mesh->mutex);

		// 使用共享对象的copyToLocal方法
		mesh->copyToLocal(localMesh);

		log(LogLevel::Debug, "获取网格对象成功: " + localMesh.name);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "获取网格对象失败: " + std::string(e.what()));
		throw;
	}
}

// 更新计算数据对象 - 使用完整的LocalData
void SharedMemoryManager::updateData(SharedData* data, const LocalData& localData) {
	if (!data || !dataSegment_) {
		log(LogLevel::Error, "计算数据对象或内存段未初始化");
		return;
	}

	try {
		// 检查内存空间是否足够
		if (!checkAndUpdateDataMemorySize(localData.name, localData)) {
			log(LogLevel::Warning, "内存空间不足，无法更新计算数据对象: " + localData.name);
			return;
		}

		// 获取计算数据段的分配器
		SharedMemoryAllocator<char> allocator = getAllocator<char>("data");

		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(data->mutex);

		// 使用共享对象的copyFromLocal方法
		data->copyFromLocal(localData, allocator);

		// 更新内存使用信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Debug, "更新计算数据对象成功: " + localData.name);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "更新计算数据对象失败: " + std::string(e.what()));
		throw;
	}
}

// 获取计算数据对象 - 使用完整的LocalData
void SharedMemoryManager::getData(SharedData* data, LocalData& localData) {
	if (!data) {
		log(LogLevel::Error, "计算数据对象未初始化");
		return;
	}

	try {
		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(data->mutex);

		// 使用共享对象的copyToLocal方法
		data->copyToLocal(localData);

		log(LogLevel::Debug, "获取计算数据对象成功: " + localData.name);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "获取计算数据对象失败: " + std::string(e.what()));
		throw;
	}
}

// 设置异常信息
void SharedMemoryManager::setException(int type, int code, const std::string& message) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return;
	}

	try {
		// 获取控制数据段的分配器
		SharedMemoryAllocator<char> allocator = getAllocator<char>();

		// 设置异常信息
		controlData_->exception.hasException.store(true);
		controlData_->exception.exceptionType.store(type);
		controlData_->exception.exceptionCode.store(code);
		controlData_->exception.exceptionMessage = SharedMemoryString(message.c_str(), allocator);

		log(LogLevel::Warning, "设置异常信息: 类型=" + std::to_string(type) +
							 ", 代码=" + std::to_string(code) +
							 ", 消息=" + message);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "设置异常信息失败: " + std::string(e.what()));
	}
}

// 检查是否有异常
bool SharedMemoryManager::hasException() const {
	if (!controlData_) {
		return false;
	}
	return controlData_->exception.hasException.load();
}

// 获取异常详情并清除异常标志
std::tuple<int, int, std::string> SharedMemoryManager::getAndClearException() {
	if (!controlData_ || !controlData_->exception.hasException.load()) {
		return std::make_tuple(0, 0, "");
	}

	// 获取异常信息
	int type = controlData_->exception.exceptionType.load();
	int code = controlData_->exception.exceptionCode.load();
	std::string message = controlData_->exception.exceptionMessage.c_str();

	// 清除异常标志
	controlData_->exception.hasException.store(false);
	controlData_->exception.exceptionType.store(0);
	controlData_->exception.exceptionCode.store(0);

	// 获取控制数据段的分配器并清空异常消息
	SharedMemoryAllocator<char> allocator = getAllocator<char>();
	controlData_->exception.exceptionMessage = SharedMemoryString("", allocator);

	log(LogLevel::Info, "获取并清除异常信息: 类型=" + std::to_string(type) +
					   ", 代码=" + std::to_string(code) +
					   ", 消息=" + message);

	return std::make_tuple(type, code, message);
}

// 创建异常对象
std::unique_ptr<yExceptionBase> SharedMemoryManager::createExceptionObject() {
	if (!hasException()) {
		return nullptr;
	}

	auto [type, code, message] = getAndClearException();
	return std::make_unique<SharedMemoryException>(type, code, message);
}

// 更新所有内存段大小信息到控制数据
void SharedMemoryManager::updateMemorySegmentInfo() {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return;
	}

	try {
		// 锁定控制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

		// 更新控制数据段信息
		auto controlUsage = getControlMemoryUsage();
		controlData_->controlSegmentTotalSize = controlUsage.first;
		controlData_->controlSegmentFreeSize = controlUsage.first - controlUsage.second;

		// 更新几何数据段信息
		auto geoUsage = getGeometryMemoryUsage();
		controlData_->geometrySegmentTotalSize = geoUsage.first;
		controlData_->geometrySegmentFreeSize = geoUsage.first - geoUsage.second;

		// 更新网格数据段信息
		auto meshUsage = getMeshMemoryUsage();
		controlData_->meshSegmentTotalSize = meshUsage.first;
		controlData_->meshSegmentFreeSize = meshUsage.first - meshUsage.second;

		// 更新计算数据段信息
		auto dataUsage = getDataMemoryUsage();
		controlData_->dataSegmentTotalSize = dataUsage.first;
		controlData_->dataSegmentFreeSize = dataUsage.first - dataUsage.second;

		// 更新模型参数数据段信息
		auto defUsage = getDefinitionMemoryUsage();
		controlData_->definitionSegmentTotalSize = defUsage.first;
		controlData_->definitionSegmentFreeSize = defUsage.first - defUsage.second;

		log(LogLevel::Debug, "更新共享内存段大小信息成功");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "更新共享内存段大小信息失败: " + std::string(e.what()));
	}
}

// 检查几何对象所需内存空间是否足够，不足则设置异常
bool SharedMemoryManager::checkAndUpdateGeometryMemorySize(const std::string& name, const LocalGeometry& localGeo) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return false;
	}

	if (!geometrySegment_) {
		// 如果几何内存段不存在，需要创建
		log(LogLevel::Warning, "几何数据内存段不存在，需等待创建");

		// 先计算需要的内存大小
		size_t requiredSize = estimateGeometryMemorySize(localGeo);

		// 更新到控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 查找是否已存在同名对象的内存大小记录
		bool found = false;
		for (size_t i = 0; i < localCtrl.modelNames.size(); ++i) {
			if (localCtrl.modelNames[i] == name) {
				if (localCtrl.modelMemorySizes[i] < requiredSize) {
					// 更新需要的内存大小
					localCtrl.modelMemorySizes[i] = requiredSize;

					// 设置异常，提示需要更大的内存空间
					std::stringstream msg;
					msg << "几何对象 " << name << " 需要更大的内存空间: " << requiredSize << " 字节";
					setException(EMP::EXCEPT_MESH, 1, msg.str());

					// 更新控制数据
					updateControlData(localCtrl);
				}
				found = true;
				break;
			}
		}

		if (!found) {
			// 添加新的记录
			localCtrl.modelNames.push_back(name);
			localCtrl.modelMemorySizes.push_back(requiredSize);

			// 设置异常，提示需要创建内存空间
			std::stringstream msg;
			msg << "需要为几何对象 " << name << " 创建内存空间: " << requiredSize << " 字节";
			setException(EMP::EXCEPT_MESH, 2, msg.str());

			// 更新控制数据
			updateControlData(localCtrl);
		}

		return false;
	}

	// 更新内存段信息
	updateMemorySegmentInfo();

	// 检查剩余内存是否足够
	size_t requiredSize = estimateGeometryMemorySize(localGeo);
	auto usage = getGeometryMemoryUsage();

	if (usage.first - usage.second < requiredSize) {
		// 空间不足，设置异常
		std::stringstream msg;
		msg << "几何内存段空间不足，需要 " << requiredSize << " 字节，"
			<< "可用 " << (usage.first - usage.second) << " 字节，"
			<< "总大小 " << usage.first << " 字节";
		setException(EMP::EXCEPT_MESH, 3, msg.str());

		// 更新需要的内存大小到控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 查找是否已存在同名对象的内存大小记录
		bool found = false;
		for (size_t i = 0; i < localCtrl.modelNames.size(); ++i) {
			if (localCtrl.modelNames[i] == name) {
				if (localCtrl.modelMemorySizes[i] < requiredSize) {
					// 更新需要的内存大小
					localCtrl.modelMemorySizes[i] = requiredSize;
				}
				found = true;
				break;
			}
		}

		if (!found) {
			// 添加新的记录
			localCtrl.modelNames.push_back(name);
			localCtrl.modelMemorySizes.push_back(requiredSize);
		}

		// 更新控制数据
		updateControlData(localCtrl);

		log(LogLevel::Warning, "几何内存段空间不足");
		return false;
	}

	return true;
}

// 检查网格对象所需内存空间是否足够，不足则设置异常
bool SharedMemoryManager::checkAndUpdateMeshMemorySize(const std::string& name, const LocalMesh& localMesh) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return false;
	}

	if (!meshSegment_) {
		// 如果网格内存段不存在，需要创建
		log(LogLevel::Warning, "网格数据内存段不存在，需等待创建");

		// 先计算需要的内存大小
		size_t requiredSize = estimateMeshMemorySize(localMesh);

		// 更新到控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 查找是否已存在同名对象的内存大小记录
		bool found = false;
		for (size_t i = 0; i < localCtrl.meshNames.size(); ++i) {
			if (localCtrl.meshNames[i] == name) {
				if (localCtrl.meshMemorySizes[i] < requiredSize) {
					// 更新需要的内存大小
					localCtrl.meshMemorySizes[i] = requiredSize;

					// 设置异常，提示需要更大的内存空间
					std::stringstream msg;
					msg << "网格对象 " << name << " 需要更大的内存空间: " << requiredSize << " 字节";
					setException(EMP::EXCEPT_MESH, 1, msg.str());

					// 更新控制数据
					updateControlData(localCtrl);
				}
				found = true;
				break;
			}
		}

		if (!found) {
			// 添加新的记录
			localCtrl.meshNames.push_back(name);
			localCtrl.meshMemorySizes.push_back(requiredSize);

			// 设置异常，提示需要创建内存空间
			std::stringstream msg;
			msg << "需要为网格对象 " << name << " 创建内存空间: " << requiredSize << " 字节";
			setException(EMP::EXCEPT_MESH, 2, msg.str());

			// 更新控制数据
			updateControlData(localCtrl);
		}

		return false;
	}

	// 更新内存段信息
	updateMemorySegmentInfo();

	// 检查剩余内存是否足够
	size_t requiredSize = estimateMeshMemorySize(localMesh);
	auto usage = getMeshMemoryUsage();

	if (usage.first - usage.second < requiredSize) {
		// 空间不足，设置异常
		std::stringstream msg;
		msg << "网格内存段空间不足，需要 " << requiredSize << " 字节，"
			<< "可用 " << (usage.first - usage.second) << " 字节，"
			<< "总大小 " << usage.first << " 字节";
		setException(EMP::EXCEPT_MESH, 3, msg.str());

		// 更新需要的内存大小到控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 查找是否已存在同名对象的内存大小记录
		bool found = false;
		for (size_t i = 0; i < localCtrl.meshNames.size(); ++i) {
			if (localCtrl.meshNames[i] == name) {
				if (localCtrl.meshMemorySizes[i] < requiredSize) {
					// 更新需要的内存大小
					localCtrl.meshMemorySizes[i] = requiredSize;
				}
				found = true;
				break;
			}
		}

		if (!found) {
			// 添加新的记录
			localCtrl.meshNames.push_back(name);
			localCtrl.meshMemorySizes.push_back(requiredSize);
		}

		// 更新控制数据
		updateControlData(localCtrl);

		log(LogLevel::Warning, "网格内存段空间不足");
		return false;
	}

	return true;
}

// 检查计算数据对象所需内存空间是否足够，不足则设置异常
bool SharedMemoryManager::checkAndUpdateDataMemorySize(const std::string& name, const LocalData& localData) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return false;
	}

	if (!dataSegment_) {
		// 如果计算数据内存段不存在，需要创建
		log(LogLevel::Warning, "计算数据内存段不存在，需等待创建");

		// 先计算需要的内存大小
		size_t requiredSize = estimateDataMemorySize(localData);

		// 更新到控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 查找是否已存在同名对象的内存大小记录
		bool found = false;
		for (size_t i = 0; i < localCtrl.dataNames.size(); ++i) {
			if (localCtrl.dataNames[i] == name) {
				if (localCtrl.dataMemorySizes[i] < requiredSize) {
					// 更新需要的内存大小
					localCtrl.dataMemorySizes[i] = requiredSize;

					// 设置异常，提示需要更大的内存空间
					std::stringstream msg;
					msg << "计算数据对象 " << name << " 需要更大的内存空间: " << requiredSize << " 字节";
					setException(EMP::EXCEPT_DATAPROCESS, 1, msg.str());

					// 更新控制数据
					updateControlData(localCtrl);
				}
				found = true;
				break;
			}
		}

		if (!found) {
			// 添加新的记录
			localCtrl.dataNames.push_back(name);
			localCtrl.dataMemorySizes.push_back(requiredSize);

			// 设置异常，提示需要创建内存空间
			std::stringstream msg;
			msg << "需要为计算数据对象 " << name << " 创建内存空间: " << requiredSize << " 字节";
			setException(EMP::EXCEPT_DATAPROCESS, 2, msg.str());

			// 更新控制数据
			updateControlData(localCtrl);
		}

		return false;
	}

	// 更新内存段信息
	updateMemorySegmentInfo();

	// 检查剩余内存是否足够
	size_t requiredSize = estimateDataMemorySize(localData);
	auto usage = getDataMemoryUsage();

	if (usage.first - usage.second < requiredSize) {
		// 空间不足，设置异常
		std::stringstream msg;
		msg << "计算数据内存段空间不足，需要 " << requiredSize << " 字节，"
			<< "可用 " << (usage.first - usage.second) << " 字节，"
			<< "总大小 " << usage.first << " 字节";
		setException(EMP::EXCEPT_DATAPROCESS, 3, msg.str());

		// 更新需要的内存大小到控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 查找是否已存在同名对象的内存大小记录
		bool found = false;
		for (size_t i = 0; i < localCtrl.dataNames.size(); ++i) {
			if (localCtrl.dataNames[i] == name) {
				if (localCtrl.dataMemorySizes[i] < requiredSize) {
					// 更新需要的内存大小
					localCtrl.dataMemorySizes[i] = requiredSize;
				}
				found = true;
				break;
			}
		}

		if (!found) {
			// 添加新的记录
			localCtrl.dataNames.push_back(name);
			localCtrl.dataMemorySizes.push_back(requiredSize);
		}

		// 更新控制数据
		updateControlData(localCtrl);

		log(LogLevel::Warning, "计算数据内存段空间不足");
		return false;
	}

	return true;
}

// 重新创建几何内存段，增加大小
bool SharedMemoryManager::recreateGeometrySegment(size_t newSize) {
	if (!isCreator_) {
		log(LogLevel::Error, "非Creator无法重新创建共享内存段");
		return false;
	}

	if (newSize <= 0) {
		log(LogLevel::Error, "新的内存段大小必须大于0");
		return false;
	}

	try {
		// 先获取控制数据中的信息
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 确保新的内存大小足够满足需求
		size_t requiredSize = 0;
		for (auto size : localCtrl.modelMemorySizes) {
			requiredSize += size;
		}

		// 增加10%作为缓冲
		requiredSize = static_cast<size_t>(requiredSize * 1.1);

		// 使用指定的大小或计算所需大小的较大者
		size_t finalSize = (newSize > requiredSize) ? newSize : requiredSize;

		// 如果总大小太小，使用默认大小
		if (finalSize < 1024 * 1024) {
			finalSize = 1024 * 1024;
		}

		log(LogLevel::Info, "重新创建几何内存段, 新大小: " + std::to_string(finalSize) + " 字节");

		// 保存当前所有几何对象的本地副本
		std::vector<LocalGeometry> localGeos;
		for (auto geo : geos_) {
			LocalGeometry localGeo;
			getGeometry(geo, localGeo);
			localGeos.push_back(localGeo);
		}

		// 移除旧的几何内存段
		std::string geoSegmentName = GenerateSegmentName(SharedMemorySuffix::GEOMETRY_SEGMENT);
		geos_.clear();
		geometrySegment_.reset();
		bip::shared_memory_object::remove(geoSegmentName.c_str());

		// 创建新的几何内存段
		geometrySegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, geoSegmentName.c_str(), finalSize);

		// 重新创建所有几何对象
		for (const auto& name : localCtrl.modelNames) {
			std::string objName = name + SharedMemorySuffix::GEOMETRY;
			SharedGeometry* geo = geometrySegment_->construct<SharedGeometry>(objName.c_str())
				(geometrySegment_->get_segment_manager());
			geos_.push_back(geo);
		}

		// 恢复几何对象的内容
		for (size_t i = 0; i < localGeos.size() && i < geos_.size(); ++i) {
			updateGeometry(geos_[i], localGeos[i]);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "重新创建几何内存段成功，当前大小: " + std::to_string(finalSize) + " 字节");
		return true;
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "重新创建几何内存段失败: " + std::string(e.what()));
		return false;
	}
}

// 重新创建网格内存段，增加大小
bool SharedMemoryManager::recreateMeshSegment(size_t newSize) {
	if (!isCreator_) {
		log(LogLevel::Error, "非Creator无法重新创建共享内存段");
		return false;
	}

	if (newSize <= 0) {
		log(LogLevel::Error, "新的内存段大小必须大于0");
		return false;
	}

	try {
		// 先获取控制数据中的信息
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 确保新的内存大小足够满足需求
		size_t requiredSize = 0;
		for (auto size : localCtrl.meshMemorySizes) {
			requiredSize += size;
		}

		// 增加10%作为缓冲
		requiredSize = static_cast<size_t>(requiredSize * 1.1);

		// 使用指定的大小或计算所需大小的较大者
		size_t finalSize = (newSize > requiredSize) ? newSize : requiredSize;

		// 如果总大小太小，使用默认大小
		if (finalSize < 1024 * 1024) {
			finalSize = 1024 * 1024;
		}

		log(LogLevel::Info, "重新创建网格内存段, 新大小: " + std::to_string(finalSize) + " 字节");

		// 保存当前所有网格对象的本地副本
		std::vector<LocalMesh> localMeshs;
		for (auto mesh : meshs_) {
			LocalMesh localMesh;
			getMesh(mesh, localMesh);
			localMeshs.push_back(localMesh);
		}

		// 移除旧的网格内存段
		std::string meshSegmentName = GenerateSegmentName(SharedMemorySuffix::MESH_SEGMENT);
		meshs_.clear();
		meshSegment_.reset();
		bip::shared_memory_object::remove(meshSegmentName.c_str());

		// 创建新的网格内存段
		meshSegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, meshSegmentName.c_str(), finalSize);

		// 重新创建所有网格对象
		for (const auto& name : localCtrl.meshNames) {
			std::string objName = name + SharedMemorySuffix::MESH;
			SharedMesh* mesh = meshSegment_->construct<SharedMesh>(objName.c_str())
				(meshSegment_->get_segment_manager());
			meshs_.push_back(mesh);
		}

		// 恢复网格对象的内容
		for (size_t i = 0; i < localMeshs.size() && i < meshs_.size(); ++i) {
			updateMesh(meshs_[i], localMeshs[i]);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "重新创建网格内存段成功，当前大小: " + std::to_string(finalSize) + " 字节");
		return true;
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "重新创建网格内存段失败: " + std::string(e.what()));
		return false;
	}
}

// 重新创建计算数据内存段，增加大小
bool SharedMemoryManager::recreateDataSegment(size_t newSize) {
	if (!isCreator_) {
		log(LogLevel::Error, "非Creator无法重新创建共享内存段");
		return false;
	}

	if (newSize <= 0) {
		log(LogLevel::Error, "新的内存段大小必须大于0");
		return false;
	}

	try {
		// 先获取控制数据中的信息
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 确保新的内存大小足够满足需求
		size_t requiredSize = 0;
		for (auto size : localCtrl.dataMemorySizes) {
			requiredSize += size;
		}

		// 增加10%作为缓冲
		requiredSize = static_cast<size_t>(requiredSize * 1.1);

		// 使用指定的大小或计算所需大小的较大者
		size_t finalSize = (newSize > requiredSize) ? newSize : requiredSize;

		// 如果总大小太小，使用默认大小
		if (finalSize < 1024 * 1024) {
			finalSize = 1024 * 1024;
		}

		log(LogLevel::Info, "重新创建计算数据内存段, 新大小: " + std::to_string(finalSize) + " 字节");

		// 保存当前所有计算数据对象的本地副本
		std::vector<LocalData> localDatas;
		for (auto data : datas_) {
			LocalData localData;
			getData(data, localData);
			localDatas.push_back(localData);
		}

		// 移除旧的计算数据内存段
		std::string dataSegmentName = GenerateSegmentName(SharedMemorySuffix::DATA_SEGMENT);
		datas_.clear();
		dataSegment_.reset();
		bip::shared_memory_object::remove(dataSegmentName.c_str());

		// 创建新的计算数据内存段
		dataSegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, dataSegmentName.c_str(), finalSize);

		// 重新创建所有计算数据对象
		for (const auto& name : localCtrl.dataNames) {
			std::string objName = name + SharedMemorySuffix::DATA;
			SharedData* data = dataSegment_->construct<SharedData>(objName.c_str())
				(dataSegment_->get_segment_manager());
			datas_.push_back(data);
		}

		// 恢复计算数据对象的内容
		for (size_t i = 0; i < localDatas.size() && i < datas_.size(); ++i) {
			updateData(datas_[i], localDatas[i]);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "重新创建计算数据内存段成功，当前大小: " + std::to_string(finalSize) + " 字节");
		return true;
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "重新创建计算数据内存段失败: " + std::string(e.what()));
		return false;
	}
}

// Creator根据控制数据中的信息，自动调整内存段大小
void SharedMemoryManager::autoAdjustMemorySegments() {
	if (!isCreator_) {
		log(LogLevel::Error, "非Creator无法自动调整内存段大小");
		return;
	}

	try {
		// 获取控制数据
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 检查是否有异常信息
		if (hasException()) {
			// 获取异常信息但不清除（后面会用到）
			int type, code;
			std::string message;
			std::tie(type, code, message) = getAndClearException();

			log(LogLevel::Info, "检测到异常：类型=" + std::to_string(type) +
							   ", 代码=" + std::to_string(code) +
							   ", 消息=" + message);

			// 根据异常类型和代码决定调整哪个内存段
			if (type == EMP::EXCEPT_GEO || type == EMP::EXCEPT_MESH) {
				// 几何或网格异常

				// 检查几何内存段是否需要创建或调整
				if (message.find("几何对象") != std::string::npos || message.find("几何内存段") != std::string::npos) {
					if (!geometrySegment_) {
						log(LogLevel::Info, "正在创建几何内存段...");
						createGeometrySegmentAndObjects();
					} else {
						// 计算所需内存大小
						size_t requiredSize = 0;
						for (auto size : localCtrl.modelMemorySizes) {
							requiredSize += size;
						}
						requiredSize = static_cast<size_t>(requiredSize * 1.5); // 增加50%作为缓冲

						auto usage = getGeometryMemoryUsage();
						if (usage.first < requiredSize) {
							log(LogLevel::Info, "几何内存段需要扩容，当前大小: " + std::to_string(usage.first) +
											  " 字节，需要大小: " + std::to_string(requiredSize) + " 字节");
							recreateGeometrySegment(requiredSize);
						}
					}
				}

				// 检查网格内存段是否需要创建或调整
				if (message.find("网格对象") != std::string::npos || message.find("网格内存段") != std::string::npos) {
					if (!meshSegment_) {
						log(LogLevel::Info, "正在创建网格内存段...");
						createMeshSegmentAndObjects();
					} else {
						// 计算所需内存大小
						size_t requiredSize = 0;
						for (auto size : localCtrl.meshMemorySizes) {
							requiredSize += size;
						}
						requiredSize = static_cast<size_t>(requiredSize * 1.5); // 增加50%作为缓冲

						auto usage = getMeshMemoryUsage();
						if (usage.first < requiredSize) {
							log(LogLevel::Info, "网格内存段需要扩容，当前大小: " + std::to_string(usage.first) +
											  " 字节，需要大小: " + std::to_string(requiredSize) + " 字节");
							recreateMeshSegment(requiredSize);
						}
					}
				}
			} else if (type == EMP::EXCEPT_DATAPROCESS) {
				// 计算数据异常

				// 检查计算数据内存段是否需要创建或调整
				if (message.find("计算数据对象") != std::string::npos || message.find("计算数据内存段") != std::string::npos) {
					if (!dataSegment_) {
						log(LogLevel::Info, "正在创建计算数据内存段...");
						createDataSegmentAndObjects();
					} else {
						// 计算所需内存大小
						size_t requiredSize = 0;
						for (auto size : localCtrl.dataMemorySizes) {
							requiredSize += size;
						}
						requiredSize = static_cast<size_t>(requiredSize * 1.5); // 增加50%作为缓冲

						auto usage = getDataMemoryUsage();
						if (usage.first < requiredSize) {
							log(LogLevel::Info, "计算数据内存段需要扩容，当前大小: " + std::to_string(usage.first) +
											  " 字节，需要大小: " + std::to_string(requiredSize) + " 字节");
							recreateDataSegment(requiredSize);
						}
					}
				}
			} else if (type == EMP::EXCEPT_DEFINITION) {
				// 模型参数异常

				// 检查模型参数内存段是否需要创建或调整
				if (message.find("模型参数对象") != std::string::npos || message.find("模型参数内存段") != std::string::npos) {
					if (!definitionSegment_) {
						log(LogLevel::Info, "正在创建模型参数内存段...");
						createDefinitionSegmentAndObjects();
					} else {
						// 使用默认大小或两倍当前大小
						auto usage = getDefinitionMemoryUsage();
						size_t requiredSize = (usage.first * 2 > static_cast<size_t>(2 * 1024 * 1024)) ?
							usage.first * 2 : static_cast<size_t>(2 * 1024 * 1024);

						log(LogLevel::Info, "模型参数内存段需要扩容，当前大小: " + std::to_string(usage.first) +
										  " 字节，需要大小: " + std::to_string(requiredSize) + " 字节");
						recreateDefinitionSegment(requiredSize);
					}
				}
			}
		} else {
			// 没有异常，检查内存段的使用情况，如果接近满载则增加大小
			if (geometrySegment_) {
				auto usage = getGeometryMemoryUsage();
				// 如果可用空间低于20%，则扩容到当前大小的2倍
				if (usage.first > 0 && (usage.first - usage.second) < usage.first * 0.2) {
					log(LogLevel::Info, "几何内存段空间不足，当前使用率: " +
									  std::to_string(static_cast<double>(usage.second) / usage.first * 100) + "%, 进行扩容");
					recreateGeometrySegment(usage.first * 2);
				}
			}

			if (meshSegment_) {
				auto usage = getMeshMemoryUsage();
				// 如果可用空间低于20%，则扩容到当前大小的2倍
				if (usage.first > 0 && (usage.first - usage.second) < usage.first * 0.2) {
					log(LogLevel::Info, "网格内存段空间不足，当前使用率: " +
									  std::to_string(static_cast<double>(usage.second) / usage.first * 100) + "%, 进行扩容");
					recreateMeshSegment(usage.first * 2);
				}
			}

			if (dataSegment_) {
				auto usage = getDataMemoryUsage();
				// 如果可用空间低于20%，则扩容到当前大小的2倍
				if (usage.first > 0 && (usage.first - usage.second) < usage.first * 0.2) {
					log(LogLevel::Info, "计算数据内存段空间不足，当前使用率: " +
									  std::to_string(static_cast<double>(usage.second) / usage.first * 100) + "%, 进行扩容");
					recreateDataSegment(usage.first * 2);
				}
			}

			if (definitionSegment_) {
				auto usage = getDefinitionMemoryUsage();
				// 如果可用空间低于20%，则扩容到当前大小的2倍
				if (usage.first > 0 && (usage.first - usage.second) < usage.first * 0.2) {
					log(LogLevel::Info, "模型参数内存段空间不足，当前使用率: " +
									  std::to_string(static_cast<double>(usage.second) / usage.first * 100) + "%, 进行扩容");
					recreateDefinitionSegment(usage.first * 2);
				}
			}
		}

		// 更新内存段信息
		updateMemorySegmentInfo();
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "自动调整内存段大小失败: " + std::string(e.what()));
	}
}

// 更新几何对象 - 使用完整的LocalGeometry
void SharedMemoryManager::updateGeometry(SharedGeometry* geo, const LocalGeometry& localGeo) {
	if (!geo || !geometrySegment_) {
		log(LogLevel::Error, "几何对象或内存段未初始化");
		return;
	}

	try {
		// 检查内存空间是否足够
		std::string primaryName = localGeo.getPrimaryName();
		if (!checkAndUpdateGeometryMemorySize(primaryName, localGeo)) {
			log(LogLevel::Warning, "内存空间不足，无法更新几何对象: " + primaryName);
			return;
		}

		// 获取几何数据段的分配器
		SharedMemoryAllocator<char> allocator = getAllocator<char>("geometry");

		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(geo->mutex);

		// 使用共享对象的copyFromLocal方法
		geo->copyFromLocal(localGeo, allocator);

		// 更新内存使用信息到控制数据
		updateMemorySegmentInfo();

		// 更新日志消息，包含几何体数量信息
		log(LogLevel::Debug, "更新几何对象成功: " + std::to_string(localGeo.shapeNames.size()) +
			" 个几何体, 主要名称: " + primaryName);
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "更新几何对象失败: " + std::string(e.what()));
		throw;
	}
}

// 创建模型参数对象共享内存段和对象
void SharedMemoryManager::createDefinitionSegmentAndObjects() {
	if (!isCreator_) {
		log(LogLevel::Warning, "非创建者不能创建模型参数内存段");
		return;
	}

	try {
		// 获取模型参数数据所需内存大小
		size_t totalSize = 0;
		LocalControlData localCtrl;
		getControlData(localCtrl);

		// 暂时使用默认大小，因为模型参数使用较少内存
		totalSize = 1024 * 1024; // 1MB 默认大小

		// 创建模型参数数据内存段
		std::string defSegmentName = GenerateSegmentName(SharedMemorySuffix::DEFINITION_SEGMENT);
		bip::shared_memory_object::remove(defSegmentName.c_str()); // 先移除现有的（如果存在）

		definitionSegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, defSegmentName.c_str(), totalSize);

		// 清空当前模型参数对象列表
		defs_.clear();

		// 暂时只创建一个默认的模型参数对象
		std::string objName = "DefaultDefinition" + std::string(SharedMemorySuffix::DEFINITION); // 后缀为char类型，需要转换为string类型
		SharedDefinitionList* def = definitionSegment_->construct<SharedDefinitionList>(objName.c_str())
			(definitionSegment_->get_segment_manager());
		defs_.push_back(def);

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "创建模型参数数据内存段: " + defSegmentName + ", 大小: " + std::to_string(totalSize) + " 字节");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "创建模型参数数据内存段失败: " + std::string(e.what()));
		throw;
	}
}

// 加载已存在的模型参数对象
void SharedMemoryManager::LoadExistingDefinitionObjects() {
	if (!definitionSegment_) {
		try {
			// 尝试连接到模型参数数据内存段
			std::string defSegmentName = GenerateSegmentName(SharedMemorySuffix::DEFINITION_SEGMENT);
			definitionSegment_ = std::make_shared<bip::managed_shared_memory>(
				bip::open_only, defSegmentName.c_str());
			log(LogLevel::Info, "连接到模型参数数据内存段: " + defSegmentName);
		}
		catch (const bip::interprocess_exception& ex) {
			log(LogLevel::Warning, "连接模型参数数据内存段失败: " + std::string(ex.what()));
			return;
		}
	}

	// 清空模型参数对象列表
	defs_.clear();

	// 尝试加载默认的模型参数对象
	std::string objName = "DefaultDefinition" + std::string(SharedMemorySuffix::DEFINITION); // 后缀为char类型，需要转换为string类型
	std::pair<SharedDefinitionList*, bip::managed_shared_memory::size_type> res =
		definitionSegment_->find<SharedDefinitionList>(objName.c_str());

	if (res.first) {
		defs_.push_back(res.first);
		log(LogLevel::Info, "加载模型参数对象成功: DefaultDefinition");
	}
	else {
		log(LogLevel::Warning, "找不到默认模型参数对象");
	}
}

// 估算模型参数对象所需的共享内存大小
size_t SharedMemoryManager::estimateDefinitionMemorySize(const LocalDefinitionList& localDef) {
	// 基础大小
	size_t baseSize = sizeof(SharedDefinitionList);

	// 名称和描述大小
	size_t nameSize = localDef.name.size() + 1;
	size_t descriptionSize = localDef.description.size() + 1;

	// 计算所有参数的总大小
	size_t totalParameterCount = 0;
	size_t totalParameterNameSize = 0;

	for (const auto& def : localDef.definitions) {
		totalParameterCount += def.parameterNames.size();

		// 计算所有参数名称的大小
		for (const auto& paramName : def.parameterNames) {
			totalParameterNameSize += paramName.size() + 1;
		}
	}

	// 计算各个向量的大小
	size_t idsSize = localDef.definitions.size() * sizeof(int);
	size_t indicesSize = localDef.definitions.size() * sizeof(int) * 2; // 开始索引和参数数量
	size_t parameterValuesSize = totalParameterCount * sizeof(double);
	size_t parameterNamesSize = totalParameterNameSize + totalParameterCount * sizeof(SharedMemoryString);

	// 考虑共享内存管理开销
	size_t totalSize = baseSize + nameSize + descriptionSize +
					   idsSize + indicesSize + parameterValuesSize + parameterNamesSize;

	// 添加额外的20%作为安全边际
	return static_cast<size_t>(totalSize * 1.2);
}

// 返回模型参数共享内存段的总大小和已使用大小
std::pair<size_t, size_t> SharedMemoryManager::getDefinitionMemoryUsage() const {
	if (definitionSegment_) {
		return std::make_pair(definitionSegment_->get_size(), definitionSegment_->get_size() - definitionSegment_->get_free_memory());
	}
	return std::make_pair(0, 0);
}

// 根据名称查找模型参数对象
SharedDefinitionList* SharedMemoryManager::findDefinitionByName(const std::string& name) {
	for (auto def : defs_) {
		if (std::string(def->name.c_str()) == name) {
			return def;
		}
	}
	return nullptr;
}

// 获取模型参数结构的指针列表
const std::vector<SharedDefinitionList*>& SharedMemoryManager::getDefinition() const {
	return defs_;
}

// 更新模型参数对象 - 使用完整的LocalDefinitionList
void SharedMemoryManager::updateDefinition(SharedDefinitionList* def, const LocalDefinitionList& localDef) {
	if (!def || !definitionSegment_) {
		log(LogLevel::Error, "模型参数对象或内存段未初始化");
		return;
	}

	try {
		// 检查内存空间是否足够
		if (!checkAndUpdateDefinitionMemorySize(localDef.name, localDef)) {
			log(LogLevel::Warning, "内存空间不足，无法更新模型参数对象: " + localDef.name);
			return;
		}

		// 获取模型参数数据段的分配器
		SharedMemoryAllocator<char> allocator = getAllocator<char>("definition");

		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(def->mutex);

		// 使用共享对象的copyFromLocal方法
		def->copyFromLocal(localDef, allocator);

		// 更新内存使用信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Debug, "更新模型参数对象成功: " + localDef.name +
			", 包含 " + std::to_string(localDef.definitions.size()) + " 组参数");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "更新模型参数对象失败: " + std::string(e.what()));
		throw;
	}
}

// 获取模型参数对象 - 使用完整的LocalDefinitionList
void SharedMemoryManager::getDefinition(SharedDefinitionList* def, LocalDefinitionList& localDef) {
	if (!def) {
		log(LogLevel::Error, "模型参数对象未初始化");
		return;
	}

	try {
		// 加锁保护并复制数据
		bip::scoped_lock<bip::interprocess_mutex> lock(def->mutex);

		// 使用共享对象的copyToLocal方法
		def->copyToLocal(localDef);

		log(LogLevel::Debug, "获取模型参数对象成功: " + localDef.name +
			", 包含 " + std::to_string(localDef.definitions.size()) + " 组参数");
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "获取模型参数对象失败: " + std::string(e.what()));
		throw;
	}
}

// 检查模型参数对象所需内存空间是否足够，不足则设置异常
bool SharedMemoryManager::checkAndUpdateDefinitionMemorySize(const std::string& name, const LocalDefinitionList& localDef) {
	if (!controlData_) {
		log(LogLevel::Error, "控制数据对象未初始化");
		return false;
	}

	if (!definitionSegment_) {
		// 如果模型参数内存段不存在，需要创建
		log(LogLevel::Warning, "模型参数内存段不存在，需等待创建");

		// 设置异常，提示需要创建内存空间
		std::stringstream msg;
		msg << "需要为模型参数对象 " << name << " 创建内存空间";
		setException(EMP::EXCEPT_DEFINITION, 1, msg.str());

		return false;
	}

	// 更新内存段信息
	updateMemorySegmentInfo();

	// 检查剩余内存是否足够
	size_t requiredSize = estimateDefinitionMemorySize(localDef);
	auto usage = getDefinitionMemoryUsage();

	if (usage.first - usage.second < requiredSize) {
		// 空间不足，设置异常
		std::stringstream msg;
		msg << "模型参数内存段空间不足，需要 " << requiredSize << " 字节，"
			<< "可用 " << (usage.first - usage.second) << " 字节，"
			<< "总大小 " << usage.first << " 字节";
		setException(EMP::EXCEPT_DEFINITION, 2, msg.str());

		log(LogLevel::Warning, "模型参数内存段空间不足");
		return false;
	}

	return true;
}

// 重新创建模型参数内存段，增加大小
bool SharedMemoryManager::recreateDefinitionSegment(size_t newSize) {
	if (!isCreator_) {
		log(LogLevel::Error, "非Creator无法重新创建共享内存段");
		return false;
	}

	if (newSize <= 0) {
		log(LogLevel::Error, "新的内存段大小必须大于0");
		return false;
	}

	try {
		// 如果总大小太小，使用默认大小
		if (newSize < 1024 * 1024) {
			newSize = 1024 * 1024;
		}

		log(LogLevel::Info, "重新创建模型参数内存段, 新大小: " + std::to_string(newSize) + " 字节");

		// 保存当前所有模型参数对象的本地副本
		std::vector<LocalDefinitionList> localDefs;
		for (auto def : defs_) {
			LocalDefinitionList localDef;
			getDefinition(def, localDef);
			localDefs.push_back(localDef);
		}

		// 移除旧的模型参数内存段
		std::string defSegmentName = GenerateSegmentName(SharedMemorySuffix::DEFINITION_SEGMENT);
		defs_.clear();
		definitionSegment_.reset();
		bip::shared_memory_object::remove(defSegmentName.c_str());

		// 创建新的模型参数内存段
		definitionSegment_ = std::make_shared<bip::managed_shared_memory>(
			bip::create_only, defSegmentName.c_str(), newSize);

		// 重新创建所有模型参数对象
		for (const auto& localDef : localDefs) {
			std::string objName = localDef.name + SharedMemorySuffix::DEFINITION; // 后缀为char类型，需要转换为string类型
			SharedDefinitionList* def = definitionSegment_->construct<SharedDefinitionList>(objName.c_str())
				(definitionSegment_->get_segment_manager());
			defs_.push_back(def);
		}

		// 如果没有现有对象，创建一个默认对象
		if (defs_.empty()) {
			std::string objName = "DefaultDefinition" + std::string(SharedMemorySuffix::DEFINITION); // 后缀为char类型，需要转换为string类型
			SharedDefinitionList* def = definitionSegment_->construct<SharedDefinitionList>(objName.c_str())
				(definitionSegment_->get_segment_manager());
			defs_.push_back(def);
		}

		// 恢复模型参数对象的内容
		for (size_t i = 0; i < localDefs.size() && i < defs_.size(); ++i) {
			updateDefinition(defs_[i], localDefs[i]);
		}

		// 更新内存段信息到控制数据
		updateMemorySegmentInfo();

		log(LogLevel::Info, "重新创建模型参数内存段成功，当前大小: " + std::to_string(newSize) + " 字节");
		return true;
	}
	catch (const std::exception& e) {
		log(LogLevel::Error, "重新创建模型参数内存段失败: " + std::string(e.what()));
		return false;
	}
}

// ========== 共享内存与磁盘文件的读写转换接口 ==========

// 将共享内存中的数据保存到磁盘文件
bool SharedMemoryManager::saveToFile(const std::string& filePath, bool binaryFormat) {
    try {
        // 保存所有类型的内存段
        std::string baseFilePath = filePath;
        if (baseFilePath.find_last_of("\\/") == std::string::npos) {
            // 如果没有路径分隔符，添加当前目录
            baseFilePath = "./" + baseFilePath;
        }

        // 移除可能的文件扩展名
        size_t extPos = baseFilePath.find_last_of(".");
        if (extPos != std::string::npos) {
            baseFilePath = baseFilePath.substr(0, extPos);
        }

        // 保存控制数据
        std::string ctrlFilePath = baseFilePath + "_control" + (binaryFormat ? ".bin" : ".txt");
        if (!saveSegmentToFile(ctrlFilePath, "control", binaryFormat)) {
            log(LogLevel::Error, "保存控制数据段失败: " + ctrlFilePath);
            return false;
        }

        // 保存几何数据
        if (geometrySegment_) {
            std::string geoFilePath = baseFilePath + "_geometry" + (binaryFormat ? ".bin" : ".txt");
            if (!saveSegmentToFile(geoFilePath, "geometry", binaryFormat)) {
                log(LogLevel::Error, "保存几何数据段失败: " + geoFilePath);
                return false;
            }
        }

        // 保存网格数据
        if (meshSegment_) {
            std::string meshFilePath = baseFilePath + "_mesh" + (binaryFormat ? ".bin" : ".txt");
            if (!saveSegmentToFile(meshFilePath, "mesh", binaryFormat)) {
                log(LogLevel::Error, "保存网格数据段失败: " + meshFilePath);
                return false;
            }
        }

        // 保存计算数据
        if (dataSegment_) {
            std::string dataFilePath = baseFilePath + "_data" + (binaryFormat ? ".bin" : ".txt");
            if (!saveSegmentToFile(dataFilePath, "data", binaryFormat)) {
                log(LogLevel::Error, "保存计算数据段失败: " + dataFilePath);
                return false;
            }
        }

        // 保存模型参数数据
        if (definitionSegment_) {
            std::string defFilePath = baseFilePath + "_definition" + (binaryFormat ? ".bin" : ".txt");
            if (!saveSegmentToFile(defFilePath, "definition", binaryFormat)) {
                log(LogLevel::Error, "保存模型参数数据段失败: " + defFilePath);
                return false;
            }
        }

        log(LogLevel::Info, "已将所有共享内存数据保存到: " + baseFilePath);
        return true;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "保存共享内存数据时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 将特定类型的共享内存段保存到文件
bool SharedMemoryManager::saveSegmentToFile(const std::string& filePath, const std::string& segmentType, bool binaryFormat) {
    try {
        std::shared_ptr<bip::managed_shared_memory> segment;
        std::vector<std::string> objectNames;

        // 根据段类型选择对应的内存段和对象列表
        if (segmentType == "control") {
            segment = controlSegment_;
            objectNames.push_back("ControlData");
        }
        else if (segmentType == "geometry" && geometrySegment_) {
            segment = geometrySegment_;
            for (auto geo : geos_) {
                // 提取对象名称，移除内存段名称前缀
                std::string fullName = segment->get_instance_name(geo);
                objectNames.push_back(fullName);
            }
        }
        else if (segmentType == "mesh" && meshSegment_) {
            segment = meshSegment_;
            for (auto mesh : meshs_) {
                std::string fullName = segment->get_instance_name(mesh);
                objectNames.push_back(fullName);
            }
        }
        else if (segmentType == "data" && dataSegment_) {
            segment = dataSegment_;
            for (auto data : datas_) {
                std::string fullName = segment->get_instance_name(data);
                objectNames.push_back(fullName);
            }
        }
        else if (segmentType == "definition" && definitionSegment_) {
            segment = definitionSegment_;
            for (auto def : defs_) {
                std::string fullName = segment->get_instance_name(def);
                objectNames.push_back(fullName);
            }
        }
        else {
            log(LogLevel::Error, "未知或未初始化的内存段类型: " + segmentType);
            return false;
        }

        // 如果没有对象，则不需要保存
        if (objectNames.empty()) {
            log(LogLevel::Warning, "内存段 " + segmentType + " 中没有对象，不需要保存");
            return true;
        }

        // 打开文件
        std::ofstream file;
        if (binaryFormat) {
            file.open(filePath, std::ios::binary | std::ios::out);
        }
        else {
            file.open(filePath, std::ios::out);
        }

        if (!file.is_open()) {
            log(LogLevel::Error, "无法打开文件进行写入: " + filePath);
            return false;
        }

        // 写入头部信息
        if (binaryFormat) {
            // 二进制格式: 写入魔术数字和版本
            const char magic[] = "SMMBINARY";
            file.write(magic, sizeof(magic) - 1); // 不包括null终止符

            // 写入版本号、内存段类型和对象数量
            uint32_t version = 1;
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));

            uint32_t segmentTypeLength = static_cast<uint32_t>(segmentType.length());
            file.write(reinterpret_cast<const char*>(&segmentTypeLength), sizeof(segmentTypeLength));
            file.write(segmentType.c_str(), segmentTypeLength);

            uint32_t objectCount = static_cast<uint32_t>(objectNames.size());
            file.write(reinterpret_cast<const char*>(&objectCount), sizeof(objectCount));
        }
        else {
            // ASCII格式: 写入头部信息作为注释
            file << "# SharedMemoryManager Memory Segment Dump" << std::endl;
            file << "# Version: 1" << std::endl;
            file << "# SegmentType: " << segmentType << std::endl;
            file << "# ObjectCount: " << objectNames.size() << std::endl;
            file << "# CreationTime: " << std::time(nullptr) << std::endl;
            file << "# Format: ASCII" << std::endl;
            file << "#" << std::endl;
        }

        // 在文件中保存每个对象
        for (const auto& objectName : objectNames) {
            if (!saveObjectToFile(file, segment, objectName, binaryFormat)) {
                log(LogLevel::Error, "保存对象失败: " + objectName);
                file.close();
                return false;
            }
        }

        file.close();
        log(LogLevel::Info, "已将内存段 " + segmentType + " 保存到文件: " + filePath);
        return true;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "保存内存段时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 将指定的共享对象保存到文件
bool SharedMemoryManager::saveObjectToFile(const std::string& filePath, const std::string& objectName, bool binaryFormat) {
    try {
        // 查找对象所在的内存段
        std::shared_ptr<bip::managed_shared_memory> segment;

        // 尝试在各个内存段中查找对象
        if (controlSegment_) {
            if (objectName == "ControlData" || controlSegment_->find<char>(objectName.c_str()).first) {
                segment = controlSegment_;
            }
        }

        if (!segment && geometrySegment_) {
            if (geometrySegment_->find<char>(objectName.c_str()).first) {
                segment = geometrySegment_;
            }
        }

        if (!segment && meshSegment_) {
            if (meshSegment_->find<char>(objectName.c_str()).first) {
                segment = meshSegment_;
            }
        }

        if (!segment && dataSegment_) {
            if (dataSegment_->find<char>(objectName.c_str()).first) {
                segment = dataSegment_;
            }
        }

        if (!segment && definitionSegment_) {
            if (definitionSegment_->find<char>(objectName.c_str()).first) {
                segment = definitionSegment_;
            }
        }

        if (!segment) {
            log(LogLevel::Error, "找不到对象: " + objectName);
            return false;
        }

        // 打开文件
        std::ofstream file;
        if (binaryFormat) {
            file.open(filePath, std::ios::binary | std::ios::out);
        }
        else {
            file.open(filePath, std::ios::out);
        }

        if (!file.is_open()) {
            log(LogLevel::Error, "无法打开文件进行写入: " + filePath);
            return false;
        }

        // 写入对象
        bool result = saveObjectToFile(file, segment, objectName, binaryFormat);

        file.close();
        return result;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "保存对象到文件时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 内部辅助函数：将对象写入已打开的文件流
bool SharedMemoryManager::saveObjectToFile(std::ofstream& file, std::shared_ptr<bip::managed_shared_memory> segment,
                                           const std::string& objectName, bool binaryFormat) {
    try {
        // 获取对象数据
        void* objectPtr = nullptr;
        size_t objectSize = 0;

        if (objectName == "ControlData" && controlData_) {
            objectPtr = controlData_;
            objectSize = sizeof(SharedControlData);
        }
        else {
            // 查找对象
            std::pair<char*, bip::managed_shared_memory::size_type> result =
                segment->find<char>(objectName.c_str());

            if (!result.first) {
                log(LogLevel::Error, "在内存段中找不到对象: " + objectName);
                return false;
            }

            objectPtr = result.first;
            objectSize = result.second;
        }

        if (binaryFormat) {
            // 二进制格式: 写入对象名称长度、名称和数据大小
            uint32_t nameLength = static_cast<uint32_t>(objectName.length());
            file.write(reinterpret_cast<const char*>(&nameLength), sizeof(nameLength));
            file.write(objectName.c_str(), nameLength);

            uint64_t dataSize = static_cast<uint64_t>(objectSize);
            file.write(reinterpret_cast<const char*>(&dataSize), sizeof(dataSize));

            // 写入对象数据
            file.write(static_cast<const char*>(objectPtr), objectSize);
        }
        else {
            // ASCII格式: 以可读形式写入对象信息
            file << "OBJECT: " << objectName << std::endl;
            file << "SIZE: " << objectSize << " bytes" << std::endl;
            file << "DATA_HEX: ";

            // 将数据转换为十六进制格式
            const unsigned char* bytes = static_cast<const unsigned char*>(objectPtr);
            for (size_t i = 0; i < objectSize; ++i) {
                file << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]) << " ";
                if ((i + 1) % 16 == 0) {
                    file << std::endl << "         ";
                }
            }
            file << std::dec << std::endl << std::endl;
        }

        return true;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "写入对象到文件时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 从磁盘文件加载数据到共享内存(仅creator调用)
bool SharedMemoryManager::loadFromFile(const std::string& filePath, bool binaryFormat) {
    if (!isCreator_) {
        log(LogLevel::Error, "只有Creator可以从文件恢复共享内存");
        return false;
    }

    try {
        // 获取基本文件路径
        std::string baseFilePath = filePath;
        if (baseFilePath.find_last_of("\\/") == std::string::npos) {
            // 如果没有路径分隔符，添加当前目录
            baseFilePath = "./" + baseFilePath;
        }

        // 移除可能的文件扩展名
        size_t extPos = baseFilePath.find_last_of(".");
        if (extPos != std::string::npos) {
            baseFilePath = baseFilePath.substr(0, extPos);
        }

        // 加载控制数据
        std::string ctrlFilePath = baseFilePath + "_control" + (binaryFormat ? ".bin" : ".txt");
        if (!loadSegmentFromFile(ctrlFilePath, "control", binaryFormat)) {
            log(LogLevel::Error, "加载控制数据段失败: " + ctrlFilePath);
            return false;
        }

        // 加载几何数据
        std::string geoFilePath = baseFilePath + "_geometry" + (binaryFormat ? ".bin" : ".txt");
        if (fs::exists(geoFilePath)) {
            if (!loadSegmentFromFile(geoFilePath, "geometry", binaryFormat)) {
                log(LogLevel::Error, "加载几何数据段失败: " + geoFilePath);
                return false;
            }
        }

        // 加载网格数据
        std::string meshFilePath = baseFilePath + "_mesh" + (binaryFormat ? ".bin" : ".txt");
        if (fs::exists(meshFilePath)) {
            if (!loadSegmentFromFile(meshFilePath, "mesh", binaryFormat)) {
                log(LogLevel::Error, "加载网格数据段失败: " + meshFilePath);
                return false;
            }
        }

        // 加载计算数据
        std::string dataFilePath = baseFilePath + "_data" + (binaryFormat ? ".bin" : ".txt");
        if (fs::exists(dataFilePath)) {
            if (!loadSegmentFromFile(dataFilePath, "data", binaryFormat)) {
                log(LogLevel::Error, "加载计算数据段失败: " + dataFilePath);
                return false;
            }
        }

        // 加载模型参数数据
        std::string defFilePath = baseFilePath + "_definition" + (binaryFormat ? ".bin" : ".txt");
        if (fs::exists(defFilePath)) {
            if (!loadSegmentFromFile(defFilePath, "definition", binaryFormat)) {
                log(LogLevel::Error, "加载模型参数数据段失败: " + defFilePath);
                return false;
            }
        }

        log(LogLevel::Info, "已从文件加载所有共享内存数据: " + baseFilePath);
        return true;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "从文件加载共享内存数据时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 从文件加载数据到特定类型的共享内存段(仅creator调用)
bool SharedMemoryManager::loadSegmentFromFile(const std::string& filePath, const std::string& segmentType, bool binaryFormat) {
    if (!isCreator_) {
        log(LogLevel::Error, "只有Creator可以从文件恢复共享内存");
        return false;
    }

    try {
        // 打开文件
        std::ifstream file;
        if (binaryFormat) {
            file.open(filePath, std::ios::binary | std::ios::in);
        }
        else {
            file.open(filePath, std::ios::in);
        }

        if (!file.is_open()) {
            log(LogLevel::Error, "无法打开文件进行读取: " + filePath);
            return false;
        }

        // 验证文件格式
        bool isValidFile = false;
        std::string fileSegmentType;
        uint32_t objectCount = 0;

        if (binaryFormat) {
            // 二进制格式: 检查魔术数字和版本
            char magic[9] = {0};
            file.read(magic, 8);
            if (std::string(magic) != "SMMBINARY") {
                log(LogLevel::Error, "文件格式不正确，不是有效的二进制共享内存转储文件");
                file.close();
                return false;
            }

            // 读取版本号
            uint32_t version;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            if (version != 1) {
                log(LogLevel::Error, "不支持的文件版本: " + std::to_string(version));
                file.close();
                return false;
            }

            // 读取段类型
            uint32_t segmentTypeLength;
            file.read(reinterpret_cast<char*>(&segmentTypeLength), sizeof(segmentTypeLength));
            char* segmentTypeBuffer = new char[segmentTypeLength + 1];
            file.read(segmentTypeBuffer, segmentTypeLength);
            segmentTypeBuffer[segmentTypeLength] = '\0';
            fileSegmentType = segmentTypeBuffer;
            delete[] segmentTypeBuffer;

            // 读取对象数量
            file.read(reinterpret_cast<char*>(&objectCount), sizeof(objectCount));

            isValidFile = true;
        }
        else {
            // ASCII格式: 解析头部注释
            std::string line;
            while (std::getline(file, line) && line.substr(0, 1) == "#") {
                if (line.find("# SegmentType:") != std::string::npos) {
                    fileSegmentType = line.substr(line.find(":") + 1);
                    // 去除前导空格
                    fileSegmentType.erase(0, fileSegmentType.find_first_not_of(" \t"));
                }
                else if (line.find("# ObjectCount:") != std::string::npos) {
                    std::string countStr = line.substr(line.find(":") + 1);
                    // 去除前导空格
                    countStr.erase(0, countStr.find_first_not_of(" \t"));
                    objectCount = std::stoi(countStr);
                }
                else if (line.find("# Format: ASCII") != std::string::npos) {
                    isValidFile = true;
                }
            }

            // 回到文件开始，跳过头部注释
            file.clear();
            file.seekg(0);
            while (std::getline(file, line) && line.substr(0, 1) == "#") {
                // 跳过注释行
            }
            // 将文件指针返回到最后一次读取的行首
            file.seekg(file.tellg() - static_cast<std::streamoff>(line.length() + 1));
        }

        if (!isValidFile || fileSegmentType != segmentType) {
            log(LogLevel::Error, "文件格式不正确或段类型不匹配: " + filePath);
            file.close();
            return false;
        }

        // 确保有对应的内存段
        std::shared_ptr<bip::managed_shared_memory> segment;
        if (segmentType == "control") {
            segment = controlSegment_;
        }
        else if (segmentType == "geometry") {
            if (!geometrySegment_) {
                createGeometrySegmentAndObjects();
            }
            segment = geometrySegment_;
        }
        else if (segmentType == "mesh") {
            if (!meshSegment_) {
                createMeshSegmentAndObjects();
            }
            segment = meshSegment_;
        }
        else if (segmentType == "data") {
            if (!dataSegment_) {
                createDataSegmentAndObjects();
            }
            segment = dataSegment_;
        }
        else if (segmentType == "definition") {
            if (!definitionSegment_) {
                createDefinitionSegmentAndObjects();
            }
            segment = definitionSegment_;
        }

        if (!segment) {
            log(LogLevel::Error, "无法获取内存段: " + segmentType);
            file.close();
            return false;
        }

        // 依次加载对象
        for (uint32_t i = 0; i < objectCount; ++i) {
            if (!loadObjectFromFile(file, segment, binaryFormat)) {
                log(LogLevel::Error, "加载对象失败，索引: " + std::to_string(i));
                file.close();
                return false;
            }
        }

        file.close();
        log(LogLevel::Info, "已从文件加载内存段 " + segmentType + ": " + filePath);
        return true;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "从文件加载内存段时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 从文件加载数据到指定的共享对象(仅creator调用)
bool SharedMemoryManager::loadObjectFromFile(const std::string& filePath, const std::string& objectName, bool binaryFormat) {
    if (!isCreator_) {
        log(LogLevel::Error, "只有Creator可以从文件恢复共享内存对象");
        return false;
    }

    try {
        // 打开文件
        std::ifstream file;
        if (binaryFormat) {
            file.open(filePath, std::ios::binary | std::ios::in);
        }
        else {
            file.open(filePath, std::ios::in);
        }

        if (!file.is_open()) {
            log(LogLevel::Error, "无法打开文件进行读取: " + filePath);
            return false;
        }

        // 查找对象所在的内存段
        std::shared_ptr<bip::managed_shared_memory> segment;

        if (objectName == "ControlData") {
            segment = controlSegment_;
        }
        else {
            // 尝试在各个内存段中查找对象
            if (geometrySegment_) {
                if (geometrySegment_->find<char>(objectName.c_str()).first) {
                    segment = geometrySegment_;
                }
            }

            if (!segment && meshSegment_) {
                if (meshSegment_->find<char>(objectName.c_str()).first) {
                    segment = meshSegment_;
                }
            }

            if (!segment && dataSegment_) {
                if (dataSegment_->find<char>(objectName.c_str()).first) {
                    segment = dataSegment_;
                }
            }

            if (!segment && definitionSegment_) {
                if (definitionSegment_->find<char>(objectName.c_str()).first) {
                    segment = definitionSegment_;
                }
            }
        }

        if (!segment) {
            log(LogLevel::Error, "找不到对象的内存段: " + objectName);
            file.close();
            return false;
        }

        // 读取和验证文件中的所有对象
        if (binaryFormat) {
            // 跳过文件头
            file.seekg(8 + sizeof(uint32_t)); // 魔术数字(8)和版本(4)

            // 跳过段类型
            uint32_t segmentTypeLength;
            file.read(reinterpret_cast<char*>(&segmentTypeLength), sizeof(segmentTypeLength));
            file.seekg(file.tellg() + static_cast<std::streamoff>(segmentTypeLength));

            // 获取对象数量
            uint32_t objectCount;
            file.read(reinterpret_cast<char*>(&objectCount), sizeof(objectCount));

            // 查找目标对象
            for (uint32_t i = 0; i < objectCount; ++i) {
                // 读取对象名称
                uint32_t nameLength;
                file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
                char* nameBuffer = new char[nameLength + 1];
                file.read(nameBuffer, nameLength);
                nameBuffer[nameLength] = '\0';
                std::string currentName(nameBuffer);
                delete[] nameBuffer;

                // 读取数据大小
                uint64_t dataSize;
                file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));

                if (currentName == objectName) {
                    // 找到目标对象，读取数据
                    void* objectPtr = nullptr;

                    if (objectName == "ControlData") {
                        objectPtr = controlData_;
                    }
                    else {
                        std::pair<void*, bip::managed_shared_memory::size_type> result =
                            segment->find<char>(objectName.c_str());
                        if (!result.first) {
                            log(LogLevel::Error, "在内存段中找不到对象: " + objectName);
                            file.close();
                            return false;
                        }
                        objectPtr = result.first;
                    }

                    // 读取对象数据
                    file.read(static_cast<char*>(objectPtr), dataSize);

                    file.close();
                    log(LogLevel::Info, "已从文件加载对象 " + objectName);
                    return true;
                }

                // 不是目标对象，跳过数据
                file.seekg(file.tellg() + static_cast<std::streamoff>(dataSize));
            }
        }
        else {
            // ASCII格式
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("OBJECT: ") == 0) {
                    std::string currentName = line.substr(8);

                    // 读取大小行
                    std::getline(file, line);
                    if (line.find("SIZE: ") != 0) {
                        log(LogLevel::Error, "无效的文件格式，缺少SIZE行");
                        file.close();
                        return false;
                    }

                    // 提取大小值(字节)
                    size_t sizePos = line.find(" bytes");
                    if (sizePos == std::string::npos) {
                        log(LogLevel::Error, "无效的文件格式，SIZE行格式错误");
                        file.close();
                        return false;
                    }
                    uint64_t dataSize = std::stoull(line.substr(6, sizePos - 6));

                    // 读取数据行
                    std::getline(file, line);
                    if (line.find("DATA_HEX: ") != 0) {
                        log(LogLevel::Error, "无效的文件格式，缺少DATA_HEX行");
                        file.close();
                        return false;
                    }

                    if (currentName == objectName) {
                        // 找到目标对象，读取数据
                        void* objectPtr = nullptr;

                        if (objectName == "ControlData") {
                            objectPtr = controlData_;
                        }
                        else {
                            std::pair<void*, bip::managed_shared_memory::size_type> result =
                                segment->find<char>(objectName.c_str());
                            if (!result.first) {
                                log(LogLevel::Error, "在内存段中找不到对象: " + objectName);
                                file.close();
                                return false;
                            }
                            objectPtr = result.first;
                        }

                        // 读取十六进制数据
                        unsigned char* bytes = static_cast<unsigned char*>(objectPtr);
                        std::string hexData = line.substr(10); // 跳过"DATA_HEX: "

                        // 继续读取可能的多行数据
                        while (hexData.length() < dataSize * 3) { // 每个字节需要2个十六进制字符+1个空格
                            std::getline(file, line);
                            if (line.find("         ") == 0) { // 9个空格前缀
                                hexData += line.substr(9);
                            }
                            else {
                                break;
                            }
                        }

                        // 解析十六进制字符串并填充对象
                        std::istringstream hexStream(hexData);
                        for (uint64_t i = 0; i < dataSize; ++i) {
                            int byteValue;
                            hexStream >> std::hex >> byteValue;
                            bytes[i] = static_cast<unsigned char>(byteValue);
                        }

                        file.close();
                        log(LogLevel::Info, "已从文件加载对象 " + objectName);
                        return true;
                    }

                    // 不是目标对象，跳过到下一个对象
                    while (std::getline(file, line) && !line.empty()) {
                        // 跳过所有行直到遇到空行
                    }
                }
            }
        }

        log(LogLevel::Error, "在文件中找不到对象: " + objectName);
        file.close();
        return false;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "从文件加载对象时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 内部辅助函数：从已打开的文件流读取对象
bool SharedMemoryManager::loadObjectFromFile(std::ifstream& file, std::shared_ptr<bip::managed_shared_memory> segment,
                                            bool binaryFormat) {
    try {
        std::string objectName;
        uint64_t dataSize = 0;

        if (binaryFormat) {
            // 读取对象名称
            uint32_t nameLength;
            file.read(reinterpret_cast<char*>(&nameLength), sizeof(nameLength));
            if (file.eof() || file.fail()) {
                return false; // 文件已结束
            }

            char* nameBuffer = new char[nameLength + 1];
            file.read(nameBuffer, nameLength);
            nameBuffer[nameLength] = '\0';
            objectName = nameBuffer;
            delete[] nameBuffer;

            // 读取数据大小
            file.read(reinterpret_cast<char*>(&dataSize), sizeof(dataSize));
        }
        else {
            // ASCII格式: 查找OBJECT行
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("OBJECT: ") == 0) {
                    objectName = line.substr(8);
                    break;
                }
                if (file.eof()) {
                    return false; // 文件已结束
                }
            }

            // 读取SIZE行
            std::getline(file, line);
            if (line.find("SIZE: ") != 0) {
                log(LogLevel::Error, "无效的文件格式，缺少SIZE行");
                return false;
            }

            size_t sizePos = line.find(" bytes");
            if (sizePos == std::string::npos) {
                log(LogLevel::Error, "无效的文件格式，SIZE行格式错误");
                return false;
            }
            dataSize = std::stoull(line.substr(6, sizePos - 6));

            // 读取DATA_HEX行
            std::getline(file, line);
            if (line.find("DATA_HEX: ") != 0) {
                log(LogLevel::Error, "无效的文件格式，缺少DATA_HEX行");
                return false;
            }
        }

        // 获取或创建对象
        void* objectPtr = nullptr;

        if (objectName == "ControlData") {
            objectPtr = controlData_;
        }
        else {
            // 尝试查找现有对象
            std::pair<void*, bip::managed_shared_memory::size_type> result =
                segment->find<char>(objectName.c_str());

            if (result.first) {
                objectPtr = result.first;
            }
            else {
                // 对象不存在，需要创建
                // 注意：这里简化了实现，实际情况可能需要根据对象类型进行特殊处理
                log(LogLevel::Warning, "对象不存在，无法从文件还原: " + objectName);

                if (binaryFormat) {
                    // 跳过数据
                    file.seekg(file.tellg() + static_cast<std::streamoff>(dataSize));
                }
                else {
                    // 跳过十六进制数据行
                    std::string line;
                    while (std::getline(file, line) && !line.empty()) {
                        // 跳过所有行直到遇到空行
                    }
                }

                return true; // 继续处理下一个对象
            }
        }

        // 读取数据并填充对象
        if (binaryFormat) {
            // 二进制直接读取
            file.read(static_cast<char*>(objectPtr), dataSize);
        }
        else {
            // ASCII格式解析十六进制
            std::string line;
            std::getline(file, line);
            if (line.find("DATA_HEX: ") != 0) {
                log(LogLevel::Error, "无效的文件格式，缺少DATA_HEX行");
                return false;
            }
            std::string hexData = line.substr(10); // 跳过"DATA_HEX: "

            // 继续读取可能的多行数据
            std::string nextLine;
            while (std::getline(file, nextLine) && nextLine.find("         ") == 0) { // 9个空格前缀
                hexData += nextLine.substr(9);
            }

            // 如果读取了一个非数据行，需要将文件指针回退
            if (!nextLine.empty() && nextLine.find("         ") != 0) {
                file.seekg(file.tellg() - static_cast<std::streamoff>(nextLine.length() + 1));
            }

            // 解析十六进制字符串并填充对象
            unsigned char* bytes = static_cast<unsigned char*>(objectPtr);
            std::istringstream hexStream(hexData);
            for (uint64_t i = 0; i < dataSize; ++i) {
                int byteValue;
                hexStream >> std::hex >> byteValue;
                bytes[i] = static_cast<unsigned char>(byteValue);
            }

            // 跳过可能的空行
            file.peek();
            if (!file.eof() && file.peek() == '\n') {
                std::getline(file, nextLine);
            }
        }

        log(LogLevel::Info, "从文件加载对象: " + objectName);
        return true;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "从文件流读取对象时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 创建共享内存快照(保存所有共享内存段)用于undo/redo操作
bool SharedMemoryManager::createSnapshot(const std::string& snapshotDir) {
    try {
        // 创建快照目录（如果不存在）
        if (!fs::exists(snapshotDir)) {
            if (!fs::create_directories(snapshotDir)) {
                log(LogLevel::Error, "无法创建快照目录: " + snapshotDir);
                return false;
            }
        }

        // 生成快照文件名（使用时间戳）
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << snapshotDir << "/snapshot_" << std::put_time(std::localtime(&timeT), "%Y%m%d_%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count();

        std::string snapshotPath = ss.str();

        // 保存所有共享内存段
        return saveToFile(snapshotPath, true); // 使用二进制格式保存
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "创建快照时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 恢复共享内存快照(仅creator调用)
bool SharedMemoryManager::restoreSnapshot(const std::string& snapshotDir) {
    if (!isCreator_) {
        log(LogLevel::Error, "只有Creator可以恢复共享内存快照");
        return false;
    }

    try {
        // 检查快照目录是否存在
        if (!fs::exists(snapshotDir)) {
            log(LogLevel::Error, "快照目录不存在: " + snapshotDir);
            return false;
        }

        // 查找最新的快照
        fs::path latestSnapshot;
        std::string latestTimestamp = "";

        for (const auto& entry : fs::directory_iterator(snapshotDir)) {
            if (entry.is_regular_file() && entry.path().filename().string().find("snapshot_") == 0) {
                // 使用文件名中的时间戳部分进行比较，而不是依赖文件系统时间
                std::string filename = entry.path().filename().string();

                // 假设快照文件名格式为 snapshot_YYYYMMDD_HHMMSS_ms
                if (filename.length() > 18) { // 确保文件名足够长以包含时间戳
                    std::string timestampStr = filename.substr(9); // 跳过"snapshot_"

                    // 简单比较字符串，因为时间戳格式是按照时间顺序排列的
                    if (timestampStr > latestTimestamp) {
                        latestTimestamp = timestampStr;
                        latestSnapshot = entry.path();
                    }
                }
            }
        }

        if (latestSnapshot.empty()) {
            log(LogLevel::Error, "在目录中找不到快照: " + snapshotDir);
            return false;
        }

        // 恢复最新的快照
        log(LogLevel::Info, "正在恢复快照: " + latestSnapshot.string());
        return loadFromFile(latestSnapshot.string(), true); // 二进制格式
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "恢复快照时发生异常: " + std::string(e.what()));
        return false;
    }
}

// 从控制数据中获取几何模型名称列表
void SharedMemoryManager::getControlDataModelNames(std::vector<std::string>& modelNames) {
    if (!controlData_) {
        log(LogLevel::Error, "控制数据对象未初始化");
        return;
    }

    try {
        // 加锁保护并获取数据
        bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

        // 清空输出向量
        modelNames.clear();

        // 复制模型名称
        for (const auto& name : controlData_->sharedModelNames) {
            modelNames.push_back(std::string(name.c_str()));
        }

        log(LogLevel::Debug, "获取模型名称列表成功：" + std::to_string(modelNames.size()) + " 个模型");
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "获取模型名称列表失败: " + std::string(e.what()));
    }
}

// 从控制数据中获取网格名称列表
void SharedMemoryManager::getControlDataMeshNames(std::vector<std::string>& meshNames) {
    if (!controlData_) {
        log(LogLevel::Error, "控制数据对象未初始化");
        return;
    }

    try {
        // 加锁保护并获取数据
        bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

        // 清空输出向量
        meshNames.clear();

        // 复制网格名称
        for (const auto& name : controlData_->sharedMeshNames) {
            meshNames.push_back(std::string(name.c_str()));
        }

        log(LogLevel::Debug, "获取网格名称列表成功：" + std::to_string(meshNames.size()) + " 个网格");
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "获取网格名称列表失败: " + std::string(e.what()));
    }
}

// 获取数据类型信息
void SharedMemoryManager::getDataTypeInfo(SharedData* data, bool& isFieldData, DataGeoType& type, bool& isSequentiallyMatchedWithMesh) {
    if (!data) {
        log(LogLevel::Error, "数据对象是空指针");
        return;
    }

    try {
        // 加锁保护并获取数据
        bip::scoped_lock<bip::interprocess_mutex> lock(data->mutex);

        // 获取数据类型信息
        isFieldData = data->isFieldData;
        type = data->type;
        isSequentiallyMatchedWithMesh = data->isSequentiallyMatchedWithMesh;
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "获取数据类型信息失败: " + std::string(e.what()));
    }
}

// 获取数据关联的网格名称
void SharedMemoryManager::getDataMeshName(SharedData* data, std::string& meshName) {
    if (!data) {
        log(LogLevel::Error, "数据对象是空指针");
        return;
    }

    try {
        // 加锁保护并获取数据
        bip::scoped_lock<bip::interprocess_mutex> lock(data->mutex);

        // 获取网格名称
        meshName = std::string(data->meshName.c_str());
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "获取数据关联的网格名称失败: " + std::string(e.what()));
    }
}

// 更新控制数据中的时间步长
void SharedMemoryManager::updateControlDataDt(double dt) {
    if (!controlData_) {
        log(LogLevel::Error, "控制数据对象未初始化");
        return;
    }

    try {
        // 加锁保护并更新数据
        bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

        // 更新时间步长
        controlData_->dt = dt;
        controlData_->version.store(controlData_->version.load() + 1);

        log(LogLevel::Debug, "更新时间步长成功：dt = " + std::to_string(dt));
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "更新时间步长失败: " + std::string(e.what()));
    }
}

// 更新控制数据中的当前时间
void SharedMemoryManager::updateControlDataTime(double t) {
    if (!controlData_) {
        log(LogLevel::Error, "控制数据对象未初始化");
        return;
    }

    try {
        // 加锁保护并更新数据
        bip::scoped_lock<bip::interprocess_mutex> lock(controlData_->mutex);

        // 更新当前时间
        controlData_->t = t;
        controlData_->version.store(controlData_->version.load() + 1);

        log(LogLevel::Debug, "更新当前时间成功：t = " + std::to_string(t));
    }
    catch (const std::exception& e) {
        log(LogLevel::Error, "更新当前时间失败: " + std::string(e.what()));
    }
}
