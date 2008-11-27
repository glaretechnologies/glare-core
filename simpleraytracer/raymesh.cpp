/*=====================================================================
raymesh.cpp
-----------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004Code By Nicholas Chapman.
=====================================================================*/
#include "raymesh.h"


#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../physics/KDTree.h"
#include "../graphics/image.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../indigo/RendererSettings.h"
#include "../physics/jscol_BIHTree.h"
#include "../physics/KDTree.h"
#include "../physics/BVH.h"
#include "../physics/SimpleBVH.h"
#include "../physics/NBVH.h"
#include "../physics/jscol_ObjectTreePerThreadData.h"
#include "../utils/fileutils.h"
#include "../utils/timer.h"
#include "../indigo/DisplacementUtils.h"
#include <fstream>
#include <algorithm>
#include "../indigo/globals.h"
#include "../utils/stringutils.h"


RayMesh::RayMesh(const std::string& name_, bool enable_normal_smoothing_, unsigned int max_num_subdivisions_, 
				 double subdivide_pixel_threshold_, bool subdivision_smoothing_, double subdivide_curvature_threshold_, 
				 bool merge_vertices_with_same_pos_and_normal_,
				bool wrap_u_,
				bool wrap_v_,
				bool view_dependent_subdivision_,
				double displacement_error_threshold_
				 )
:	name(name_),
	tritree(NULL),
	enable_normal_smoothing(enable_normal_smoothing_),
	max_num_subdivisions(max_num_subdivisions_),
	subdivide_pixel_threshold(subdivide_pixel_threshold_),
	subdivide_curvature_threshold(subdivide_curvature_threshold_),
	subdivision_smoothing(subdivision_smoothing_),
	merge_vertices_with_same_pos_and_normal(merge_vertices_with_same_pos_and_normal_),
	wrap_u(wrap_u_),
	wrap_v(wrap_v_),
	view_dependent_subdivision(view_dependent_subdivision_),
	displacement_error_threshold(displacement_error_threshold_)
{
	subdivide_and_displace_done = false;
	vertex_shading_normals_provided = false;

	num_uvs_per_group = 0;
	num_uv_groups = 0;
}


RayMesh::~RayMesh()
{	
}


/*const std::string RayMesh::debugName() const 
{ 
	return "RayMesh (name=" + name + ")"; 
}*/


const std::string RayMesh::getName() const 
{ 
	return name; 
}


//returns negative number if object not hit by the ray
double RayMesh::traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const
{
	return tritree->traceRay(
		ray, 
		max_t, 
		thread_context, 
		context.tritree_context, 
		object, 
		hitinfo_out
		);
}


const js::AABBox& RayMesh::getAABBoxWS() const
{
	return tritree->getAABBoxWS();
}


void RayMesh::getAllHits(const Ray& ray, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	tritree->getAllHits(
		ray, // ray 
		thread_context, 
		context.tritree_context, // tri tree context
		object, // object context
		hitinfos_out
		);
}


bool RayMesh::doesFiniteRayHit(const Ray& ray, double raylength, ThreadContext& thread_context, js::ObjectTreePerThreadData& context, const Object* object) const
{
	return tritree->doesFiniteRayHit(
		ray, 
		raylength, 
		thread_context, 
		context.tritree_context,
		object
		);
}


