#include "SolverHub.h"

#include <limits>
#include <stdlib.h>
#include <sstream>
#include <stack>
#include <iostream>
#include <string>
#include <fstream>
#include "json.hpp"

#include "UniMesh.h"
#include "mesh_Edge.h"
#include "mesh_Facet.h"
#include "mesh_block.h"
#include "GeoException.h"
#include "Numeric.h"
#include "ConstFieldSolver.h"

using json = nlohmann::json;

using namespace std;
using namespace EMP;

using PairVector = std::vector<std::pair<int, int>>;
using PairSet = std::set<std::pair<int, int>>;

std::vector<SolverHub *> SolverHub::list;
int SolverHub::_current = -1;

Interface::Interface()
{
	isDebug = true;
}

Interface::~Interface()
{
}

//int Interface::addData(std::string dname, int storesize)
//{
//	if (!dataMap.count(dname)) {
//		Tdata newdata(dname,name,;
//		newdata.size = storesize;
//		newdata.time.clear();
//		newdata.data.clear();
//		newdata.name = dname;
//		newdata.pos = this;
//		dataPool.push_back(newdata);
//		//dataMap.clear();
//		//for (auto& d : dataPool) {
//		//	dataMap[d.name] = &d;
//		//}
//		dataMap[dname] = &(dataPool.back());
//	}
//	else {
//		dataMap[dname]->size = storesize;
//	}
//	return 0;
//}
//
//
//int Interface::setData(std::string dname, double t, ArrayXd& data)
//{
//	Tdata* thedata = dataMap[dname];
//	if (!thedata) return 1;
//	if (!data.size()) return 2;
//	if (thedata->time.size() && thedata->time.back() >= t) return 3;
//
//	thedata->time.push_back(t);
//	thedata->data.push_back(data);
//	if (thedata->time.size() > thedata->size)
//		thedata->time.pop_front();
//	if (thedata->data.size() > thedata->size)
//		thedata->data.pop_front();
//	if (isDebug)
//		GM_MESSAGE("%s = [%f, %f, %f]\n", dname.c_str(), data[0], data[1], data[2]);
//
//	return 0;
//}
//
//int Interface::setData(std::string dname, double t, std::vector<double>& data)
//{
//	Tdata* thedata = dataMap[dname];
//	if (!thedata) return 1;
//	if (!data.size()) return 2;
//	if (thedata->time.size() && thedata->time.back() >= t) return 3;
//	thedata->time.push_back(t);
//	thedata->data.push_back(Map<ArrayXd>(&(data[0]), data.size()));
//	if (thedata->time.size() > thedata->size)
//		thedata->time.pop_front();
//	if (thedata->data.size() > thedata->size)
//		thedata->data.pop_front();
//
//	if (isDebug)
//		GM_MESSAGE("%s = [%f, %f, %f]\n", dname.c_str(), data[0], data[1], data[2]);
//	return 0;
//}
//
//int Interface::getDataByPos(std::string dname, int i, ArrayXd& data)
//{
//	Tdata* thedata = dataMap[dname];
//	if (!thedata) return 1;
//	int dsize = thedata->data.size();
//	if (i >= dsize || -i >dsize) return 2;
//	if (i < 0) {
//		data = thedata->data[dsize + i];
//	}
//	else {
//		data = thedata->data[i];
//	}
//	if (isDebug)
//		GM_MESSAGE("%s = [%f, %f, %f]\n", dname.c_str(), data[0], data[1], data[2]);
//	return 0;
//}
//
//int Interface::getData(std::string dname, double t, ArrayXd& data)
//{
//	Tdata* thedata = dataMap[dname];
//	if (!thedata) return 1;
//	if (thedata->time.size() == 0)
//		return 3;
//	else if (thedata->time.size() == 1) {
//		if (t == thedata->time[0]) {
//			data = thedata->data[0];
//		}
//		else
//			return 4;
//	}
//	else {
//		if (thedata->time.back() < t) return 5;
//		int i = thedata->time.size() - 2;
//		for (;i < 0; i--) {
//			if (thedata->time[i] < t) break;
//		}
//		// 1st order interpolation
//		double coe = (t - thedata->time[i]) / (thedata->time[i + 1] - thedata->time[i]);
//		data = thedata->data[i] + coe * (thedata->data[i + 1] - thedata->data[i]);
//	}
//	if (isDebug)
//		GM_MESSAGE("%s = [%d, %d, %d]\n", dname.c_str(), data[0], data[1], data[2]);
//	return 0;
//}
//
//
//int Interface::getData(std::string dname, double t, std::vector<double>& data)
//{
//	Tdata* thedata = dataMap[dname];
//	if (!thedata) return 1;
//	data.clear();
//	if (thedata->time.size() == 0) 
//		return 3;
//	else if (thedata->time.size() == 1) {
//		if (t == thedata->time[0]) {
//			data.resize(thedata->data[0].size());
//			Map<ArrayXd> cdata(&(data[0]), data.size());
//			cdata = thedata->data[0];
//		}
//		else
//			return 4;
//	}
//	else {
//		if (thedata->time.back() < t) return 5;
//		int i = thedata->time.size() - 2;
//		for (;i < 0; i--) {
//			if (thedata->time[i] < t) break;
//		}
//		// 1st order interpolation
//		data.resize(thedata->data[i].size());
//
//		Map<ArrayXd> cdata(&(data[0]), data.size());
//		double coe = (t - thedata->time[i]) / (thedata->time[i + 1] - thedata->time[i]);
//		cdata = thedata->data[i] + coe * (thedata->data[i + 1] - thedata->data[i]);
//	}
//	if (isDebug)
//		GM_MESSAGE("%s = [%d, %d, %d]\n", dname.c_str(), data[0], data[1], data[2]);
//	return 0;
//}
//

