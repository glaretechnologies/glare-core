/*=====================================================================
raymesh.cpp
-----------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004Code By Nicholas Chapman.
=====================================================================*/
#include "raymesh.h"

#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../physics/jscol_tritree.h"
#include "../graphics/image.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/globals.h"
#include "../indigo/RendererSettings.h"
#include "../physics/jscol_BIHTree.h"
#include "../physics/jscol_tritree.h"
#include <fstream>
#include <algorithm>

RayMesh::RayMesh(const std::string& name_, bool enable_normal_smoothing_)
:	name(name_),
	tritree(NULL),
	enable_normal_smoothing(enable_normal_smoothing_)
{
	num_texcoord_sets = 0;
	num_vertices = 0;
	total_surface_area = 0.0f;
	done_init_as_emitter = false;
}


RayMesh::~RayMesh()
{	
}



//returns negative number if object not hit by the ray
double RayMesh::traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const
{
	return tritree->traceRay(ray, max_t, context, hitinfo_out);
}

const js::AABBox& RayMesh::getAABBoxWS() const
{
	return tritree->getAABBoxWS();
}

void RayMesh::getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const
{
	return tritree->getAllHits(ray, context, hitinfos_out);
}

bool RayMesh::doesFiniteRayHit(const Ray& ray, double raylength, js::TriTreePerThreadData& context) const
{
	return tritree->doesFiniteRayHit(ray, raylength, context);
}


const Vec3d RayMesh::getShadingNormal(const FullHitInfo& hitinfo) const
{
	const SimpleIndexTri& tri = triangles[hitinfo.hittri_index];

	if(!this->enable_normal_smoothing)
		return toVec3d(triNormal(hitinfo.hittri_index));

	const Vec3f& v0norm = vertNormal( tri.vertex_indices[0] );
	const Vec3f& v1norm = vertNormal( tri.vertex_indices[1] );
	const Vec3f& v2norm = vertNormal( tri.vertex_indices[2] );

	//NOTE: not normalising, because a raymesh is always contained by a InstancedGeom geometry,
	//which will normalise the normal.
	//return toVec3d(v0norm * (1.0f - (float)hitinfo.tri_coords.x - (float)hitinfo.tri_coords.y) + 
	//	(v1norm)*(float)hitinfo.tri_coords.x + 
	//	(v2norm)*(float)hitinfo.tri_coords.y);

	// Gratuitous removal of function calls
	const double w = 1.0 - hitinfo.tri_coords.x - hitinfo.tri_coords.y;
	return Vec3d(
		(double)v0norm.x * w + (double)v1norm.x * hitinfo.tri_coords.x + (double)v2norm.x * hitinfo.tri_coords.y,
		(double)v0norm.y * w + (double)v1norm.y * hitinfo.tri_coords.x + (double)v2norm.y * hitinfo.tri_coords.y,
		(double)v0norm.z * w + (double)v1norm.z * hitinfo.tri_coords.x + (double)v2norm.z * hitinfo.tri_coords.y
		);
}

const Vec3d RayMesh::getGeometricNormal(const FullHitInfo& hitinfo) const
{
	//return mesh.getTri(hitinfo.hittri_index).getNormal();
	//return triangles[hitinfo.hittri_index].getNormal();
	return toVec3d(triNormal(hitinfo.hittri_index));
}

/*void RayMesh::insertTri(js::Triangle& tri)
{
	tritree->insertTri(tri);
}*/