const RayMesh::Vec3Type RayMesh::getShadingNormal(const HitInfo& hitinfo) const
{
	if(!this->enable_normal_smoothing)
		return triNormal(hitinfo.sub_elem_index);

	const RayMeshTriangle& tri = triangles[hitinfo.sub_elem_index];

	const Vec3f& v0norm = vertNormal( tri.vertex_indices[0] );
	const Vec3f& v1norm = vertNormal( tri.vertex_indices[1] );
	const Vec3f& v2norm = vertNormal( tri.vertex_indices[2] );

	//NOTE: not normalising, because a raymesh is always contained by a InstancedGeom geometry,
	//which will normalise the normal.
	//return toVec3d(v0norm * (1.0f - (float)hitinfo.tri_coords.x - (float)hitinfo.tri_coords.y) + 
	//	(v1norm)*(float)hitinfo.tri_coords.x + 
	//	(v2norm)*(float)hitinfo.tri_coords.y);

	// Gratuitous removal of function calls
	const Vec3RealType w = (Vec3RealType)1.0 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	return Vec3Type(
		v0norm.x * w + v1norm.x * hitinfo.sub_elem_coords.x + v2norm.x * hitinfo.sub_elem_coords.y,
		v0norm.y * w + v1norm.y * hitinfo.sub_elem_coords.x + v2norm.y * hitinfo.sub_elem_coords.y,
		v0norm.z * w + v1norm.z * hitinfo.sub_elem_coords.x + v2norm.z * hitinfo.sub_elem_coords.y
		);
}


const RayMesh::Vec3Type RayMesh::getGeometricNormal(const HitInfo& hitinfo) const
{
	return triNormal(hitinfo.sub_elem_index);
}


static inline bool operator < (const RayMeshVertex& a, const RayMeshVertex& b)
{
	if(a.pos < b.pos)
		return true;
	else if(b.pos < a.pos) // else if a.pos > b.pos
		return false;
	else	// else a.pos == b.pos
		return a.normal < b.normal;
}

/*
static bool isDisplacingMaterial(const std::vector<Reference<Material> >& materials)
{
	for(unsigned int i=0; i<materials.size(); ++i)
		if(materials[i]->displacing())
			return true;
	return false;
}*/


void RayMesh::subdivideAndDisplace(ThreadContext& context, const Object& object, const CoordFramed& camera_coordframe_os, double pixel_height_at_dist_one, 
								   //const std::vector<Reference<Material> >& materials, 
	const std::vector<Plane<double> >& camera_clip_planes
	)
{
	if(subdivide_and_displace_done)
	{
		// Throw exception if we are supposed to do a view-dependent subdivision
		//if(subdivide_pixel_threshold > 0.0)
		if(view_dependent_subdivision && max_num_subdivisions > 0)
			throw GeometryExcep("Tried to do a view-dependent subdivision on an instanced mesh.");

		// else not an error, but we don't need to subdivide again, so return.
		return;
	}

	if(merge_vertices_with_same_pos_and_normal)
		mergeVerticesWithSamePosAndNormal();
	
	if(!vertex_shading_normals_provided)
		computeShadingNormals();

	if(object.hasDisplacingMaterial() || max_num_subdivisions > 0)
	{
		conPrint("Subdividing and displacing mesh '" + this->getName() + "', (max num subdivisions = " + toString(max_num_subdivisions) + ") ...");

		// Convert to single precision floating point planes
		std::vector<Plane<float> > camera_clip_planes_f(camera_clip_planes.size());
		for(unsigned int i=0; i<camera_clip_planes_f.size(); ++i)
			camera_clip_planes_f[i] = Plane<float>(toVec3f(camera_clip_planes[i].getNormal()), (float)camera_clip_planes[i].getD());

		std::vector<RayMeshTriangle> temp_tris;
		std::vector<RayMeshVertex> temp_verts;
		std::vector<Vec2f> temp_uvs;

		DUOptions options;
		options.wrap_u = wrap_u;
		options.wrap_v = wrap_v;
		options.view_dependent_subdivision = view_dependent_subdivision;
		options.pixel_height_at_dist_one = pixel_height_at_dist_one;
		options.subdivide_pixel_threshold = subdivide_pixel_threshold;
		options.subdivide_curvature_threshold = subdivide_curvature_threshold;
		options.displacement_error_threshold = displacement_error_threshold;
		options.max_num_subdivisions = max_num_subdivisions;
		options.camera_clip_planes = camera_clip_planes_f;
		options.camera_coordframe_os = camera_coordframe_os;

		DisplacementUtils::subdivideAndDisplace(
			context,
			object,
			subdivision_smoothing,
			triangles,
			vertices,
			uvs,
			this->num_uvs_per_group,
			options,
			temp_tris,
			temp_verts,
			temp_uvs
			);

		triangles = temp_tris;
		vertices = temp_verts;
		uvs = temp_uvs;

		// Check data
#ifdef DEBUG
		for(unsigned int i=0; i<triangles.size(); ++i)
			for(unsigned int c=0; c<3; ++c)
			{
				assert(triangles[i].vertex_indices[c] < vertices.size());
				if(this->num_uvs_per_group > 0)
				{
					assert(triangles[i].uv_indices[c] < uvs.size());
				}
			}
#endif	
		conPrint("\tDone.");
	}

	subdivide_and_displace_done = true;
}


