/*=====================================================================
raymesh.cpp
-----------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "../indigo/EmbreeAccel.h"

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
#include "../indigo/PrintOutput.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#if defined(WIN32) || defined(WIN64)
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif


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

	num_uv_sets = 0;
	num_uv_groups = 0;
}


RayMesh::~RayMesh()
{	
	if(tritree)
	{
		tritree->~Tree();
		SSE::alignedFree(tritree);
	}
}


const std::string RayMesh::getName() const 
{ 
	return name; 
}


//returns negative number if object not hit by the ray
Geometry::DistType RayMesh::traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const
{
	//if(this->areSubElementsCurved())
	//	ignore_tri = std::numeric_limits<unsigned int>::max();

	return tritree->traceRay(
		ray,
		max_t,
		thread_context,
		object,
		ignore_tri,
		hitinfo_out
		);
}


const js::AABBox& RayMesh::getAABBoxWS() const
{
	return tritree->getAABBoxWS();
}


void RayMesh::getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	tritree->getAllHits(
		ray, // ray 
		thread_context, 
		object, // object context
		hitinfos_out
		);
}


bool RayMesh::doesFiniteRayHit(const Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const
{
	return tritree->doesFiniteRayHit(
		ray,
		raylength,
		thread_context,
		object,
		ignore_tri
		);
}


const RayMesh::Vec3Type RayMesh::getShadingNormal(const HitInfo& hitinfo) const
{
	//if(!this->enable_normal_smoothing)
	if(this->triangles[hitinfo.sub_elem_index].getUseShadingNormals() == 0)
	{
		//Vec4f n;
		////this->triangle_geom_normals[hitinfo.sub_elem_index].vectorToVec4f(n);
		//this->triangles[hitinfo.sub_elem_index].geom_normal.vectorToVec4f(n);
		//return n;

		const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
		const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
		const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
		const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
		const Vec3f e0(v1.pos - v0.pos);
		const Vec3f e1(v2.pos - v0.pos);
		return RayMesh::Vec3Type((e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
								 (e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
								 (e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);
	}

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
		v0norm.z * w + v1norm.z * hitinfo.sub_elem_coords.x + v2norm.z * hitinfo.sub_elem_coords.y,
		0.f
		);
}


const RayMesh::Vec3Type RayMesh::getGeometricNormal(const HitInfo& hitinfo) const
{
	//Vec4f n;
	////triNormal(hitinfo.sub_elem_index).vectorToVec4f(n);
	////this->triangle_geom_normals[hitinfo.sub_elem_index].vectorToVec4f(n);
	//this->triangles[hitinfo.sub_elem_index].geom_normal.vectorToVec4f(n);
	//return n;
	////return triNormal(hitinfo.sub_elem_index).toVec4fVector(); // This is slower :(

	const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
	const Vec3f e0(v1.pos - v0.pos);
	const Vec3f e1(v2.pos - v0.pos);
	return RayMesh::Vec3Type((e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
							 (e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
							 (e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);
}


void RayMesh::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out) const
{
	// Set N_g_os_out
	////triNormal(hitinfo.sub_elem_index).vectorToVec4f(N_g_os_out);
	////this->triangle_geom_normals[hitinfo.sub_elem_index].vectorToVec4f(N_g_os_out);
	//this->triangles[hitinfo.sub_elem_index].geom_normal.vectorToVec4f(N_g_os_out);
	const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
	const Vec3f e0(v1.pos - v0.pos);
	const Vec3f e1(v2.pos - v0.pos);
	N_g_os_out.set((e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
				   (e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
				   (e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);

	// Set N_s_os_out
	//if(!this->enable_normal_smoothing)
	if(tri.getUseShadingNormals() == 0)
	{
		//this->triangle_geom_normals[hitinfo.sub_elem_index].vectorToVec4f(N_s_os_out);
		N_s_os_out = N_g_os_out;
	}
	else
	{
		const Vec3f& v0norm = vertNormal( tri.vertex_indices[0] );
		const Vec3f& v1norm = vertNormal( tri.vertex_indices[1] );
		const Vec3f& v2norm = vertNormal( tri.vertex_indices[2] );

		// Gratuitous removal of function calls
		const Vec3RealType w = (Vec3RealType)1.0 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
		N_s_os_out = Vec3Type(
			v0norm.x * w + v1norm.x * hitinfo.sub_elem_coords.x + v2norm.x * hitinfo.sub_elem_coords.y,
			v0norm.y * w + v1norm.y * hitinfo.sub_elem_coords.x + v2norm.y * hitinfo.sub_elem_coords.y,
			v0norm.z * w + v1norm.z * hitinfo.sub_elem_coords.x + v2norm.z * hitinfo.sub_elem_coords.y,
			0.f
			);
	}

	mat_index_out = this->triangles[hitinfo.sub_elem_index].getTriMatIndex();
}


/*
static bool isDisplacingMaterial(const std::vector<Reference<Material> >& materials)
{
	for(unsigned int i=0; i<materials.size(); ++i)
		if(materials[i]->displacing())
			return true;
	return false;
}*/