void RayMesh::build(bool use_cached_trees)
{
	assert(!tritree.get());
	if((int)triangles.size() >= RendererSettings::getInstance().bih_tri_threshold)
		tritree = std::auto_ptr<js::Tree>(new js::BIHTree(this));
	else
		tritree = std::auto_ptr<js::Tree>(new js::TriTree(this));
		
	//------------------------------------------------------------------------
	//print out our mem usage
	//------------------------------------------------------------------------
	conPrint("Building Mesh '" + name + "'...");
	conPrint("\t" + toString(num_vertices) + " vertices (" + ::getNiceByteSize(vertex_data.size()*sizeof(float)) + ")");
	conPrint("\t" + toString((unsigned int)triangles.size()) + " triangles (" + ::getNiceByteSize(triangles.size()*sizeof(SimpleIndexTri)) + ")");

	if(RendererSettings::getInstance().cache_trees && use_cached_trees)
	{
		bool built_from_cache = false;

		if(tritree->diskCachable())
		{
			//------------------------------------------------------------------------
			//Load from disk
			//------------------------------------------------------------------------
			const unsigned int tree_checksum = tritree->checksum();
			const std::string path = "tree_cache/" + toString(tree_checksum) + ".tre";

			std::ifstream file(path.c_str(), std::ifstream::binary);

			if(file)
			{
				unsigned int file_checksum;
				file.read((char*)&file_checksum, sizeof(unsigned int));

				assert(file_checksum == tree_checksum);

				conPrint("\tLoading tree from disk cache...");
				tritree->buildFromStream(file);
				built_from_cache = true;
				conPrint("\tDone.");
			}
			else
			{
				conPrint("\tCouldn't find matching cached tree file, rebuilding tree...");
			}

		}
		
		if(!built_from_cache)
		{
			tritree->build();
		
			if(tritree->diskCachable())
			{
				//------------------------------------------------------------------------
				//Save to disk
				//------------------------------------------------------------------------
				const std::string path = "tree_cache/" + toString(tritree->checksum()) + ".tre";

				conPrint("\tSaving tree to '" + path + "'...");

				std::ofstream cachefile(path.c_str(), std::ofstream::binary);

				if(cachefile)
				{
					tritree->saveTree(cachefile);
					conPrint("\tDone.");
				}
				else
				{
					conPrint("\tFailed to open file '" + path + "' for writing.");
				}
			}
		}
	}
	else
	{
		tritree->build();
	}


	//if(emitter_init)
	//	this->doInitAsEmitter();

	conPrint("Done Building Mesh '" + name + "'.");

	//mesh.buildTriNormals();//NEWCODE
	//------------------------------------------------------------------------
	//copy triangle normals
	//------------------------------------------------------------------------
	//for(unsigned int i=0; i<triangles.size(); ++i)
	//	triangles[i].normal = triNormal(i)

	//TEMP:
	/*js::TreeStats stats;
	tritree->getTreeStats(stats);

	if(diffuse_reflector)
	{

	}*/
}


const Vec2d RayMesh::getTexCoords(const FullHitInfo& hitinfo, unsigned int texcoords_set) const
{
	const Vec2f& v0tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[0], texcoords_set);
	const Vec2f& v1tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[1], texcoords_set);
	const Vec2f& v2tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[2], texcoords_set);
	
	//return toVec2d(
	//	v0tex*(1.0f - (float)hitinfo.tri_coords.x - (float)hitinfo.tri_coords.y) + v1tex*(float)hitinfo.tri_coords.x + v2tex*(float)hitinfo.tri_coords.y);

	// Gratuitous removal of function calls
	const double w = 1.0 - hitinfo.tri_coords.x - hitinfo.tri_coords.y;
	return Vec2d(
		(double)v0tex.x * w + (double)v1tex.x * hitinfo.tri_coords.x + (double)v2tex.x * hitinfo.tri_coords.y,
		(double)v0tex.y * w + (double)v1tex.y * hitinfo.tri_coords.x + (double)v2tex.y * hitinfo.tri_coords.y
		);
}