void RayMesh::build(const std::string& indigo_base_dir_path, const RendererSettings& renderer_settings)
{
	Timer timer;

	if(triangles.size() == 0)
		throw GeometryExcep("No triangles in mesh.");

	if(tritree.get())
		return; // build() has already been called.

	try
	{
		if((int)triangles.size() >= renderer_settings.bih_tri_threshold)
			tritree = std::auto_ptr<js::Tree>(new js::BVH(this));
			//tritree = std::auto_ptr<js::Tree>(new js::SimpleBVH(this));
		else
			tritree = std::auto_ptr<js::Tree>(new js::KDTree(this));
	}
	catch(js::TreeExcep& e)
	{
		throw GeometryExcep("Exception while creating tree: " + e.what());
	}
		
	//------------------------------------------------------------------------
	//print out our mem usage
	//------------------------------------------------------------------------
	
	conPrint("Building Mesh '" + name + "'...");
	conPrint("\t" + toString(getNumVerts()) + " vertices (" + ::getNiceByteSize(vertices.size() * sizeof(RayMeshVertex)) + ")");
	//conPrint("\t" + toString(getNumVerts()) + " vertices (" + ::getNiceByteSize(vertex_data.size()*sizeof(float)) + ")");
	conPrint("\t" + toString((unsigned int)triangles.size()) + " triangles (" + ::getNiceByteSize(triangles.size()*sizeof(RayMeshTriangle)) + ")");

	if(renderer_settings.cache_trees) //RendererSettings::getInstance().cache_trees && use_cached_trees)
	{
		bool built_from_cache = false;

		if(tritree->diskCachable())
		{
			//------------------------------------------------------------------------
			//Load from disk
			//------------------------------------------------------------------------
			const unsigned int tree_checksum = tritree->checksum();
			const std::string path = FileUtils::join(
				indigo_base_dir_path, 
				FileUtils::join("tree_cache", toString(tree_checksum) + ".tre")
				);

			std::ifstream file(path.c_str(), std::ifstream::binary);

			if(file)
			{
				//unsigned int file_checksum;
				//file.read((char*)&file_checksum, sizeof(unsigned int));

				//assert(file_checksum == tree_checksum);

				//conPrint("\tLoading tree from disk cache...");
				try
				{
					tritree->buildFromStream(file);
					built_from_cache = true;
				}
				catch(js::TreeExcep& e)
				{
					conPrint("\tError while loading cached tree: " + e.what());
				}
				//conPrint("\tDone.");
			}
			else
			{
				conPrint("\tCouldn't find matching cached tree file, rebuilding tree...");
			}

		}
		
		if(!built_from_cache)
		{
			try
			{
				tritree->build();
			}
			catch(js::TreeExcep& e)
			{
				throw GeometryExcep("Exception while building tree: " + e.what());
			}
		
			if(tritree->diskCachable())
			{
				//------------------------------------------------------------------------
				//Save to disk
				//------------------------------------------------------------------------
				const std::string path = FileUtils::join(
					indigo_base_dir_path, 
					FileUtils::join("tree_cache", toString(tritree->checksum()) + ".tre")
					);

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
		try
		{
			tritree->build();
		}
		catch(js::TreeExcep& e)
		{
			throw GeometryExcep("Exception while building tree: " + e.what());
		}
	}

	conPrint("Done Building Mesh '" + name + "'. (Time taken: " + toString(timer.getSecondsElapsed()) + " s)");
}


unsigned int RayMesh::getNumTexCoordSets() const
{ 
	return num_uvs_per_group; 
}


const RayMesh::TexCoordsType RayMesh::getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	assert(texcoords_set < num_uvs_per_group);
	const Vec2f& v0tex = uvs[triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uvs_per_group + texcoords_set];
	const Vec2f& v1tex = uvs[triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uvs_per_group + texcoords_set];
	const Vec2f& v2tex = uvs[triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uvs_per_group + texcoords_set];

	//const Vec2f& v0tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[0], texcoords_set);
	//const Vec2f& v1tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[1], texcoords_set);
	//const Vec2f& v2tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[2], texcoords_set);
	
	//return toVec2d(
	//	v0tex*(1.0f - (float)hitinfo.tri_coords.x - (float)hitinfo.tri_coords.y) + v1tex*(float)hitinfo.tri_coords.x + v2tex*(float)hitinfo.tri_coords.y);

	// Gratuitous removal of function calls
	const float w = (float)1.0 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	return RayMesh::TexCoordsType(
		v0tex.x * w + v1tex.x * hitinfo.sub_elem_coords.x + v2tex.x * hitinfo.sub_elem_coords.y,
		v0tex.y * w + v1tex.y * hitinfo.sub_elem_coords.x + v2tex.y * hitinfo.sub_elem_coords.y
		);
}


void RayMesh::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out, Vec3Type& dNs_du_out, Vec3Type& dNs_dv_out) const
{
	const Vec3f& v0pos = triVertPos(hitinfo.sub_elem_index, 0);
	const Vec3f& v1pos = triVertPos(hitinfo.sub_elem_index, 1);
	const Vec3f& v2pos = triVertPos(hitinfo.sub_elem_index, 2);

	dp_du_out = v1pos - v0pos;
	dp_dv_out = v2pos - v0pos;


	if(this->enable_normal_smoothing)
	{
		// TEMP NEW:
		//dp_du_out.removeComponentInDir(

		



		const RayMeshTriangle& tri = triangles[hitinfo.sub_elem_index];

		const Vec3f& v0norm = vertNormal( tri.vertex_indices[0] );
		const Vec3f& v1norm = vertNormal( tri.vertex_indices[1] );
		const Vec3f& v2norm = vertNormal( tri.vertex_indices[2] );

		dNs_du_out = v1norm - v0norm;
		dNs_dv_out = v2norm - v0norm;
	}
	else
	{
		dNs_du_out = dNs_dv_out = Vec3Type(0,0,0);
	}
}


