#include "Solver.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include "mesh_block.h"
#include "mesh_facet.h"
#include "mesh_edge.h"
#include "mesh_point.h"


namespace EMP {

    Solver::Solver(std::string _name)
		: name(_name), _index(0), solverType(UnknownSolver)
    {
        // 构造函数实现
    }

Solver::~Solver()
{
        // 析构函数实现，清理资源
        for (auto model : modelList) {
            if (model) {
                delete model;
            }
        }
        modelList.clear();

        for (auto mesh : meshList) {
            if (mesh) {
                delete mesh;
            }
        }
        meshList.clear();
    }

    int Solver::init()
    {
        // 默认实现，子类应重写此方法
        return 0;
    }

    int Solver::step()
    {
        // 默认实现，子类应重写此方法
        return 0;
    }

    int Solver::stop()
    {
        // 默认实现，子类应重写此方法
        return 0;
    }

    int Solver::readControlDataFromSharedControlData()
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 获取控制数据
            SharedControlData* ctrlData = sharedMemoryManager->getControlData();
            if (ctrlData == nullptr) {
                std::cerr << "Failed to get control data" << std::endl;
                return -2;
            }

            // 获取当前本地控制数据的版本号
            uint64_t currentVersion = localCtrlData.version;
            
            // 获取共享控制数据的版本号
            uint64_t sharedVersion = ctrlData->version.load();
            
            // 如果版本号相同，则不需要更新
            if (currentVersion == sharedVersion) {
                std::cout << "Control data is already up to date (version " << currentVersion << ")" << std::endl;
                return 0;
            }

            // 从共享控制数据复制到本地控制数据
            ctrlData->copyToLocal(localCtrlData);
            
