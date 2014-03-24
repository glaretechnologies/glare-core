/*=====================================================================
csmodelloader.h
---------------
File created by ClassTemplate on Sat Nov 13 06:32:42 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CSMODELLOADER_H_666_
#define __CSMODELLOADER_H_666_


#include <vector>
#include <string>
#include <memory>
#include <map>
#include "../utils/NameMap.h"
class Object;
class ModelFormatDecoder;
class Texture;
class Material;
class MatOverride;

namespace Indigo
{
	class Mesh;
}

namespace CS{ class Material; }

class CSModelLoaderExcep
{
public:
	CSModelLoaderExcep(const std::string& s_) : s(s_) {}
	~CSModelLoaderExcep(){}

	const std::string& what() const { return s; }
private:
	std::string s;
};

/*=====================================================================
CSModelLoader
-------------
Loads a model off disk, using the ModelDecoder code to build an intermediate
model of type CS::Model.
A final set of ray tracer Object*s is built from the CS::Model.
=====================================================================*/
class CSModelLoader
{
public:
	/*=====================================================================
	CSModelLoader
	-------------
	
	=====================================================================*/
	CSModelLoader();

	~CSModelLoader();

	//throws CSModelLoaderExcep
	//void loadModel(const std::string& pathname, const std::string& workpath/*texturepath*/, const Matrix3& rotation, const Vec3& offset, 
	//				float scalefactor, std::map<std::string, MatOverride>& mat_overrrides,
	//				std::vector<Object*>& obs_out, bool enable_normal_smoothing);

	//loads materials found as well
	//IntermediateMesh* buildIntermediateMesh(const std::string& fullpathname,
	//	NameMap<Material*>& materials);

	void streamModel(const std::string& pathname, Indigo::Mesh& handler, float scale);



private:
	//void parseMaterial(const CS::Material& srcmat, const std::string& texworkpath, NameMap<Material*>& materials);
	//Texture* loadTexture(const std::string& pathname, const std::string& texworkpath);//const std::string& modelpath, const std::string& texturepath);

	std::auto_ptr<ModelFormatDecoder> decoder;

};




#endif //__CSMODELLOADER_H_666_