void RayMesh::getTexCoordPartialDerivs(const HitInfo& hitinfo, unsigned int texcoords_set, TexCoordsRealType& ds_du_out, TexCoordsRealType& ds_dv_out, TexCoordsRealType& dt_du_out, TexCoordsRealType& dt_dv_out) const
{
	assert(texcoords_set < num_uvs_per_group);
	const Vec2f& v0tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uvs_per_group + texcoords_set];
	const Vec2f& v1tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uvs_per_group + texcoords_set];
	const Vec2f& v2tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uvs_per_group + texcoords_set];

	ds_du_out = v1tex.x - v0tex.x;
	dt_du_out = v1tex.y - v0tex.y;

	ds_dv_out = v2tex.x - v0tex.x;
	dt_dv_out = v2tex.y - v0tex.y;
}


void RayMesh::setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets)
{
	num_uvs_per_group = myMax(num_uvs_per_group, max_num_texcoord_sets);
}


void RayMesh::addVertex(const Vec3f& pos)
{
	vertices.push_back(RayMeshVertex(pos, Vec3f(0.f, 0.f, 0.0f)));
}

void RayMesh::addVertex(const Vec3f& pos, const Vec3f& normal)
{
	const Vec3f n = normalise(normal);

	if(!isFinite(normal.x) || !isFinite(normal.y) || !isFinite(normal.z))
		throw ModelLoadingStreamHandlerExcep("Invalid normal");
	
	vertices.push_back(RayMeshVertex(pos, n));

	this->vertex_shading_normals_provided = true;
}


