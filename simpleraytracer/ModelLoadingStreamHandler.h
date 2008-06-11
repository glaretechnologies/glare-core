/*=====================================================================
ModelLoadingStreamHandler.h
---------------------------
File created by ClassTemplate on Fri Dec 01 18:14:30 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MODELLOADINGSTREAMHANDLER_H_666_
#define __MODELLOADINGSTREAMHANDLER_H_666_


#include <string>
#include <vector>
#include "../maths/vec2.h"
#include "../maths/vec3.h"


class ModelLoadingStreamHandlerExcep
{
public:
	ModelLoadingStreamHandlerExcep(const std::string& s_) : s(s_) {}
	~ModelLoadingStreamHandlerExcep(){}
	const std::string& what() const { return s; }
private:
	std::string s;
};


/*=====================================================================
ModelLoadingStreamHandler
-------------------------

=====================================================================*/
class ModelLoadingStreamHandler
{
public:
	/*=====================================================================
	ModelLoadingStreamHandler
	-------------------------
	
	=====================================================================*/
	ModelLoadingStreamHandler(){};
	virtual ~ModelLoadingStreamHandler(){};

	virtual void setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets) = 0;
	//virtual void addMaterial(const std::string& name, const Colour3& colour, float opacity) = 0;
	virtual void addMaterialUsed(const std::string& material_name) = 0;
	//virtual void addVertex(const Vec3f& pos, const Vec3f& normal, const std::vector<Vec2f>& texcoord_sets) = 0;
	//virtual void addTriangle(const unsigned int* vertex_indices, unsigned int material_index) = 0;
	virtual void addVertex(const Vec3f& pos) = 0;
	virtual void addVertex(const Vec3f& pos, const Vec3f& normal) = 0;
	virtual void addUVs(const std::vector<Vec2f>& uvs) = 0;
	virtual void addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index) = 0;
	virtual void addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index) = 0;
	virtual void endOfModel() = 0;
};


#endif //__MODELLOADINGSTREAMHANDLER_H_666_