void RayMesh::subdivideAndDisplace(ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one, 
								   //const std::vector<Reference<Material> >& materials, 
	const std::vector<Plane<Vec3RealType> >& camera_clip_planes_os, PrintOutput& print_output, bool verbose
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
		mergeVerticesWithSamePosAndNormal(print_output, verbose);
	
	if(!vertex_shading_normals_provided)
		computeShadingNormals(print_output, verbose);

	if(object.hasDisplacingMaterial() || max_num_subdivisions > 0)
	{
		if(verbose) print_output.print("Subdividing and displacing mesh '" + this->getName() + "', (max num subdivisions = " + toString(max_num_subdivisions) + ") ...");

		// Convert to single precision floating point planes
		/*std::vector<Plane<float> > camera_clip_planes_f(camera_clip_planes.size());
		for(unsigned int i=0; i<camera_clip_planes_f.size(); ++i)
			camera_clip_planes_f[i] = Plane<float>(toVec3f(camera_clip_planes[i].getNormal()), (float)camera_clip_planes[i].getD());*/

		js::Vector<RayMeshTriangle, 16> temp_tris;
		std::vector<RayMeshVertex> temp_verts;
		std::vector<Vec2f> temp_uvs;

		DUOptions options;
		options.object_to_camera = object_to_camera;
		options.wrap_u = wrap_u;
		options.wrap_v = wrap_v;
		options.view_dependent_subdivision = view_dependent_subdivision;
		options.pixel_height_at_dist_one = pixel_height_at_dist_one;
		options.subdivide_pixel_threshold = subdivide_pixel_threshold;
		options.subdivide_curvature_threshold = subdivide_curvature_threshold;
		options.displacement_error_threshold = displacement_error_threshold;
		options.max_num_subdivisions = max_num_subdivisions;
		options.camera_clip_planes_os = camera_clip_planes_os;

		DisplacementUtils::subdivideAndDisplace(
			print_output,
			context,
			object.getMaterials(),
			subdivision_smoothing,
			triangles,
			quads,
			vertices,
			uvs,
			this->num_uv_sets,
			options,
			temp_tris,
			temp_verts,
			temp_uvs
			);

		triangles = temp_tris;
		vertices = temp_verts;
		uvs = temp_uvs;

		this->quads.clearAndFreeMem();

		assert(num_uv_sets == 0 || ((temp_uvs.size() % num_uv_sets) == 0));
		this->num_uv_groups = num_uv_sets > 0 ? temp_uvs.size() / num_uv_sets : 0;

		// Check data
#ifdef DEBUG
		for(unsigned int i = 0; i < triangles.size(); ++i)
			for(unsigned int c = 0; c < 3; ++c)
			{
				assert(triangles[i].vertex_indices[c] < vertices.size());
				if(this->num_uv_sets > 0)
				{
					assert(triangles[i].uv_indices[c] < uvs.size());
				}
			}
#endif	
		if(verbose) print_output.print("\tDone.");
	}
	
	// convert any quads to tris
	if(this->getNumQuads() > 0)
	{
		for(size_t i=0; i<this->quads.size(); ++i)
		{
			const RayMeshQuad& q = this->quads[i];
			this->triangles.push_back(RayMeshTriangle(q.vertex_indices[0], q.vertex_indices[1], q.vertex_indices[2], q.getMatIndex()));
			this->triangles.back().uv_indices[0] = q.uv_indices[0];
			this->triangles.back().uv_indices[1] = q.uv_indices[1];
			this->triangles.back().uv_indices[2] = q.uv_indices[2];
			this->triangles.back().setUseShadingNormals(q.getUseShadingNormals());

			this->triangles.push_back(RayMeshTriangle(q.vertex_indices[0], q.vertex_indices[2], q.vertex_indices[3], q.getMatIndex()));
			this->triangles.back().uv_indices[0] = q.uv_indices[0];
			this->triangles.back().uv_indices[1] = q.uv_indices[2];
			this->triangles.back().uv_indices[2] = q.uv_indices[3];
			this->triangles.back().setUseShadingNormals(q.getUseShadingNormals());
		}

		this->quads.clearAndFreeMem();
	}

	subdivide_and_displace_done = true;
}