inline static double getTriArea(const RayMesh& mesh, int tri_index, const Matrix3<RayMesh::Vec3RealType>& to_parent)
{
	const Vec3f& v0 = mesh.triVertPos(tri_index, 0);
	const Vec3f& v1 = mesh.triVertPos(tri_index, 1);
	const Vec3f& v2 = mesh.triVertPos(tri_index, 2);

	return ::crossProduct(to_parent * (v1 - v0), to_parent * (v2 - v0)).length() * 0.5;
}


inline static float getTriArea(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2)
{
	return ::crossProduct(v1 - v0, v2 - v0).length() * 0.5f;
}


void RayMesh::addUVs(const std::vector<Vec2f>& new_uvs)
{
	assert(new_uvs.size() <= num_uvs_per_group);

	for(unsigned int i=0; i<num_uvs_per_group; ++i)
	{
		if(i < new_uvs.size())
			uvs.push_back(new_uvs[i]);
		else
			uvs.push_back(Vec2f(0.f, 0.f));
	}

	num_uv_groups++;
}


void RayMesh::addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index)
{
	// Check material index is in bounds
	if(material_index >= getMaterialNameToIndexMap().size())
		throw ModelLoadingStreamHandlerExcep("Triangle material_index is out of bounds.  (material index=" + toString(material_index) + ")");

	// Check vertex indices are in bounds
	for(unsigned int i=0; i<3; ++i)
		if(vertex_indices[i] >= getNumVerts())
			throw ModelLoadingStreamHandlerExcep("Triangle vertex index is out of bounds.  (vertex index=" + toString(vertex_indices[i]) + ")");

	// Check uv indices are in bounds
	if(num_uvs_per_group > 0)
		for(unsigned int i=0; i<3; ++i)
			if(uv_indices[i] >= num_uv_groups)
				throw ModelLoadingStreamHandlerExcep("Triangle uv index is out of bounds.  (uv index=" + toString(uv_indices[i]) + ")");


	// Check the area of the triangle
	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	// Push the triangle onto tri array.

	triangles.push_back(RayMeshTriangle());
	for(unsigned int i=0; i<3; ++i)
	{
		triangles.back().vertex_indices[i] = vertex_indices[i];
		triangles.back().uv_indices[i] = uv_indices[i];
	}

	triangles.back().tri_mat_index = material_index;
}


void RayMesh::addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index)
{
	uvset_name_to_index[uv_set_name] = uv_set_index;
}


void RayMesh::addMaterialUsed(const std::string& material_name)
{
	if(matname_to_index_map.find(material_name) == matname_to_index_map.end()) // If this name not already added...
	{
		const unsigned int mat_index = (unsigned int)matname_to_index_map.size();
		this->matname_to_index_map[material_name] = mat_index;
	}
}


void RayMesh::endOfModel()
{
	// Check that any UV set expositions actually have the corresponding uv data.
	for(std::map<std::string, unsigned int>::const_iterator i=getUVSetNameToIndexMap().begin(); i != getUVSetNameToIndexMap().end(); ++i)
	{
		if((*i).second >= num_uvs_per_group)
			throw ModelLoadingStreamHandlerExcep("UV set with index " + toString((*i).second) + " was exposed, but the UV set data was not provided.");
	}
}


unsigned int RayMesh::getMaterialIndexForTri(unsigned int tri_index) const
{
	assert(tri_index < triangles.size());
	return triangles[tri_index].tri_mat_index;
}