//NOTE: need to make this use shading normal
bool RayMesh::getTangents(const FullHitInfo& hitinfo, unsigned int texcoords_set, Vec3d& tangent_out, Vec3d& bitangent_out) const
{
	//const IndexTri& tri = mesh.getTri(hitinfo.hittri_index);

	/*const Vertex& v0 = mesh.getVert( mesh.getTri(hitinfo.hittri_index).vertex_indices[0] );
	const Vertex& v1 = mesh.getVert( mesh.getTri(hitinfo.hittri_index).vertex_indices[1] );
	const Vertex& v2 = mesh.getVert( mesh.getTri(hitinfo.hittri_index).vertex_indices[2] );*/

	const Vec3f& v0pos = triVertPos(hitinfo.hittri_index, 0);//tritree->triVertPos(hitinfo.hittri_index, 0);
	const Vec3f& v1pos = triVertPos(hitinfo.hittri_index, 1);//tritree->triVertPos(hitinfo.hittri_index, 1);
	const Vec3f& v2pos = triVertPos(hitinfo.hittri_index, 2);//tritree->triVertPos(hitinfo.hittri_index, 2);

	const Vec2f& v0tex = vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[0], texcoords_set);
	const Vec2f& v1tex = vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[1], texcoords_set);
	const Vec2f& v2tex = vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[2], texcoords_set);

	/*const float a = v1.texcoords.x - v0.texcoords.x;
	const float b = v1.texcoords.y - v0.texcoords.y;
	const float c = v2.texcoords.x - v0.texcoords.x;
	const float d = v2.texcoords.y - v0.texcoords.y;*/
	const float a = v1tex.x - v0tex.x;
	const float b = v1tex.y - v0tex.y;
	const float c = v2tex.x - v0tex.x;
	const float d = v2tex.y - v0tex.y;

	Matrix2f mat(a, b, c, d);
	if(mat.determinant() == 0.0f)
	{
		return false;
		//abort! abort!
		//tangent_out = Vec3(1,0,0);
		//bitangent_out = Vec3(0,1,0);
		//return;
	}


	mat.invert();

	/*const float det = 1.0f / (a*d - b*c);

	const float t11 = det * d;
	const float t12 = det * -b;
	const float t21 = det * -c;
	const float t22 = det * a;*/

	/*const Vec2 tangents_x = mat * Vec2(v1.pos.x - v0.pos.x, v2.pos.x - v0.pos.x);
	const Vec2 tangents_y = mat * Vec2(v1.pos.y - v0.pos.y, v2.pos.y - v0.pos.y);
	const Vec2 tangents_z = mat * Vec2(v1.pos.z - v0.pos.z, v2.pos.z - v0.pos.z);*/
	const Vec2f tangents_x = mat * Vec2f(v1pos.x - v0pos.x, v2pos.x - v0pos.x);
	const Vec2f tangents_y = mat * Vec2f(v1pos.y - v0pos.y, v2pos.y - v0pos.y);
	const Vec2f tangents_z = mat * Vec2f(v1pos.z - v0pos.z, v2pos.z - v0pos.z);


	tangent_out.set(tangents_x.x, tangents_y.x, tangents_z.x);
	bitangent_out.set(tangents_x.y, tangents_y.y, tangents_z.y);





//really old
	/*tangent_out.x = (v1.pos.x - v0.pos.x)*t11 + (v2.pos.x - v0.pos.x)*t12;
	tangent_out.y = (v1.pos.y - v0.pos.y)*t11 + (v2.pos.y - v0.pos.y)*t12;
	tangent_out.z = (v1.pos.z - v0.pos.z)*t11 + (v2.pos.z - v0.pos.z)*t12;*/
	//tangent_out.normalise();

	/*bitangent_out.x = (v1.pos.x - v0.pos.x)*t21 + (v2.pos.x - v0.pos.x)*t22;
	bitangent_out.y = (v1.pos.y - v0.pos.y)*t21 + (v2.pos.y - v0.pos.y)*t22;
	bitangent_out.z = (v1.pos.z - v0.pos.z)*t21 + (v2.pos.z - v0.pos.z)*t22;*/
	//bitangent_out.normalise();

	return true;
}



void RayMesh::setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets)
{
	num_texcoord_sets = myMax(num_texcoord_sets, max_num_texcoord_sets);
}

void RayMesh::addVertex(const Vec3f& pos, const Vec3f& normal, const std::vector<Vec2f>& texcoord_sets)
{
	//assert(normal.isUnitLength());
	if(!normal.isUnitLength())
		throw ModelLoadingStreamHandlerExcep("Normal was not unit length.");

	unsigned int oldsize = (unsigned int)vertex_data.size();
	vertex_data.resize(oldsize + vertSize());
	num_vertices++;

	*((Vec3f*)&vertex_data[oldsize]) = pos;
	oldsize += 3;
	*((Vec3f*)&vertex_data[oldsize]) = normal;
	oldsize += 3;
	assert(texcoord_sets.size() <= num_texcoord_sets);
	for(unsigned int i=0; i<texcoord_sets.size(); ++i)
	{
		assert(vertSize() >= 8);

		*((Vec2f*)&vertex_data[oldsize]) = texcoord_sets[i];
		oldsize += 2;
	}
}