void RayMesh::build(const std::string& appdata_path, const RendererSettings& renderer_settings, PrintOutput& print_output, bool verbose)
{
	Timer timer;

	if(triangles.size() == 0)
		throw GeometryExcep("No triangles in mesh.");

	if(tritree != NULL)
		return; // build() has already been called.


	//NOTE: disabled due to questionable speed-up offered.
	//mergeUVs(print_output, verbose);

	float max_r2 = 0;
	for(size_t i = 0; i < this->vertices.size(); ++i)
		max_r2 = myMax(max_r2, this->vertices[i].pos.length2());
	bounding_radius = std::sqrt(max_r2);


	//NEW: Build triangle_geom_normals
	//this->triangle_geom_normals.resize(this->triangles.size());
	const size_t num_tris = triangles.size();
	for(size_t i = 0; i < num_tris; ++i)
	{
		RayMeshTriangle& tri(triangles[i]);
		const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
		const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
		const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
		const double p0[3] = { (double)v0.pos.x, (double)v0.pos.y, (double)v0.pos.z };
		const double e0[3] = { (double)v1.pos.x - p0[0], (double)v1.pos.y - p0[1], (double)v1.pos.z - p0[2] };
		const double e1[3] = { (double)v2.pos.x - p0[0], (double)v2.pos.y - p0[1], (double)v2.pos.z - p0[2] };
		const double nv[3] = {  e0[1] * e1[2] - e0[2] * e1[1],
								e0[2] * e1[0] - e0[0] * e1[2],
								e0[0] * e1[1] - e0[1] * e1[0] };
		tri.inv_cross_magnitude = (float)(1.0 / sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]));
	}

	try
	{
		if(renderer_settings.use_embree)
		{
			tritree = new (SSE::alignedSSEMalloc(sizeof(EmbreeAccel))) EmbreeAccel(this);
		}
		else
		{
			if((int)triangles.size() >= renderer_settings.bih_tri_threshold)
				tritree = new (SSE::alignedSSEMalloc(sizeof(js::BVH))) js::BVH(this);
				//tritree = std::auto_ptr<js::Tree>(new js::BVH(this));
				//tritree = std::auto_ptr<js::Tree>(new js::SimpleBVH(this));
			else
				tritree = new (SSE::alignedSSEMalloc(sizeof(js::KDTree))) js::KDTree(this);
				//tritree = std::auto_ptr<js::Tree>(new js::KDTree(this));
		}
}
	catch(js::TreeExcep& e)
	{
		throw GeometryExcep("Exception while creating tree: " + e.what());
	}
		
	//------------------------------------------------------------------------
	//print out our mem usage
	//------------------------------------------------------------------------
	
	if(verbose)
	{
		print_output.print("Building Mesh '" + name + "'...");
		print_output.print("\t" + toString(getNumVerts()) + " vertices (" + ::getNiceByteSize(vertices.size() * sizeof(RayMeshVertex)) + ")");
		//print_output.print("\t" + toString(getNumVerts()) + " vertices (" + ::getNiceByteSize(vertex_data.size()*sizeof(float)) + ")");
		print_output.print("\t" + toString((unsigned int)triangles.size()) + " triangles (" + ::getNiceByteSize(triangles.size()*sizeof(RayMeshTriangle)) + ")");
		print_output.print("\t" + toString((unsigned int)uvs.size()) + " UVs (" + ::getNiceByteSize(uvs.size()*sizeof(Vec2f)) + ")");
	}

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
				appdata_path, 
				FileUtils::join("cache/tree_cache", toString(tree_checksum) + ".tre")
				);

			std::ifstream file(FileUtils::convertUTF8ToFStreamPath(path).c_str(), std::ifstream::binary);

			if(file)
			{
				//unsigned int file_checksum;
				//file.read((char*)&file_checksum, sizeof(unsigned int));

				//assert(file_checksum == tree_checksum);

				//conPrint("\tLoading tree from disk cache...");
				try
				{
					tritree->buildFromStream(file, print_output, verbose);
					built_from_cache = true;
				}
				catch(js::TreeExcep& e)
				{
					print_output.print("\tError while loading cached tree: " + e.what());
				}
				//conPrint("\tDone.");
			}
			else
			{
				if(verbose) print_output.print("\tCouldn't find matching cached tree file, rebuilding tree...");
			}

		}
		
		if(!built_from_cache)
		{
			try
			{
				tritree->build(print_output, verbose);
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
					appdata_path, 
					FileUtils::join("cache/tree_cache", toString(tritree->checksum()) + ".tre")
					);

				if(verbose) print_output.print("\tSaving tree to '" + path + "'...");

				std::ofstream cachefile(FileUtils::convertUTF8ToFStreamPath(path).c_str(), std::ofstream::binary);

				if(cachefile)
				{
					tritree->saveTree(cachefile);
					if(verbose) print_output.print("\tDone.");
				}
				else
				{
					print_output.print("\tFailed to open file '" + path + "' for writing.");
				}
			}
		}
	}
	else
	{
		try
		{
			tritree->build(print_output, verbose);
		}
		catch(js::TreeExcep& e)
		{
			throw GeometryExcep("Exception while building tree: " + e.what());
		}
	}

	if(verbose) print_output.print("Done Building Mesh '" + name + "'. (Time taken: " + toString(timer.getSecondsElapsed()) + " s)");
}


unsigned int RayMesh::getNumTexCoordSets() const
{ 
	return num_uv_sets; 
}