            std::cout << "Updated control data from version " << currentVersion << " to " << localCtrlData.version << std::endl;
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in readControlDataFromSharedControlData: " << e.what() << std::endl;
            return -3;
        }
    }

    int Solver::generateGModelFromSharedGeometry()
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 获取控制数据中的几何模型名称列表
            std::vector<std::string> modelNames;
            sharedMemoryManager->getControlDataModelNames(modelNames);

            for (const auto& modelName : modelNames) {
                // 查找几何对象
                SharedGeometry* geo = sharedMemoryManager->findGeometryByName(modelName);
                if (!geo) {
                    std::cerr << "Failed to find geometry: " << modelName << std::endl;
                    continue;
                }

                // 将几何数据转换为本地数据
                LocalGeometry localGeo;
                localGeo.name = modelName; // 设置名称，以便在copyToLocal中查找对应几何体

                // 检查版本号，如果已经是最新版本则跳过
                if (localGeo.version == geo->version.load()) {
                    std::cout << "Geometry data " << modelName << " is already up to date (version " << localGeo.version << ")" << std::endl;
                    continue;
                }

                // 从共享几何数据复制到本地几何数据
                geo->copyToLocal(localGeo);
                
                // 现在localGeo可能包含多个几何体数据
                std::cout << "Retrieved " << localGeo.shapeNames.size() << " geometry objects from shared memory" << std::endl;
                
                // 处理所有几何体
                for (size_t i = 0; i < localGeo.shapeNames.size(); i++) {
                    const std::string& geoName = localGeo.shapeNames[i];
                    const std::string& geoFile = localGeo.shapeBrps[i];
                    
                    std::cout << "Processing geometry: " << geoName << ", file: " << geoFile << std::endl;
                    
                    // 创建新的GModel实例
                    GModel* model = new GModel();
                    model->_name = geoName;
                    // 填充模型数据...
                    
                    // 将模型添加到列表中
                    modelList.push_back(model);
                }
                
                // 向后兼容：如果没有使用向量存储，但有单一几何体
                if (localGeo.shapeNames.empty() && !localGeo.name.empty()) {
                    std::cout << "Processing single geometry: " << localGeo.name << std::endl;
                    
                    // 创建新的GModel实例
                    GModel* model = new GModel();
                    model->_name = localGeo.name;
                    // 其他初始化操作...
                    
                    // 将模型添加到列表中
                    modelList.push_back(model);
                }
            }
            
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in generateGModelFromSharedGeometry: " << e.what() << std::endl;
            return -2;
        }
    }

    int Solver::generateUniMeshFromSharedMesh()
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 获取控制数据中的网格名称列表
            std::vector<std::string> meshNames;
            sharedMemoryManager->getControlDataMeshNames(meshNames);

            for (const auto& meshName : meshNames) {
                // 查找网格对象
                SharedMesh* mesh = sharedMemoryManager->findMeshByName(meshName);
                if (!mesh) {
                    std::cerr << "Failed to find mesh: " << meshName << std::endl;
                    continue;
                }

                // 将网格数据转换为本地数据
                LocalMesh localMesh;
                localMesh.name = meshName;

                // 检查版本号，如果已经是最新版本则跳过
                if (localMesh.version == mesh->version.load()) {
                    std::cout << "Mesh data " << meshName << " is already up to date (version " << localMesh.version << ")" << std::endl;
                    continue;
                }

                // 从共享网格数据复制到本地网格数据
                mesh->copyToLocal(localMesh);

                // 创建新的UniMesh实例
                UniMesh* uniMesh = new UniMesh();
                
                // 将本地网格数据转换为UniMesh
                localMesh2UniMesh(&localMesh, uniMesh);

                // 将网格添加到列表中
                meshList.push_back(uniMesh);
            }

            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in generateUniMeshFromSharedMesh: " << e.what() << std::endl;
            return -2;
        }
    }

    int Solver::writeUniMeshToSharedMesh(std::string meshname)
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 查找网格对象
            UniMesh* uniMesh = getMeshByName(meshname);
            if (!uniMesh) {
                std::cerr << "Failed to find UniMesh: " << meshname << std::endl;
                return -2;
            }

            // 将UniMesh转换为本地网格数据
            LocalMesh localMesh;
            localMesh.name = meshname;
            
            // 从UniMesh获取模型名称
            if (!modelList.empty() && modelList[0]) {
                localMesh.modelName = modelList[0]->_name;
            }

            // 将UniMesh转换为本地网格
            uniMesh2LocalMesh(uniMesh, &localMesh);

            // 查找共享网格对象
            SharedMesh* sharedMesh = sharedMemoryManager->findMeshByName(meshname);
            if (!sharedMesh) {
                std::cerr << "Failed to find shared mesh: " << meshname << std::endl;
                return -3;
            }

            // 获取当前共享网格的版本号
            uint64_t currentVersion = sharedMesh->version.load();
            
            // 更新本地网格的版本号
            localMesh.version = currentVersion;
            
            // 将本地网格数据复制到共享网格数据
            sharedMemoryManager->updateMesh(sharedMesh, localMesh);
            
            std::cout << "Updated mesh data " << meshname << " to version " << sharedMesh->version.load() << std::endl;
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in writeUniMeshToSharedMesh: " << e.what() << std::endl;
            return -4;
        }
    }

    int Solver::localMeshNodes2mesh_PointList(LocalMesh* localMesh, mesh_PointList* pointList)
    {
        try {
            if (!localMesh || !pointList) {
                std::cerr << "Invalid parameters" << std::endl;
                return -1;
            }
            // 设置点的数量
            pointList->num = localMesh->nodes.size();
            
            // 为每个节点创建一个mesh_Point对象
            mesh_Point* p = nullptr;
            for (const auto& node : localMesh->nodes) {
                p = new mesh_Point();
                p->id = node.id;
                p->ref = node.ref;
                p->x = node.x;
                p->y = node.y;
                p->z = node.z;
                p->next = nullptr;
                
                // 将点添加到列表中
                if (pointList->head_point == nullptr) {
                    pointList->head_point = p;
                }
                else {
                    pointList->end_point->next = p;
                }
                pointList->end_point = p;
            }
            
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in localMeshNodes2mesh_PointList: " << e.what() << std::endl;
            return -2;
        }
    }

    int Solver::localMeshFacets2mesh_FacetList(LocalMesh* localMesh, mesh_PointList* pointList, mesh_FacetList* facetList, int** edge_ref)
    {
        try {
            if (!localMesh || !pointList || !facetList) {
                std::cerr << "Invalid parameters" << std::endl;
                return -1;
            }
            // 设置面的数量
            facetList->num = localMesh->triangles.size();
            
            // 为每个面创建一个mesh_Facet对象
            mesh_Facet* f = nullptr;
            int i = 0;
            for (const auto& tri : localMesh->triangles) {
                f = new mesh_Facet();
                f->triangle = new mesh_Triangle(f);
                f->id = tri.id;
                f->ref = tri.ref;
                
                // 设置面的节点
				f->set_point(0, pointList->get_point_by_id(tri.nodes[0]));
				f->set_point(1, pointList->get_point_by_id(tri.nodes[1]));
                f->set_point(2, pointList->get_point_by_id(tri.nodes[2]));
                
                edge_ref[0][i] = tri.edge_ref[0];
                edge_ref[1][i] = tri.edge_ref[1];
                edge_ref[2][i] = tri.edge_ref[2];
                i++;

                // 将面添加到列表中
                if (facetList->head_facet == nullptr) {
                    facetList->head_facet = f;
                }
                else {
                    facetList->end_facet->next = f;
                }
                facetList->end_facet = f;
            }
            
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in localMeshFacets2mesh_FacetList: " << e.what() << std::endl;
            return -2;
        }
    }

    void Solver::assign_ref2mesh_EdgeList(LocalMesh* localMesh, mesh_EdgeList* edgeList)
    {
        // 这里是一个示例实现，需要根据实际情况修改
        if (!localMesh || !edgeList) {
            std::cerr << "Invalid parameters" << std::endl;
            return;
        }
        
        // 遍历所有边
        for (mesh_Edge* edge = edgeList->head_edge; edge; edge = edge->next) {
            // 设置边的ref
            for (const auto& e : localMesh->edges) {
                if ((edge->point[0]->id == e.nodes[0] && edge->point[1]->id == e.nodes[1])||
                    (edge->point[0]->id == e.nodes[1] && edge->point[1]->id == e.nodes[2]) ){
                    edge->e_ref = e.ref;
                    break;
                }
            }
        }
    }


    int Solver::localMeshBlocks2mesh_BlockList(LocalMesh* localMesh, mesh_PointList* pointList, mesh_BlockList* blockList)
    {
        try {
            if (!localMesh || !pointList || !blockList) {
                std::cerr << "Invalid parameters" << std::endl;
                return -1;
            }
            // 设置块的数量
            blockList->num = localMesh->tetrahedrons.size();
            
            // 为每个块创建一个mesh_Block对象
            mesh_Block* b = nullptr;
            for (const auto& tet : localMesh->tetrahedrons) {
                b = new mesh_Block();
                b->tetra = new mesh_Tetra(b);
                b->id = tet.id;
                b->ref = tet.ref;
                
                // 设置块的节点
				b->set_point(0, pointList->get_point_by_id(tet.nodes[0]));
				b->set_point(1, pointList->get_point_by_id(tet.nodes[1]));
				b->set_point(2, pointList->get_point_by_id(tet.nodes[2]));
				b->set_point(3, pointList->get_point_by_id(tet.nodes[3]));
                
                // 将块添加到列表中
                if (blockList->head_block == nullptr) {
                    blockList->head_block = b;
                }
                else {
                    blockList->end_block->next = b;
                }
                blockList->end_block = b;
            }
            
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in localMeshBlocks2mesh_BlockList: " << e.what() << std::endl;
            return -2;
        }
    }
    int Solver::localMesh2UniMesh(LocalMesh* localMesh, UniMesh* uniMesh)
    {
        try {
            if (!localMesh || !uniMesh) {
                std::cerr << "Invalid parameters" << std::endl;
                return -1;
            }

            // 设置网格名称
            uniMesh->setName(localMesh->name);

            // 添加节点
            for (const auto& node : localMesh->nodes) {
                uniMesh->AddVertex(node.id, node.x, node.y, node.z);
                // 如果需要，设置节点的其他属性，如ref
            }

            // 添加边
            for (const auto& edge : localMesh->edges) {
                // 这里需要根据UniMesh的实际API调整
                // 假设UniMesh有AddEdge方法
                uniMesh->AddEdge(edge.id, edge.nodes[0], edge.nodes[1]);
            }

            // 添加三角形
            for (const auto& tri : localMesh->triangles) {
                // 这里需要根据UniMesh的实际API调整
                // 假设UniMesh有AddTriangle方法
                uniMesh->AddTriangle(tri.id, tri.nodes[0], tri.nodes[1], tri.nodes[2]);
            }

            // 添加四面体
            for (const auto& tet : localMesh->tetrahedrons) {
                // 这里需要根据UniMesh的实际API调整
                // 假设UniMesh有AddTetrahedron方法
                uniMesh->AddTetrahedron(tet.id, tet.nodes[0], tet.nodes[1], tet.nodes[2], tet.nodes[3]);
            }

            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in localMesh2UniMesh: " << e.what() << std::endl;
            return -2;
        }
    }

    int Solver::uniMesh2LocalMesh(UniMesh* uniMesh, LocalMesh* localMesh, bool onlyOnGEntity)
    {
        try {
            if (!uniMesh || !localMesh) {
                std::cerr << "Invalid parameters" << std::endl;
                return -1;
            }

            // 设置网格名称
            localMesh->name = uniMesh->getName();

            mesh_PointList* point_list = uniMesh->point_list;
			mesh_EdgeList* edge_list = uniMesh->edge_list;
			mesh_FacetList* facet_list = uniMesh->facet_list;
			mesh_BlockList* block_list = uniMesh->block_list;
            UniMeshInfo minfo = uniMesh->GetMeshInfo();

            // 清空本地网格数据
            localMesh->nodes.clear();
            localMesh->edges.clear();
            localMesh->triangles.clear();
            localMesh->tetrahedrons.clear();
            localMesh->nodes.reserve(uniMesh->point_list->num);
            if (onlyOnGEntity) {
				localMesh->edges.reserve(uniMesh->edge_list->num_ref);
				localMesh->triangles.reserve(uniMesh->facet_list->num_ref);
            }
            localMesh->edges.reserve(uniMesh->edge_list->num);
			localMesh->triangles.reserve(uniMesh->facet_list->num);
			localMesh->tetrahedrons.reserve(uniMesh->block_list->num);

            // 获取节点
            for (mesh_Point* p = point_list->head_point;p;p = p->next) {
				Node node{ p->id, p->ref, p->x, p->y, p->z };
                localMesh->nodes.push_back(node);
            }

            // 获取边
            for (mesh_Edge* edge = edge_list->head_edge; edge; edge = edge->next) {
                if (!onlyOnGEntity || edge->ref > 0) {
                    Edge e{ edge->id, edge->ref, edge->point[0]->id, edge->point[1]->id };
				    localMesh->edges.push_back(e);
                }
			}

			// 获取三角形
			for (mesh_Facet* triangle = facet_list->head_facet; triangle; triangle = triangle->next) {
                if (!onlyOnGEntity || triangle->ref > 0) {
                    Triangle tri{ triangle->id, triangle->ref, 
                        triangle->point[0]->id, triangle->point[1]->id, triangle->point[2]->id,
                        triangle->edge[0]->ref, triangle->edge[1]->ref, triangle->edge[2]->ref };
                    localMesh->triangles.push_back(tri);
                }
			}

            int ref_map[6] = { 0,1,3,2,4,5 };
			// 获取四面体
			for (mesh_Block* block = block_list->head_block; block; block = block->next) {
                Tetrahedron tet{ 
                    block->id, block->ref,
                    block->point[0]->id, block->point[1]->id, block->point[2]->id, block->point[3]->id,
					block->edge[ref_map[0]]->ref, block->edge[ref_map[1]]->ref, block->edge[ref_map[2]]->ref, block->edge[ref_map[3]]->ref, block->edge[ref_map[4]]->ref, block->edge[ref_map[5]]->ref,
					block->facet[0]->ref, block->facet[1]->ref, block->facet[2]->ref, block->facet[3]->ref
                };
				localMesh->tetrahedrons.push_back(tet);
			}

            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in uniMesh2LocalMesh: " << e.what() << std::endl;
            return -2;
        }
    }

    int Solver::readDataFromSharedDatas(std::string dataName, double& t, ArrayXd& data, ArrayXi& pos)
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 查找数据对象
            SharedData* sharedData = sharedMemoryManager->findDataByName(dataName);
            if (!sharedData) {
                std::cerr << "Failed to find data: " << dataName << std::endl;
                return -2;
            }

            // 创建一个临时的LocalData对象
            LocalData localData;
            
            // 检查版本号，如果已经更新则跳过
            bool isVersionSame = (localData.version == sharedData->version.load());
            if (isVersionSame) {
                std::cout << "Data " << dataName << " is already up to date (version " << localData.version << ")" << std::endl;
                return 0;
            }
            
            // 从共享数据复制到本地数据
            sharedData->copyToLocal(localData);

            // 设置返回值
            t = localData.t;
            
            // 将本地数据的值转换为Eigen::ArrayXd
            data.resize(localData.data.size());
            for (size_t i = 0; i < localData.data.size(); i++) {
                data(i) = localData.data[i];
            }
            
            // 将本地数据的索引转换为Eigen::ArrayXi
            pos.resize(localData.index.size());
            for (size_t i = 0; i < localData.index.size(); i++) {
                pos(i) = localData.index[i];
            }

            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in readDataFromSharedDatas: " << e.what() << std::endl;
            return -3;
        }
    }

    int Solver::readDataFromSharedDatas(std::string dataName, LocalData& data)
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 查找数据对象
            SharedData* sharedData = sharedMemoryManager->findDataByName(dataName);
            if (!sharedData) {
                std::cerr << "Failed to find data: " << dataName << std::endl;
                return -2;
            }

            // 设置数据名称
            data.name = dataName;
            
            // 检查版本号，如果已经是最新版本则跳过
            if (data.version == sharedData->version.load()) {
                std::cout << "Data " << dataName << " is already up to date (version " << data.version << ")" << std::endl;
                return 0;
            }
            
            // 从共享数据复制到本地数据
            sharedData->copyToLocal(data);

            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in readDataFromSharedDatas: " << e.what() << std::endl;
            return -3;
        }
    }

    int Solver::writeDataToSharedDatas(std::string dataName, double& t, const ArrayXd& data, const ArrayXi& pos)
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 查找数据对象
            SharedData* sharedData = sharedMemoryManager->findDataByName(dataName);
            if (!sharedData) {
                std::cerr << "Failed to find data: " << dataName << std::endl;
                return -2;
            }

            // 创建一个临时的LocalData对象
            LocalData localData;
            localData.name = dataName;
            localData.t = t;
            
            // 从SharedData获取一些必要的信息
            bool isFieldData;
            DataGeoType type;
            bool isSequentiallyMatchedWithMesh;
            std::string meshName;
            
            sharedMemoryManager->getDataTypeInfo(sharedData, isFieldData, type, isSequentiallyMatchedWithMesh);
            sharedMemoryManager->getDataMeshName(sharedData, meshName);
            
            localData.isFieldData = isFieldData;
            localData.type = type;
            localData.isSequentiallyMatchedWithMesh = isSequentiallyMatchedWithMesh;
            localData.meshName = meshName;
            
            // 将Eigen::ArrayXd转换为std::vector<double>
            localData.data.resize(data.size());
            for (int i = 0; i < data.size(); i++) {
                localData.data[i] = data(i);
            }

            // 将Eigen::ArrayXi转换为std::vector<int>
            localData.index.resize(pos.size());
            for (int i = 0; i < pos.size(); i++) {
                localData.index[i] = pos(i);
            }
            
            // 获取当前共享数据的版本号并设置到本地数据
            localData.version = sharedData->version.load();
            
            // 将本地数据复制到共享数据
            sharedMemoryManager->updateData(sharedData, localData);
            
            std::cout << "Updated data " << dataName << " to version " << sharedData->version.load() << std::endl;
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in writeDataToSharedDatas: " << e.what() << std::endl;
            return -3;
        }
    }

    int Solver::writeDataToSharedDatas(std::string dataName, LocalData& data)
    {
        try {
            if (sharedMemoryManager == nullptr) {
                std::cerr << "SharedMemoryManager is not initialized" << std::endl;
                return -1;
            }

            // 查找数据对象
            SharedData* sharedData = sharedMemoryManager->findDataByName(dataName);
            if (!sharedData) {
                std::cerr << "Failed to find data: " << dataName << std::endl;
                return -2;
            }

            // 确保数据名称正确
            data.name = dataName;
            
            // 获取当前共享数据的版本号
            uint64_t currentVersion = sharedData->version.load();
            
            // 如果版本号相同，不需要更新
            if (data.version == currentVersion) {
                std::cout << "Data " << dataName << " is already up to date (version " << data.version << ")" << std::endl;
                return 0;
            }
            
            // 更新本地数据的版本号
            data.version = currentVersion;
            
            // 将本地数据复制到共享数据
            sharedMemoryManager->updateData(sharedData, data);
            
            std::cout << "Updated data " << dataName << " from version " << currentVersion << " to " << sharedData->version.load() << std::endl;
            return 0;
        }
        catch (const std::exception& e) {
            std::cerr << "Exception in writeDataToSharedDatas: " << e.what() << std::endl;
            return -3;
        }
    }

    UniMesh* Solver::getMeshByName(std::string name)
    {
        for (auto mesh : meshList) {
            if (mesh && mesh->getName() == name) {
                return mesh;
            }
        }
        return nullptr;
    }

    GModel* Solver::getModelByName(std::string name)
    {
        for (auto model : modelList) {
            if (model && model->_name == name) {
                return model;
            }
        }
        return nullptr;
    }

    void Solver::setException(int type, int code, const std::string& message)
    {
        if (sharedMemoryManager) {
            sharedMemoryManager->setException(type, code, message);
        }
    }

    std::tuple<int, int, std::string> Solver::getAndClearException()
    {
        if (sharedMemoryManager) {
            return sharedMemoryManager->getAndClearException();
        }
        return std::make_tuple(0, 0, "SharedMemoryManager is not initialized");
    }

    double Solver::getTimeStep()
    {
        return localCtrlData.dt;
    }

    double Solver::getTime()
    {
        return localCtrlData.t;
    }

    void Solver::setTimeStep(double dt_)
    {
        if (dt_ > 0) {
            localCtrlData.dt = dt_;
            // 如果有共享内存管理器，则更新共享控制数据
            if (sharedMemoryManager) {
                sharedMemoryManager->updateControlDataDt(dt_);
            }
        }
    }

    void Solver::setTime(double t_)
    {
        localCtrlData.t = t_;
        // 如果有共享内存管理器，则更新共享控制数据
        if (sharedMemoryManager) {
            sharedMemoryManager->updateControlDataTime(t_);
        }
}

} // namespace EMP 