inline static double getTriArea(const RayMesh& mesh, int tri_index, const Matrix3d& to_parent)
{
	const Vec3f& v0 = mesh.triVertPos(tri_index, 0);
	const Vec3f& v1 = mesh.triVertPos(tri_index, 1);
	const Vec3f& v2 = mesh.triVertPos(tri_index, 2);

	return ::crossProduct(to_parent * toVec3d(v1 - v0), to_parent * toVec3d(v2 - v0)).length() * 0.5;
}

inline static float getTriArea(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2)
{
	return ::crossProduct(v1 - v0, v2 - v0).length() * 0.5f;
}


void RayMesh::addTriangle(const unsigned int* vertex_indices, unsigned int material_index)
{
	// Check material index is in bounds
	if(material_index >= matname_to_index_map.size())
		throw ModelLoadingStreamHandlerExcep("Triangle material_index is out of bounds.");

	// Check vertex indices are in bounds
	for(unsigned int i=0; i<3; ++i)
		if(vertex_indices[i] >= num_vertices)
			throw ModelLoadingStreamHandlerExcep("Triangle vertex index is out of bounds.");

	// Check the area of the triangle
	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	// Push the triangle onto tri array.

	triangles.push_back(SimpleIndexTri());
	for(unsigned int i=0; i<3; ++i)
		triangles.back().vertex_indices[i] = vertex_indices[i];

	triangles.back().tri_mat_index = material_index;
}



void RayMesh::addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index)
{
	uvset_name_to_index[uv_set_name] = uv_set_index;
}

void RayMesh::addMaterialUsed(const std::string& material_name)
{
	const unsigned int mat_index = (unsigned int)matname_to_index_map.size();
	this->matname_to_index_map[material_name] = mat_index;
}

unsigned int RayMesh::getMaterialIndexForTri(unsigned int tri_index) const
{
	assert(tri_index < triangles.size());
	return triangles[tri_index].tri_mat_index;
}





void RayMesh::doInitAsEmitter()
{
	if(done_init_as_emitter)
		return;

	//------------------------------------------------------------------------
	//compute total surface area (in local coordinate space)
	//------------------------------------------------------------------------
	total_surface_area = 0.0;
	for(unsigned int i=0; i<this->getNumTris(); ++i)
		total_surface_area += (double)getTriArea(*this, i, Matrix3d::identity());

	//------------------------------------------------------------------------
	//build CDF
	//cdf[i] = sum of areas of triangles 0...i-1 / total area
	//cdf[0] = 0
	//cdf[1] = area of tri 0 / total area
	//cdf[2] = (area of tri 0 + area of tri 1) / total area
	//...
	//cdf[numtris] = (area of tri 0 + area of tri 1 + ... + area of tri numtris-1) / total area
	//------------------------------------------------------------------------
	cdf.resize(getNumTris() + 1);

	double sumarea = 0.0;
	cdf[0] = 0.0;
	for(unsigned int i=1; i<cdf.size(); ++i)
	{
		//sumarea += getTriArea(*this, i);
		//cdf[i] = sumarea / total_surface_area;

		cdf[i] = cdf[i-1] + ((double)getTriArea(*this, i-1, Matrix3d::identity()) / total_surface_area);

//		assert(cdf[i] >= 0.0f && cdf[i] <= 1.0f);
	}
	assert(::epsEqual(cdf.back(), 1.0));

	this->done_init_as_emitter = true;
}