const RayMesh::TexCoordsType RayMesh::getTexCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	assert(texcoords_set < num_uv_sets);
	assert(hitinfo.sub_elem_index < triangles.size());
	const Vec2f& v0tex = uvs[triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uv_sets + texcoords_set];
	const Vec2f& v1tex = uvs[triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uv_sets + texcoords_set];
	const Vec2f& v2tex = uvs[triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uv_sets + texcoords_set];

	//const Vec2f& v0tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[0], texcoords_set);
	//const Vec2f& v1tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[1], texcoords_set);
	//const Vec2f& v2tex = this->vertTexCoord(triangles[hitinfo.hittri_index].vertex_indices[2], texcoords_set);
	
	//return toVec2d(
	//	v0tex*(1.0f - (float)hitinfo.tri_coords.x - (float)hitinfo.tri_coords.y) + v1tex*(float)hitinfo.tri_coords.x + v2tex*(float)hitinfo.tri_coords.y);

	// Gratuitous removal of function calls
	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	return RayMesh::TexCoordsType(
		v0tex.x * w + v1tex.x * hitinfo.sub_elem_coords.x + v2tex.x * hitinfo.sub_elem_coords.y,
		v0tex.y * w + v1tex.y * hitinfo.sub_elem_coords.x + v2tex.y * hitinfo.sub_elem_coords.y
		);
}


void RayMesh::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out, Vec3Type& dNs_dalpha_out, Vec3Type& dNs_dbeta_out) const
{
	Vec4f v0pos;
	Vec4f v1pos;
	Vec4f v2pos;
	triVertPos(hitinfo.sub_elem_index, 0).pointToVec4f(v0pos);
	triVertPos(hitinfo.sub_elem_index, 1).pointToVec4f(v1pos);
	triVertPos(hitinfo.sub_elem_index, 2).pointToVec4f(v2pos);


	dp_dalpha_out = v1pos - v0pos;
	dp_dbeta_out = v2pos - v0pos;


	//if(this->enable_normal_smoothing)
	if(triangles[hitinfo.sub_elem_index].getUseShadingNormals() != 0)
	{
		const RayMeshTriangle& tri = triangles[hitinfo.sub_elem_index];

		Vec4f v0norm;
		Vec4f v1norm;
		Vec4f v2norm;
		vertNormal( tri.vertex_indices[0] ).vectorToVec4f(v0norm);
		vertNormal( tri.vertex_indices[1] ).vectorToVec4f(v1norm);
		vertNormal( tri.vertex_indices[2] ).vectorToVec4f(v2norm);

		dNs_dalpha_out = v1norm - v0norm;
		dNs_dbeta_out = v2norm - v0norm;
	}
	else
	{
		dNs_dalpha_out = dNs_dbeta_out = Vec3Type(0,0,0,0);
	}
}


void RayMesh::getUVPartialDerivs(const HitInfo& hitinfo, unsigned int texcoords_set, 
										TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, 
										TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out
									   ) const
{
	assert(texcoords_set < num_uv_sets);
	const Vec2f& v0tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uv_sets + texcoords_set];
	const Vec2f& v1tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uv_sets + texcoords_set];
	const Vec2f& v2tex = this->uvs[triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uv_sets + texcoords_set];

	/*ds_du_out = v1tex.x - v0tex.x;
	dt_du_out = v1tex.y - v0tex.y;

	ds_dv_out = v2tex.x - v0tex.x;
	dt_dv_out = v2tex.y - v0tex.y;*/

	du_dalpha_out =  v1tex.x - v0tex.x;
	dv_dalpha_out =  v1tex.y - v0tex.y;

	du_dbeta_out =  v2tex.x - v0tex.x;
	dv_dbeta_out =  v2tex.y - v0tex.y;
}


inline static const ::Vec3f toVec3(const Indigo::Vec3f& v)
{
	return ::Vec3f(v.x, v.y, v.z);
}


inline static const ::Vec2f toVec2(const Indigo::Vec2f& v)
{
	return ::Vec2f(v.x, v.y);
}


// This can be optimised quite a bit...
inline static float getTriArea(const RayMesh& mesh, int tri_index, const Matrix4f& to_parent)
{
	/*const Vec3f& v0 = mesh.triVertPos(tri_index, 0);
	const Vec3f& v1 = mesh.triVertPos(tri_index, 1);
	const Vec3f& v2 = mesh.triVertPos(tri_index, 2);*/
	Vec4f v0, v1, v2;
	mesh.triVertPos(tri_index, 0).vectorToVec4f(v0);
	mesh.triVertPos(tri_index, 1).vectorToVec4f(v1);
	mesh.triVertPos(tri_index, 2).vectorToVec4f(v2);

	const Vec4f e0(to_parent * (v1 - v0));
	const Vec4f e1(to_parent * (v2 - v0));

	return Vec4f(crossProduct(e0, e1)).length() * 0.5f;
}


inline static float getTriArea(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2)
{
	return ::crossProduct(v1 - v0, v2 - v0).length() * 0.5f;
}