//Solver::Solver(std::string _name, SolverHub *sys)
//{
//	name = _name;
//	simsystem = sys;
//	_index = -1;
//	timestamp = 0;
//	dt = 0;
//	updateRequired = false;
//	context = nullptr;
//	progbar = nullptr;
//	logger = nullptr;
//}
//
//Solver::~Solver()
//{
//	for (auto& d : rdata) {
//		delete d;
//	}
//	rdata.clear();
//	for (auto& d : wdata) {
//		delete d;
//	}
//	wdata.clear();
//}
//
//int Solver::init() {
//	Interface *f;
//	for (auto& r : rdata) {
//		f = simsystem->interfaceMap[r->interfaceName];
//		if (f) {
//			f->readtags.insert(std::make_pair(name, r.second));
//			f->addData(r.second);
//		}
//		else
//			return 1;
//	}
//	for (auto& r : writetags) {
//		f = simsystem->interfaceMap[r.first];
//		if (f) {
//			f->writetags.insert(std::make_pair(name, r.second));
//			f->addData(r.second);
//		}
//		else
//			return 1;
//	}
//	return 0;
//}
//
//bool Solver::checkTime(double t)
//{
//	if (timestamp <= t) {
//		updateRequired = true;
//		return true;
//	}
//	else
//		return false;
//
//}
//
//void Solver::updateTime(double dt) {
//	//change timestamp
//	timestamp += dt;
//	updateRequired = false;
//}

SolverHub::SolverHub(const std::string &name)
{
	// push new one into the list
	list.push_back(this);
	timestamp = 0;
	t_final = 0;
	_name = name;
	context = nullptr;
	progbar = nullptr;
	logger = nullptr;
}

SolverHub::~SolverHub()
{
	std::vector<SolverHub *>::iterator it =
		std::find(list.begin(), list.end(), this);
	if (it != list.end()) list.erase(it);

	//destroy();
}