const Vec3d RayMesh::sampleSurface(const Vec2d& samples, const Vec3d& viewer_point, Vec3d& normal_out,
										 HitInfo& hitinfo_out) const
{
	assert(done_init_as_emitter);

	//------------------------------------------------------------------------
	//pick triangle to sample
	//------------------------------------------------------------------------
	double xsample;
	unsigned int tri_index;

	//"The return value of lower_bound() is the iterator for the first element in 
	//the container that is greater than or equal to value"
	std::vector<double>::const_iterator it = std::lower_bound(cdf.begin(), cdf.end(), samples.x);
	const unsigned int n1 = (unsigned int)(it - cdf.begin());
	if(n1 == 0) //this will happen if samples.x == 0
	{
		tri_index = 0;
		xsample = 0.0;
	}
	else
	{
		assert(n1 < cdf.size());

		tri_index = n1 - 1;

		xsample = (samples.x - cdf[tri_index]) / (cdf[n1] - cdf[tri_index]);//renormalise x sample

		//const double low = *(i-1);//NOTE: this will fail if samples.x == 0 because i will be begin()
		//const double high = *(i);
		//const double xsample = (samples.x - low) / (high - low);//renormalise x sample
	}

	assert(xsample >= 0.0 && xsample <= 1.0);
	assert(tri_index >= 0 && tri_index < getNumTris());

	normal_out = toVec3d(triNormal(tri_index));//tri.getNormal();
	assert(normal_out.isUnitLength());

	//------------------------------------------------------------------------
	//pick point using barycentric coords
	//------------------------------------------------------------------------

	//see siggraph montecarlo course 2003 pg 47
	const double s = sqrt(xsample);
	const double t = samples.y;

	// Compute barycentric coords
	const double u = s * (1.0 - t);
	const double v = (s * t);

	hitinfo_out.hittriindex = tri_index;
	hitinfo_out.hittricoords.set(u, v);

	return toVec3d(
		triVertPos(tri_index, 0) * (float)(1.0 - s) + 
		triVertPos(tri_index, 1) * (float)u + 
		triVertPos(tri_index, 2) * (float)v
		);

	//return toVec3d(
	//	triVertPos(tri_index, 0) * (1.0 - s) + 
	//	triVertPos(tri_index, 1) * (s * (1.0 - t)) + 
	//	triVertPos(tri_index, 2) * (s * t)
	//	);
}

double RayMesh::surfacePDF(const Vec3d& pos, const Vec3d& normal, const Matrix3d& to_parent) const
{
	assert(done_init_as_emitter);
	assert(total_surface_area > 0.0);
	assert(normal.isUnitLength());

	
	// Generate a tangent and bitangent 

	Vec3d v2;//x axis

	//thanks to Pharr and Humprehys for this code
	if(fabs(normal.x) > fabs(normal.y))
	{
		const double recip_len = 1.0 / sqrt(normal.x * normal.x + normal.z * normal.z);

		v2.set(-normal.z * recip_len, 0.0, normal.x * recip_len);
	}
	else
	{
		const double recip_len = 1.0 / sqrt(normal.y * normal.y + normal.z * normal.z);

		v2.set(0.0, normal.z * recip_len, -normal.y * recip_len);
	}

	const Vec3d v1 = crossProduct(v2, normal);

	assert(v1.isUnitLength());
	assert(v2.isUnitLength());
	assert(::epsEqual(dot(normal, v1), 0.0));
	assert(::epsEqual(dot(normal, v2), 0.0));
	assert(::epsEqual(dot(v1, v2), 0.0));

	// Now get the transformed differential area
	const double da_primed = crossProduct(to_parent * v1, to_parent * v2).length();

	return 1.0 / (da_primed * total_surface_area);

	//return 1.0 / total_surface_area;
}


double RayMesh::surfaceArea(const Matrix3d& to_parent) const
{
	assert(done_init_as_emitter);
	//assert(total_surface_area > 0.0);

	double surface_area = 0.0;
	for(unsigned int i=0; i<this->getNumTris(); ++i)
		surface_area += (double)getTriArea(*this, i, to_parent);

	return surface_area;
}


void RayMesh::emitterInit()
{
	doInitAsEmitter();
}

int RayMesh::UVSetIndexForName(const std::string& uvset_name) const
{
	//if(matname_to_index_map.find(uvset_name) == matname_to_index_map.end())
	//	return -1;

	//return matname_to_index_map[uvset_name];
	std::map<std::string, int>::const_iterator it = matname_to_index_map.find(uvset_name);
	if(it == matname_to_index_map.end())
	{
		return -1;
	}
	else
	{
		return (*it).second;
	}
}