void RayMesh::fromIndigoMesh(const Indigo::Mesh& mesh)
{
	this->setMaxNumTexcoordSets(mesh.num_uv_mappings);

	for(size_t i = 0; i < mesh.used_materials.size(); ++i)
		this->addMaterialUsed(toStdString(mesh.used_materials[i]));

	// Copy Vertices
	this->vertices.resize(mesh.vert_positions.size());

	if(mesh.vert_normals.size() == 0)
	{
		for(size_t i = 0; i < mesh.vert_positions.size(); ++i)
		{
			this->vertices[i].pos.set(mesh.vert_positions[i].x, mesh.vert_positions[i].y, mesh.vert_positions[i].z);
			this->vertices[i].normal.set(0.f, 0.f, 0.f);
		}
	}
	else
	{
		assert(mesh.vert_normals.size() == mesh.vert_positions.size());

		for(size_t i = 0; i < mesh.vert_positions.size(); ++i)
		{
			this->vertices[i].pos.set(mesh.vert_positions[i].x, mesh.vert_positions[i].y, mesh.vert_positions[i].z);
			this->vertices[i].normal.set(mesh.vert_normals[i].x, mesh.vert_normals[i].y, mesh.vert_normals[i].z);

			assert(::isFinite(mesh.vert_normals[i].x));
		}
	}

	// UVs
	assert(mesh.num_uv_mappings == 0 || (mesh.uv_pairs.size() % mesh.num_uv_mappings == 0));


	this->uvs.resize(mesh.uv_pairs.size());

	for(size_t i = 0; i < mesh.uv_pairs.size(); ++i)
		this->uvs[i].set(mesh.uv_pairs[i].x, mesh.uv_pairs[i].y);

	this->num_uv_groups = mesh.num_uv_mappings == 0 ? 0 : (unsigned int)mesh.uv_pairs.size() / mesh.num_uv_mappings;

	// Triangles
	this->triangles.reserve(mesh.triangles.size());
	unsigned int dest_i = 0;
	for(size_t i = 0; i < mesh.triangles.size(); ++i)
	{
		const Indigo::Triangle& src_tri = mesh.triangles[i];

		// Check material index is in bounds
		if(src_tri.tri_mat_index >= getMaterialNameToIndexMap().size())
			throw ModelLoadingStreamHandlerExcep("Triangle material_index is out of bounds.  (material index=" + toString(mesh.triangles[i].tri_mat_index) + ")");

		// Check vertex indices are in bounds
		for(unsigned int v = 0; v < 3; ++v)
			if(src_tri.vertex_indices[v] >= getNumVerts())
				throw ModelLoadingStreamHandlerExcep("Triangle vertex index is out of bounds.  (vertex index=" + toString(mesh.triangles[i].vertex_indices[v]) + ")");

		// Check uv indices are in bounds
		if(num_uv_sets > 0)
			for(unsigned int v = 0; v < 3; ++v)
				if(src_tri.uv_indices[v] >= num_uv_groups)
					throw ModelLoadingStreamHandlerExcep("Triangle uv index is out of bounds.  (uv index=" + toString(mesh.triangles[i].uv_indices[v]) + ")");

		// Check the area of the triangle.
		// If the area is zero, then the geometric normal will be undefined, and it will lead to NaN shading normals.
		const float MIN_TRIANGLE_AREA = 1.0e-20f;
		if(getTriArea(
			vertPos(src_tri.vertex_indices[0]), 
			vertPos(src_tri.vertex_indices[1]), 
			vertPos(src_tri.vertex_indices[2])) < MIN_TRIANGLE_AREA)
		{
			//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
			//return;
		}
		else
		{
			// Add a triangle
			this->triangles.resize(this->triangles.size() + 1);

			RayMeshTriangle& dest_tri = this->triangles[dest_i];

			dest_tri.vertex_indices[0] = src_tri.vertex_indices[0];
			dest_tri.vertex_indices[1] = src_tri.vertex_indices[1];
			dest_tri.vertex_indices[2] = src_tri.vertex_indices[2];
			
			dest_tri.uv_indices[0] = src_tri.uv_indices[0];
			dest_tri.uv_indices[1] = src_tri.uv_indices[1];
			dest_tri.uv_indices[2] = src_tri.uv_indices[2];
		
			dest_tri.setTriMatIndex(src_tri.tri_mat_index);
			dest_tri.setUseShadingNormals(this->enable_normal_smoothing);

			dest_i++;
		}
	}

	// Quads
	this->quads.resize(mesh.quads.size());
	for(size_t i = 0; i < mesh.quads.size(); ++i)
	{
		// Check material index is in bounds
		if(mesh.quads[i].mat_index >= getMaterialNameToIndexMap().size())
			throw ModelLoadingStreamHandlerExcep("Quad material_index is out of bounds.  (material index=" + toString(mesh.quads[i].mat_index) + ")");

		// Check vertex indices are in bounds
		for(unsigned int v = 0; v < 4; ++v)
			if(mesh.quads[i].vertex_indices[v] >= getNumVerts())
				throw ModelLoadingStreamHandlerExcep("Quad vertex index is out of bounds.  (vertex index=" + toString(mesh.quads[i].vertex_indices[v]) + ")");

		// Check uv indices are in bounds
		if(num_uv_sets > 0)
			for(unsigned int v = 0; v < 4; ++v)
				if(mesh.quads[i].uv_indices[v] >= num_uv_groups)
					throw ModelLoadingStreamHandlerExcep("Quad uv index is out of bounds.  (uv index=" + toString(mesh.quads[i].uv_indices[v]) + ")");


		this->quads[i].vertex_indices[0] = mesh.quads[i].vertex_indices[0];
		this->quads[i].vertex_indices[1] = mesh.quads[i].vertex_indices[1];
		this->quads[i].vertex_indices[2] = mesh.quads[i].vertex_indices[2];
		this->quads[i].vertex_indices[3] = mesh.quads[i].vertex_indices[3];

		this->quads[i].uv_indices[0] = mesh.quads[i].uv_indices[0];
		this->quads[i].uv_indices[1] = mesh.quads[i].uv_indices[1];
		this->quads[i].uv_indices[2] = mesh.quads[i].uv_indices[2];
		this->quads[i].uv_indices[3] = mesh.quads[i].uv_indices[3];

		this->quads[i].setMatIndex(mesh.quads[i].mat_index);
		this->quads[i].setUseShadingNormals(this->enable_normal_smoothing);
	}

	// UV set expositions
	//for(unsigned int i=0; i<mesh.uv_set_expositions.size(); ++i)
	//	this->addUVSetExposition(mesh.uv_set_expositions[i].uv_set_name, mesh.uv_set_expositions[i].uv_set_index);

	this->endOfModel();
}