void RayMesh::getSubElementSurfaceAreas(const Matrix3<Vec3RealType>& to_parent, std::vector<double>& surface_areas_out) const
{
	surface_areas_out.resize(triangles.size());

	for(unsigned int i=0; i<surface_areas_out.size(); ++i)
		surface_areas_out[i] = (double)getTriArea(*this, i, to_parent);

}

void RayMesh::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Vec3d& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const
{
	//------------------------------------------------------------------------
	//pick point using barycentric coords
	//------------------------------------------------------------------------

	//see siggraph montecarlo course 2003 pg 47
	const Vec3RealType s = sqrt(samples.x);
	const Vec3RealType t = samples.y;

	// Compute barycentric coords
	const Vec3RealType u = s * (1.0f - t);
	const Vec3RealType v = (s * t);

	hitinfo_out.sub_elem_index = sub_elem_index;
	hitinfo_out.sub_elem_coords.set(u, v);

	normal_out = triNormal(sub_elem_index);
	assert(normal_out.isUnitLength());

	pos_out = toVec3d(
		triVertPos(sub_elem_index, 0) * (1.0f - s) + 
		triVertPos(sub_elem_index, 1) * u + 
		triVertPos(sub_elem_index, 2) * v
		);
}


double RayMesh::subElementSamplingPDF(unsigned int sub_elem_index, const Vec3d& pos, double sub_elem_area_ws) const
{
	return 1.0 / sub_elem_area_ws;
}


void RayMesh::printTreeStats()
{
	tritree->printStats();
}


void RayMesh::printTraceStats()
{
	tritree->printTraceStats();
}


static inline const Vec3f triGeometricNormal(const std::vector<RayMeshVertex>& verts, unsigned int v0, unsigned int v1, unsigned int v2)
{
	return normalise(crossProduct(verts[v1].pos - verts[v0].pos, verts[v2].pos - verts[v0].pos));
}


void RayMesh::computeShadingNormals()
{
	conPrint("Computing shading normals for mesh '" + this->getName() + "'.");

	for(unsigned int i=0; i<vertices.size(); ++i)
		vertices[i].normal = Vec3f(0.f, 0.f, 0.f);

	for(unsigned int t=0; t<triangles.size(); ++t)
	{
		const Vec3f tri_normal = triGeometricNormal(
					vertices, 
					triangles[t].vertex_indices[0], 
					triangles[t].vertex_indices[1], 
					triangles[t].vertex_indices[2]
				);

		for(int i=0; i<3; ++i)
			vertices[triangles[t].vertex_indices[i]].normal += tri_normal;
	}

	for(unsigned int i=0; i<vertices.size(); ++i)
		vertices[i].normal.normalise();
}


void RayMesh::mergeVerticesWithSamePosAndNormal()
{
	conPrint("Merging vertices for mesh '" + this->getName() + "'...");
	conPrint("\tInitial num vertices: " + toString((unsigned int)vertices.size()));

	std::map<RayMeshVertex, unsigned int> new_vert_indices;
	std::vector<RayMeshVertex> newverts;
	
	for(unsigned int t=0; t<triangles.size(); ++t)
	{
		for(unsigned int i=0; i<3; ++i)
		{
			const RayMeshVertex& old_vert = vertices[triangles[t].vertex_indices[i]];

			unsigned int new_vert_index;

			const std::map<RayMeshVertex, unsigned int>::const_iterator result = new_vert_indices.find(old_vert);

			if(result == new_vert_indices.end())
			{
				new_vert_index = (unsigned int)newverts.size();
				newverts.push_back(old_vert);
				new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
			}
			else
				new_vert_index = (*result).second;

			triangles[t].vertex_indices[i] = new_vert_index;
		}
	}

	vertices = newverts;

	conPrint("\tNew num vertices: " + toString((unsigned int)vertices.size()) + "");
	conPrint("\tDone.");
}


bool RayMesh::isEnvSphereGeometry() const
{
	return false;
}