//void SolverHub::deleteCouplingSystems()
//{
//	// Delete all InterfaceCouplings
//	for (int k = 0; k < SolverHub::list.size(); k++)
//		delete SolverHub::list[k];
//
//	std::vector<SolverHub *>().swap(SolverHub::list);
//}
//
//SolverHub *SolverHub::current(int index)
//{
//	if (list.empty()) {
//		GM_MESSAGE("No current model available: creating one");
//		new SolverHub();
//	}
//	if (index >= 0) _current = index;
//	if (_current < 0 || _current >= (int)list.size()) return list.back();
//	return list[_current];
//}
//
//int SolverHub::setCurrent(SolverHub *m)
//{
//	for (std::size_t i = 0; i < list.size(); i++) {
//		if (list[i] == m) {
//			_current = i;
//			break;
//		}
//	}
//	return _current;
//}
//
//SolverHub *SolverHub::findByName(const std::string name, const std::string fileName)
//{
//	// return last mesh with given name
//	for (int i = list.size() - 1; i >= 0; i--)
//		if (list[i]->getName() == name &&
//			(fileName.empty() || !list[i]->hasFileName(fileName)))
//			return list[i];
//	return 0;
//}
//
//void SolverHub::destroy(bool keepName)
//{
//	GM_MESSAGE("Destroying model %s", getName().c_str());
//	_destroying = true;
//
//	if (!keepName) {
//		_name.clear();
//		_fileNames.clear();
//	}
//
//	for (piter it = firstPart(); it != lastPart(); ++it) delete *it;
//	parts.clear();
//	std::vector<Solver *>().swap(parts);
//	for (auto& loc : interfaces)
//		delete loc;
//	interfaces.clear();
//	std::vector<Interface *>().swap(interfaces);
//
//	_destroying = false;
//}
//
//void SolverHub::remove(Solver *r)
//{
//	piter it = std::find(firstPart(), lastPart(), r);
//	if (it != (piter)parts.end()) parts.erase(it);
//}
//
//
//bool SolverHub::empty() const
//{
//	return !(parts.size());
//}
//
//void SolverHub::add(Solver *r) 
//{ 
//	parts.push_back(r); 
//	interfaces[r->name] = r;
//	r->context = context;
//	r->progbar = progbar;
//	r->logger = logger;
//}
//
//void SolverHub::add(Interface *loc)
//{
//	interfaces.push_back(loc);
//	interfaceMap[loc->name] = loc;
//}
//
//
//int SolverHub::exchangedata(std::string interfaceName, std::string dataName)
//{
//	Interface* inf = interfaceMap[interfaceName];
//	Solver *partW = nullptr, *partR = nullptr;
//	for (auto& wtag : inf->writetags) {
//		if (wtag.second == dataName) {
//			partW = interfaces[wtag.first];
//			if (partW->isUpdateRequired())
//				partW->write(dataName);
//		}
//	}
//	for (auto& rtag : inf->readtags) {
//		if (rtag.second == dataName) {
//			partR = interfaces[rtag.first];
//			if (partR->checkTime(partW->getTime()))
//				partR->read(dataName);
//		}
//	}
//	return 0;
//}
//
//void SolverHub::updateSystemTime()
//{
//	timestamp = BIGNUMBER;
//	for (auto& part : parts)
//	{
//		timestamp = min(timestamp, part->getTime());
//	}
//}
//
//bool SolverHub::continueRun()
//{
//	if (timestamp < t_final)
//		return true;
//	else
//		return false;
//}
//
//int SolverHub::init(ISolverContext* context_, ISolverProgress* progbar_, EMP::ILogger * logger_)
//{
//	context = context_;
//	progbar = progbar_;
//	logger = logger_;
//
//	CouplingPart_Fem3d::fields = {
//			kElectroStatic,                // "es"
//			kDirectCurrent,                // "sc"
//			kMagnetoStatic,                // "ms"
//			kEddyCurrent,                  // "qms"
//			kTransientMagnetodynamic,      // "qmst"
//			kRotatingMachineMagnetodynamic,// "rmmd"
//			kElectroDynamic,               // "qes"
//			kTransientElectroDynamic,      // "qest"
//			kElecMagnetWave,               // "wv"
//			//kFieldCircuitCoupling,       // Removed
//			kElasticStatic,                // "elst"
//			kForcedPeriodResponse,         // "fvbr"
//			kElasticDynamicTransient,      // "eldt"
//			kModalAnalysis,                // "ma"
//			kPiezoSteady,                  // "pzes"
//			kPiezoTransient,               // "pzed"
//			kPiezoFrequency,               // "pzef"
//			kThermalStationary,            // "therstat"
//			kThermalTransient,             // "thertran"
//			kFluid,                        // "fluid"
//			kRandomMovement,               // "rndmv"
//			kHeatConductingSteady,         // "hcs"
//			kHeatConductingTriansient,     // "hct"
//			kFemBemCoupling,               // "bemmd"
//			kFluidLaminar,                 // "fluid-lam"
//			kFluidKEpsilon,                // "fluid-tke"
//			kFluidKOmega,                  // "fluid-tko"
//			kMP_Dc_Hct,                    // "mp-sc-hct"
//			kMP_Md_Hcs,                    // "mp-qms-hcs"
//			kMP_Md_Hct,                    // "mp-qms-hct"
//			kMP_Mdt_Hcs,                   // "mp-qmst-hcs"
//			kMP_Mdt_Hct,                   // "mp-qmst-hct"
//			//Magnetomechanics,            // Removed
//			kMP_Md_Fvbr,                   // "mp-qms-fvbr"
//			kMP_Ms_Elst,                   // "mp-ms-elst"
//			kMP_Ms_Hcs,                    // "mp-ms-hcs"
//			kMP_Mdt_Elst,                  // "mp-qmst-elst"
//			kMP_Mdt_Eldt,                  // "mp-qmst-eldt"
//			kMP_Ms_Fluidlam,               // "mp-ms-fluid-lam"
//			kMP_Ms_FluidKE,                // "mp-ms-fluid-tke"
//			kMP_Ms_FluidKO,                // "mp-ms-fluid-tko"
//			kElectroStaticBEM,             // "bem-es"
//			kParaExtraction,               // "paraext"
//			kTrialCLN,                     // "*trialcln"
//			kDarwin                       // "*darwin"
//	};
//}
//
//int SolverHub::run()
//{
//	for (auto& part : parts) {
//		if(part->init())
//			return 1;
//	}
//
//	//// t=0
//	//for (auto& part : parts) {
//	//	part->start();
//	//}
//
//	while (continueRun())
//	{
//		for (auto& part : parts) {
//			if(part->step())
//				return 2;
//		}
//		updateSystemTime();
//	}
//	for (auto& part : parts) {
//		if(part->stop())
//			return 3;
//	}
//	return 0;
//}
//
//int SolverHub::readConfig(std::string fileName)
//{
//	json j;
//	ifstream jfile(fileName);
//	if (!jfile.is_open())
//		return 1;
//	jfile >> j;
//	bool is_coupling = false;
//	int num_analysis;
//	std::string jpb;
//
//	///2.read problem type,  is coupling?
//	if (j.find("coupling") != j.end())
//		is_coupling = j.at("coupling").get<bool>();
//	jpb = j.at("ProblemType").get<std::string>();
//	CouplingPart_Fem3d *fem3d = nullptr;
//	if (CouplingPart_Fem3d::fields.count(jpb)) {
//		fem3d = new CouplingPart_Fem3d("EmbSolver3d");
//		fem3d->problemType = jpb;
//		fem3d->path = _workingPath;
//		fem3d->casename = getName();
//		fem3d->inputFileName = _workingPath + getName();
//		fem3d->dt = 1;
//		fem3d->context = context;
//		fem3d->progbar = progbar;
//		fem3d->logger = logger;
//		add(fem3d);
//	}
//
//	bool embdyncoupling = false;
//	CouplingPart_MBDyn *mbdyn = nullptr;
//	if (embdyncoupling) {
//		mbdyn = new CouplingPart_MBDyn("MBDyn");
//		mbdyn->path = _workingPath;
//		mbdyn->scriptFileName = _workingPath + "external.mbd";
//		mbdyn->inputFileName = _workingPath + "IN.dat";
//		mbdyn->outputFileNames.push_back(_workingPath + "OUT.dat");
//		mbdyn->dt = 1;
//		mbdyn->nodeTag = 2;
//		add(mbdyn);
//	}
//
//	///read motion
//	vector< Interface *> mvfs;
//	if (j.find("Motion") != j.end())
//	{
//		std::cout << "reading motion settings" << std::endl;
//		//read_motions_json(j.at("Motion"));
//		json jmotion = j.at("Motion");
//		int num_motions;
//		num_motions = jmotion.at("num").get<int>();
//		if (num_motions > 0) {
//			if (jmotion.at("items").size() != num_motions)
//				printf("Error: The number of motions is set incorrectly!\n");
//			json jmotionlist = jmotion.at("items");
//			int num_mv = 0;
//			for (int i = 0; i < num_motions; i++) {
//				if (jmotionlist[i].find("remesh") != jmotionlist[i].end())
//				{
//					Interface *mf = new Interface();
//					if (jmotionlist[i].find("name") != jmotionlist[i].end()) {
//						mf->name = jmotionlist[i].at("name").get<std::string>();
//					}
//					else
//						mf->name = "interface_" + std::to_string(i);
//
//					add(mf);
//					mvfs.push_back(mf);
//
//					if (fem3d && embdyncoupling) {
//						Tdata *td = new Tdata("position", mf->name, 6, 1);
//						td->reader.insert(fem3d->name);
//						td->writer = mbdyn->name;
//						fem3d->rdata.push_back(td);
//						td = new Tdata("position", mf->name, 6, 1);
//						td->reader.insert(fem3d->name);
//						td->writer = mbdyn->name;
//						mbdyn->wdata.push_back(td);
//						
//						td = new Tdata("force", mf->name, 3, 1);
//						fem3d->wdata.push_back(td);
//						td = new Tdata("force", mf->name, 3, 1);
//						mbdyn->rdata.push_back(td);
//
//						t_final = 3;
//					}
//
//					CouplingPart_MeshSolver *meshsolver = new CouplingPart_MeshSolver("MovingMesh");
//					meshsolver->path = _workingPath;
//					meshsolver->inputFileName = _workingPath + getName();
//					meshsolver->dt = 1;
//					//meshsolver->interfaces.push_back(mf);
//					add(meshsolver);
//
//
//					num_mv = jmotionlist[i].at("num_mov_domains").get<int>();
//					std::vector<std::pair<int, int>> ids;
//					std::set<int> idset;
//					for (int j = 0; j < num_mv; j++) {
//						int domainId = jmotionlist[i].at("mov_domains")[j] + 1;
//						ids.push_back({ 3,domainId });
//						mf->dimtag.push_back({ 3,domainId });
//						idset.insert(domainId);
//						meshsolver->movingdomains.push_back(domainId);
//					}
//
//					//find airgap domain
//					std::vector<std::pair<int, int>> faces;
//					GModel* themodel = GModel::current();;
//					themodel->getBoundaryTags(ids, faces, true, false, false);
//					std::set<int> aid;
//					for (auto& ftag : faces) {
//						GFace* f = themodel->getFaceByTag(ftag.second);
//						for (auto& r : f->regions()) {
//							if (!idset.count(r->tag()))
//								aid.insert(r->tag());
//						}
//					}
//					if (aid.size() != 1)
//						return 3;
//					meshsolver->remeshDomainId = *(aid.begin());
//
//				}
//			}
//		}
//		////只为了EmbSolver3d - MBDyn 耦合，以后需要改成通用
//		//if (embdyncoupling) {
//		//	
//		//	for (Interface* mf : mvfs) {
//		//		mf->readtags.clear();
//		//		mf->readtags.insert(std::make_pair("EMP3d", "position"));
//		//		mf->readtags.insert(std::make_pair("MBDyn", "force"));
//		//		mf->writetags.clear();
//		//		mf->writetags.insert(std::make_pair("EMP3d", "force"));
//		//		mf->writetags.insert(std::make_pair("MBDyn", "position"));
//		//		mf->addData("position", 2);
//		//		mf->addData("force", 2);
//		//	}
//		//}
//	}
//
//	return 0;
//}