void RayMesh::setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets)
{
	num_uv_sets = myMax(num_uv_sets, max_num_texcoord_sets);
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


void RayMesh::addVertexUnchecked(const Vec3f& pos, const Vec3f& normal)
{
	const Vec3f n = normalise(normal);

	vertices.push_back(RayMeshVertex(pos, n));

	this->vertex_shading_normals_provided = true;
}





void RayMesh::addUVs(const std::vector<Vec2f>& new_uvs)
{
	assert(new_uvs.size() <= num_uv_sets);

	for(unsigned int i = 0; i < num_uv_sets; ++i)
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
	for(unsigned int i = 0; i < 3; ++i)
		if(vertex_indices[i] >= getNumVerts())
			throw ModelLoadingStreamHandlerExcep("Triangle vertex index is out of bounds.  (vertex index=" + toString(vertex_indices[i]) + ")");

	// Check uv indices are in bounds
	if(num_uv_sets > 0)
		for(unsigned int i = 0; i < 3; ++i)
			if(uv_indices[i] >= num_uv_groups)
				throw ModelLoadingStreamHandlerExcep("Triangle uv index is out of bounds.  (uv index=" + toString(uv_indices[i]) + ")");


	// Check the area of the triangle
	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	// Push the triangle onto tri array.

	triangles.push_back(RayMeshTriangle());
	for(unsigned int i = 0; i < 3; ++i)
	{
		triangles.back().vertex_indices[i] = vertex_indices[i];
		triangles.back().uv_indices[i] = uv_indices[i];
	}

	triangles.back().setTriMatIndex(material_index);
	triangles.back().setUseShadingNormals(this->enable_normal_smoothing);
}


void RayMesh::addQuad(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index)
{
	// Check material index is in bounds
	if(material_index >= getMaterialNameToIndexMap().size())
		throw ModelLoadingStreamHandlerExcep("Quad material_index is out of bounds.  (material index=" + toString(material_index) + ")");

	// Check vertex indices are in bounds
	for(unsigned int i = 0; i < 4; ++i)
		if(vertex_indices[i] >= getNumVerts())
			throw ModelLoadingStreamHandlerExcep("Quad vertex index is out of bounds.  (vertex index=" + toString(vertex_indices[i]) + ")");

	// Check uv indices are in bounds
	if(num_uv_sets > 0)
		for(unsigned int i = 0; i < 4; ++i)
			if(uv_indices[i] >= num_uv_groups)
				throw ModelLoadingStreamHandlerExcep("Quad uv index is out of bounds.  (uv index=" + toString(uv_indices[i]) + ")");


	// Check the area of the triangle
/*	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}
*/
	// Push the triangle onto tri array.

	quads.push_back(RayMeshQuad());
	for(unsigned int i = 0; i < 4; ++i)
	{
		quads.back().vertex_indices[i] = vertex_indices[i];
		quads.back().uv_indices[i] = uv_indices[i];
	}

	quads.back().setMatIndex(material_index);
	quads.back().setUseShadingNormals(this->enable_normal_smoothing);
}


void RayMesh::addTriangleUnchecked(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index, bool use_shading_normals)
{
	// Check the area of the triangle
	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	// Push the triangle onto tri array.
	triangles.push_back(RayMeshTriangle());
	for(unsigned int i = 0; i < 3; ++i)
	{
		triangles.back().vertex_indices[i] = vertex_indices[i];
		triangles.back().uv_indices[i] = uv_indices[i];
	}

	triangles.back().setTriMatIndex(material_index);
	triangles.back().setUseShadingNormals(use_shading_normals);
}


//void RayMesh::addUVSetExposition(const std::string& uv_set_name, unsigned int uv_set_index)
//{
//	this->getUVSetNameToIndexMap()[uv_set_name] = uv_set_index;
//}


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
	/*for(std::map<std::string, unsigned int>::const_iterator i=getUVSetNameToIndexMap().begin(); i != getUVSetNameToIndexMap().end(); ++i)
	{
		if((*i).second >= num_uv_sets)
			throw ModelLoadingStreamHandlerExcep("UV set with index " + toString((*i).second) + " was exposed, but the UV set data was not provided.");
	}*/
}


unsigned int RayMesh::getMaterialIndexForTri(unsigned int tri_index) const
{
	assert(tri_index < triangles.size());
	return triangles[tri_index].getTriMatIndex();
}


void RayMesh::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<double>& surface_areas_out) const
{
	surface_areas_out.resize(triangles.size());

	for(size_t i = 0; i < surface_areas_out.size(); ++i)
		surface_areas_out[i] = (double)getTriArea(*this, i, to_parent);

}


void RayMesh::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out) const
{
	//------------------------------------------------------------------------
	//pick point using barycentric coords
	//------------------------------------------------------------------------

	//see siggraph montecarlo course 2003 pg 47
	const Vec3RealType s = std::sqrt(samples.x);
	const Vec3RealType t = samples.y;

	// Compute barycentric coords
	const Vec3RealType u = s * (1.0f - t);
	const Vec3RealType v = s * t;

	hitinfo_out.sub_elem_index = sub_elem_index;
	hitinfo_out.sub_elem_coords.set(u, v);

	//triNormal(sub_elem_index).vectorToVec4f(normal_out);
	//this->triangle_geom_normals[sub_elem_index].vectorToVec4f(normal_out);
	//this->triangles[sub_elem_index].geom_normal.vectorToVec4f(normal_out);
	const RayMeshTriangle& tri(this->triangles[sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
	const Vec3f e0(v1.pos - v0.pos);
	const Vec3f e1(v2.pos - v0.pos);
	//normal_out = crossProduct(e0, e1) * tri.inv_cross_magnitude;
	normal_out.set((e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
				   (e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
				   (e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);


	assert(normal_out.isUnitLength());

	//Vec4f a, b, c;
	//triVertPos(sub_elem_index, 0).pointToVec4f(a);
	//triVertPos(sub_elem_index, 1).pointToVec4f(b);
	//triVertPos(sub_elem_index, 2).pointToVec4f(c);

	//pos_out = 
	//	v0.pos * (1.0f - s) + 
	//	v1.pos * u + 
	//	v2.pos * v;
	pos_out.set(v0.pos.x * (1 - s) + v1.pos.x * u + v2.pos.x * v,
				v0.pos.y * (1 - s) + v1.pos.y * u + v2.pos.y * v,
				v0.pos.z * (1 - s) + v1.pos.z * u + v2.pos.z * v, 1);

	/*pos_out = 
		triVertPos(sub_elem_index, 0) * (1.0f - s) + 
		triVertPos(sub_elem_index, 1) * u + 
		triVertPos(sub_elem_index, 2) * v
		;*/
}


double RayMesh::subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, double sub_elem_area_ws) const
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


void RayMesh::computeShadingNormals(PrintOutput& print_output, bool verbose)
{
	if(verbose) print_output.print("Computing shading normals for mesh '" + this->getName() + "'.");

	for(size_t i = 0; i < vertices.size(); ++i)
		vertices[i].normal = Vec3f(0.f, 0.f, 0.f);

	for(size_t t = 0; t<triangles.size(); ++t)
	{
		const Vec3f tri_normal = triGeometricNormal(
					vertices, 
					triangles[t].vertex_indices[0], 
					triangles[t].vertex_indices[1], 
					triangles[t].vertex_indices[2]
				);

		for(int i = 0; i < 3; ++i)
			vertices[triangles[t].vertex_indices[i]].normal += tri_normal;
	}

	for(size_t q = 0; q < quads.size(); ++q)
	{
		const Vec3f normal = triGeometricNormal(
			vertices, 
			quads[q].vertex_indices[0], 
			quads[q].vertex_indices[1], 
			quads[q].vertex_indices[2]
		);

		for(int i = 0; i < 4; ++i)
			vertices[quads[q].vertex_indices[i]].normal += normal;
	}

	for(size_t i = 0; i < vertices.size(); ++i)
		vertices[i].normal.normalise();
}


// Hash function for RayMeshVertex
class RayMeshVertexHash
{
public:
	inline size_t operator()(const RayMeshVertex& v) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform

		union A
		{
			uint32 i;
			float x;
		};

		A a;
		a.x = v.pos.x + v.pos.y + v.pos.z;

		return (size_t)a.i;
	}
};


//typedef std::map<RayMeshVertex, unsigned int> VertToIndexMap;
typedef std::tr1::unordered_map<RayMeshVertex, unsigned int, RayMeshVertexHash> VertToIndexMap;


void RayMesh::mergeVerticesWithSamePosAndNormal(PrintOutput& print_output, bool verbose)
{
	Timer timer;
	if(verbose)
	{
		print_output.print("Merging vertices for mesh '" + this->getName() + "'...");
		print_output.print("\tInitial num vertices: " + toString((unsigned int)vertices.size()));
	}

	VertToIndexMap new_vert_indices;
	std::vector<RayMeshVertex> newverts;
	newverts.reserve(vertices.size());
	
	for(size_t t = 0; t < triangles.size(); ++t)
	{
		for(unsigned int i = 0; i < 3; ++i)
		{
			const RayMeshVertex& old_vert = vertices[triangles[t].vertex_indices[i]];

			unsigned int new_vert_index;

			const VertToIndexMap::const_iterator result = new_vert_indices.find(old_vert);

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

	for(size_t t = 0; t < quads.size(); ++t)
	{
		for(unsigned int i = 0; i < 4; ++i)
		{
			const RayMeshVertex& old_vert = vertices[quads[t].vertex_indices[i]];

			unsigned int new_vert_index;

			const VertToIndexMap::const_iterator result = new_vert_indices.find(old_vert);

			if(result == new_vert_indices.end())
			{
				new_vert_index = (unsigned int)newverts.size();
				newverts.push_back(old_vert);
				new_vert_indices.insert(std::make_pair(old_vert, new_vert_index));
			}
			else
				new_vert_index = (*result).second;

			quads[t].vertex_indices[i] = new_vert_index;
		}
	}

	vertices = newverts;

	if(verbose)
	{
		print_output.print("\tNew num vertices: " + toString((unsigned int)vertices.size()) + "");
		print_output.print("\tDone.  (Time taken: " + timer.elapsedString());
	}
}


// Hash function for UV
class UVHash
{
public:
	inline size_t operator()(const Vec2f& v) const
	{	// hash _Keyval to size_t value by pseudorandomizing transform

		union A
		{
			uint32 i;
			float x;
		};

		A a;
		a.x = v.x + v.y;

		return (size_t)a.i;
	}
};


typedef std::tr1::unordered_map<Vec2f, unsigned int, UVHash> UVToIndexMap;


void RayMesh::mergeUVs(PrintOutput& print_output, bool verbose)
{
	assert(quads.size() == 0);

	// NOTE: This code only works in the case of num_uv_sets == 1 currently.
	if(num_uv_sets != 1)
		return;

	Timer timer;
	if(verbose)
	{
		print_output.print("Merging uvs for mesh '" + this->getName() + "'...");
		print_output.print("\tInitial num uvs: " + toString((unsigned int)uvs.size()));
	}

	std::vector<Vec2f> new_uvs;

	UVToIndexMap new_uv_indices;

	for(size_t t = 0; t < triangles.size(); ++t)
	{
		for(unsigned int i = 0; i < 3; ++i)
		{
			const Vec2f& old_uv = uvs[triangles[t].uv_indices[i]];

			unsigned int new_index;

			const UVToIndexMap::const_iterator result = new_uv_indices.find(old_uv);

			if(result == new_uv_indices.end())
			{
				new_index = (unsigned int)new_uvs.size();
				new_uvs.push_back(old_uv);
				new_uv_indices.insert(std::make_pair(old_uv, new_index));
			}
			else
				new_index = (*result).second;

			triangles[t].uv_indices[i] = new_index;
		}
	}

	uvs = new_uvs;


	if(verbose)
	{
		print_output.print("\tNew num uvs: " + toString((unsigned int)uvs.size()) + "");
		print_output.print("\tDone.  (Time taken: " + timer.elapsedString());
	}
}


bool RayMesh::isEnvSphereGeometry() const
{
	return false;
}


bool RayMesh::areSubElementsCurved() const
{
	return false;
}


RayMesh::Vec3RealType RayMesh::getBoundingRadius() const
{
	return bounding_radius;
}


const RayMesh::Vec3Type RayMesh::positionForHitInfo(const HitInfo& hitinfo) const
{
	const RayMeshTriangle& tri = triangles[hitinfo.sub_elem_index];

	const Vec3f& v0 = vertPos( tri.vertex_indices[0] );
	const Vec3f& v1 = vertPos( tri.vertex_indices[1] );
	const Vec3f& v2 = vertPos( tri.vertex_indices[2] );

	const Vec3RealType w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	return Vec3Type(
		v0.x * w + v1.x * hitinfo.sub_elem_coords.x + v2.x * hitinfo.sub_elem_coords.y,
		v0.y * w + v1.y * hitinfo.sub_elem_coords.x + v2.y * hitinfo.sub_elem_coords.y,
		v0.z * w + v1.z * hitinfo.sub_elem_coords.x + v2.z * hitinfo.sub_elem_coords.y,
		1.f
	);
}
