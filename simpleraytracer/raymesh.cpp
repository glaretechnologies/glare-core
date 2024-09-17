/*=====================================================================
raymesh.cpp
-----------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "raymesh.h"


#include "hitinfo.h"
#include "../utils/TestUtils.h"
#include "../graphics/BatchedMesh.h"
#include "../physics/BVH.h"
#include "../physics/SmallBVH.h"
#if IS_INDIGO
#include "../indigo/material.h"
#include "../indigo/DisplacementUtils.h"
#include "../indigo/TransformPath.h"
#endif
#include "../dll/include/IndigoException.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Numeric.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/PrintOutput.h"
#include "../utils/FileUtils.h"
#include "../utils/Timer.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#ifndef NO_EMBREE
#include "../indigo/EmbreeAccel.h"
#endif
#include <fstream>
#include <algorithm>


// Allocates a little extra memory, so that we do 4-vector loads of vertex data without reading out-of-bounds.
class PaddingAllocator : public glare::Allocator
{
	virtual void* alloc(size_t size, size_t alignment)
	{
		void* data = MemAlloc::alignedMalloc(size + 16, alignment);

		std::memset((uint8*)data + size, 0, /*size=*/16); // Zero out padding

		return data;
	}

	virtual void free(void* ptr)
	{
		MemAlloc::alignedFree(ptr);
	}

	virtual std::string getDiagnostics() const { return std::string(); }
};


RayMesh::RayMesh(const std::string& name_, bool enable_shading_normals_, unsigned int max_num_subdivisions_,
				 double subdivide_pixel_threshold_, bool subdivision_smoothing_, double subdivide_curvature_threshold_, 
				bool view_dependent_subdivision_,
				double displacement_error_threshold_
				 )
:	Geometry(
		GeometryType_RayMesh,
		false // sub-elements curved
	),
	name(name_),
	tritree(NULL),
	enable_shading_normals(enable_shading_normals_),
	max_num_subdivisions(max_num_subdivisions_),
	subdivide_pixel_threshold(subdivide_pixel_threshold_),
	subdivide_curvature_threshold(subdivide_curvature_threshold_),
	subdivision_smoothing(subdivision_smoothing_),
	view_dependent_subdivision(view_dependent_subdivision_),
	displacement_error_threshold(displacement_error_threshold_),
	num_smoothings(2),
	planar(false),
	planar_normal(Vec4f(666.f))
{
	subdivide_and_displace_done = false;
	vertex_shading_normals_provided = false;

	num_uv_sets = 0;

	this->vertices.setAllocator(new PaddingAllocator());
	this->uvs     .setAllocator(new PaddingAllocator());
}


RayMesh::~RayMesh()
{	
	delete tritree;
}


const std::string RayMesh::getName() const 
{ 
	return name; 
}


//returns negative number if object not hit by the ray
Geometry::DistType RayMesh::traceRay(const Ray& ray, HitInfo& hitinfo_out) const
{
	return tritree->traceRay(
		ray,
		hitinfo_out
	);
}


RayMesh::DistType RayMesh::traceSphere(const Ray& ray_ws, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_pos_ws_out, Vec4f& hit_normal_ws_out, bool& point_in_tri_out) const
{
	return tritree->traceSphere(
		ray_ws,
		to_object,
		to_world,
		radius_ws,
		hit_pos_ws_out,
		hit_normal_ws_out,
		point_in_tri_out
	);
}


void RayMesh::appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_ws_in_out) const
{
	tritree->appendCollPoints(sphere_pos_ws, radius_ws, to_object, to_world, points_ws_in_out);
}


const js::AABBox RayMesh::getAABBox() const
{
	if(tritree)
		return tritree->getAABBox();
	else
	{
		if(vertices.empty())
			return js::AABBox(Vec4f(0,0,0,1), Vec4f(0,0,0,1));

		js::AABBox box(vertices[0].pos.toVec4fPoint(), vertices[0].pos.toVec4fPoint());
		for(size_t v=0; v<vertices.size(); ++v)
			box.enlargeToHoldPoint(vertices[v].pos.toVec4fPoint());
		return box;
	}
}



const js::AABBox RayMesh::getTightAABBoxWS(const TransformPath& transform_path) const
{
#if IS_INDIGO // Only compile this in Indigo, so we don't have the dependency on TransformPath
	if(transform_path.isDynamicPath())
		return transform_path.worldSpaceAABB(this->getAABBox());
	else
	{
		return transform_path.worldSpaceAABB(this->getAABBox());//TEMP
		
		/*const Matrix4f to_world = transform_path.getStaticToWorldMatrix();

		js::AABBox box_ws = js::AABBox::emptyAABBox();
		const size_t vertices_size = vertices.size();
		for(size_t v=0; v<vertices_size; ++v)
		{
			const Vec4f vpos = loadUnalignedVec4f(&vertices[v].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
			box_ws.enlargeToHoldPoint(to_world.mul3Point(vpos));

			//box_ws.enlargeToHoldPoint(to_world.mul3Point(vertices[v].pos.toVec4fPoint()));
		}

		return box_ws;*/
	}
#else
	assert(0);
	conPrint("ERROR: RayMesh::getTightAABBoxWS() not enabled.");
	return js::AABBox::emptyAABBox();
#endif
}



// Length of returned normal vector should be set to the area of triangle.
const RayMesh::Vec3Type RayMesh::getGeometricNormalAndMatIndex(const HitInfo& hitinfo, unsigned int& mat_index_out) const
{
	assert(built());

	const RayMeshTriangle& tri = this->triangles[hitinfo.sub_elem_index];
	const RayMeshVertex& v0 = vertices[tri.vertex_indices[0]];
	const RayMeshVertex& v1 = vertices[tri.vertex_indices[1]];
	const RayMeshVertex& v2 = vertices[tri.vertex_indices[2]];

	static_assert(offsetof(RayMeshVertex, pos) + sizeof(Vec4f) <= sizeof(RayMeshVertex), "in range");
	const Vec4f v0pos = loadUnalignedVec4f(&v0.pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
	const Vec4f v1pos = loadUnalignedVec4f(&v1.pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&v2.pos.x);

	// Compute N_g_os_out
	const Vec4f e0 = maskWToZero(v1pos - v0pos);
	const Vec4f e1 = maskWToZero(v2pos - v0pos);
	const Vec4f N_g = crossProduct(e0, e1) * 0.5f; // Length should be equal to area of triangle.
	assert(N_g[3] == 0);

	mat_index_out = tri.getTriMatIndex();

	return N_g;
}


void RayMesh::getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_os_out, Vec3RealType& pos_os_abs_error_out, 
	Vec3Type& N_g_os_out) const
{
	assert(built());

	const float u = hitinfo.sub_elem_coords.x;
	const float v = hitinfo.sub_elem_coords.y;
	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;

	const RayMeshTriangle& tri = this->triangles[hitinfo.sub_elem_index];
	const RayMeshVertex& v0 = vertices[tri.vertex_indices[0]];
	const RayMeshVertex& v1 = vertices[tri.vertex_indices[1]];
	const RayMeshVertex& v2 = vertices[tri.vertex_indices[2]];

	static_assert(offsetof(RayMeshVertex, pos) + sizeof(Vec4f) <= sizeof(RayMeshVertex), "in range");
	const Vec4f v0pos = loadUnalignedVec4f(&v0.pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
	const Vec4f v1pos = loadUnalignedVec4f(&v1.pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&v2.pos.x);

	// Compute N_g_os_out
	const Vec4f e0 = maskWToZero(v1pos - v0pos);
	const Vec4f e1 = maskWToZero(v2pos - v0pos);
	N_g_os_out = crossProduct(e0, e1) * tri.inv_cross_magnitude;
	assert(N_g_os_out.isUnitLength() && N_g_os_out[3] == 0);

	// Compute pos_os_out and pos_os_abs_error_out
	pos_os_out = v0pos * w + v1pos * u + v2pos * v;
	pos_os_out[3] = 1.f;

	// Compute absolute error in pos_os.
	
	// NOTE: assume delta_u = delta_v = delta_m.
	const float del_m = 0.5f * std::numeric_limits<float>::epsilon(); // Machine epsilon
	const float a = Numeric::L1Norm(v0pos);
	const float b = Numeric::L1Norm(v1pos);
	const float c = Numeric::L1Norm(v2pos);

	// Do the absolute error computation
	pos_os_abs_error_out = ((5*(u + v) + 3)*a + 4*(u*b + v*c)) * del_m;
}


void RayMesh::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_abs_error_out, Vec2f& uv0_out) const
{
	assert(built());

	// NOTE: We'll manually inline the body of getPosAndGeomNormal() here in order to avoid the overhead of the virtual method call, and so we can reuse some variables.
	
	const float u = hitinfo.sub_elem_coords.x;
	const float v = hitinfo.sub_elem_coords.y;
	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;

	const RayMeshTriangle& tri = this->triangles[hitinfo.sub_elem_index];
	const RayMeshVertex& v0 = vertices[tri.vertex_indices[0]];
	const RayMeshVertex& v1 = vertices[tri.vertex_indices[1]];
	const RayMeshVertex& v2 = vertices[tri.vertex_indices[2]];

	static_assert(offsetof(RayMeshVertex, pos) + sizeof(Vec4f) <= sizeof(RayMeshVertex), "in range");
	const Vec4f v0pos = loadUnalignedVec4f(&v0.pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
	const Vec4f v1pos = loadUnalignedVec4f(&v1.pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&v2.pos.x);

	// Compute N_g_os_out
	const Vec4f e0 = maskWToZero(v1pos - v0pos);
	const Vec4f e1 = maskWToZero(v2pos - v0pos);
	N_g_os_out = crossProduct(e0, e1) * tri.inv_cross_magnitude;
	assert(N_g_os_out.isUnitLength() && N_g_os_out[3] == 0);

	// Compute pos_os_out and pos_os_abs_error_out
	pos_os_out = v0pos * w + v1pos * u + v2pos * v;
	pos_os_out[3] = 1.f;

	// Compute absolute error in pos_os.
	
	// NOTE: assume delta_u = delta_v = delta_m.
	//float del_u = std::numeric_limits<float>::epsilon(); // abs value of Relative error in u
	//float del_v = std::numeric_limits<float>::epsilon(); // abs value of Relative error in v
	const float del_m = 0.5f * std::numeric_limits<float>::epsilon(); // Machine epsilon
	const float a = Numeric::L1Norm(v0pos);
	const float b = Numeric::L1Norm(v1pos);
	const float c = Numeric::L1Norm(v2pos);

	// Do the absolute error computation
	//float eps = ((3*a + 4*c)*v + (3*a + 4*b)*u + 4*a)*del_m;  // Sage version
	//float eps = (u*(del_u + 4*del_m) + v*(del_v + 4*del_m) + 3*del_m)*a + (del_u + 3*del_m)*u*b + (del_v + 3*del_m)*v*c; // Version if del_u, del_v != del_m
	// Assuming |del_u| = |del_v| = del_m:
	pos_os_abs_error_out = ((5*(u + v) + 3)*a + 4*(u*b + v*c)) * del_m;


	mat_index_out = tri.getTriMatIndex();

	// Compute shading normal - set N_s_os_out.
	if(tri.getUseShadingNormals() == 0)
		N_s_os_out = N_g_os_out;
	else
	{
		const Vec4f v0norm = loadUnalignedVec4f(&v0.normal.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
		const Vec4f v1norm = loadUnalignedVec4f(&v1.normal.x);
		const Vec4f v2norm = loadUnalignedVec4f(&v2.normal.x);

		N_s_os_out = maskWToZero(v0norm * w + v1norm * u + v2norm * v);
	}

	// Compute uv0
	if(num_uv_sets == 0)
		uv0_out.set(0, 0);
	else
	{
		const Vec4f uv0 = loadUnalignedVec4f(&uvs[tri.uv_indices[0] * num_uv_sets].x);
		const Vec4f uv1 = loadUnalignedVec4f(&uvs[tri.uv_indices[1] * num_uv_sets].x);
		const Vec4f uv2 = loadUnalignedVec4f(&uvs[tri.uv_indices[2] * num_uv_sets].x);

		const Vec4f uv = uv0 * w + uv1 * u + uv2 * v;

		uv0_out = RayMesh::UVCoordsType(uv[0], uv[1]);
	}
}


/*
static bool isDisplacingMaterial(const std::vector<Reference<Material> >& materials)
{
	for(unsigned int i=0; i<materials.size(); ++i)
		if(materials[i]->displacing())
			return true;
	return false;
}*/


bool RayMesh::subdivideAndDisplace(glare::TaskManager& task_manager, 
	const ArrayRef<Reference<Material> >& materials,
	const Matrix4f& object_to_camera, double pixel_height_at_dist_one, 
	const std::vector<Planef>& camera_clip_planes_os, const std::vector<Planef>& section_planes_os, const WorldParams& world_params,
	PrintOutput& print_output, bool verbose, ShouldCancelCallback* should_cancel_callback
	)
{
#if IS_INDIGO
	if(subdivide_and_displace_done)
	{
		// Throw exception if we are supposed to do a view-dependent subdivision
		//if(subdivide_pixel_threshold > 0.0)
		if(view_dependent_subdivision && max_num_subdivisions > 0)
			throw glare::Exception("Tried to do a view-dependent subdivision on instanced mesh '" + this->getName() + "'.");

		// else not an error, but we don't need to subdivide again.
	}
	else
	{
		computeShadingNormalsAndMeanCurvature(
			task_manager,
			!vertex_shading_normals_provided, // update shading normals - we only want to compute shading normals if they were not provided in the mesh file.
			print_output, verbose
		);

		if(max_num_subdivisions > 0)
		{
			if(verbose) print_output.print("Subdividing and displacing mesh '" + this->getName() + "', (max num subdivisions = " + toString(max_num_subdivisions) + ") ...");

			DUOptions options;
			options.object_to_camera = object_to_camera;
			options.view_dependent_subdivision = view_dependent_subdivision;
			options.pixel_height_at_dist_one = pixel_height_at_dist_one;
			options.subdivide_pixel_threshold = subdivide_pixel_threshold;
			options.subdivide_curvature_threshold = subdivide_curvature_threshold;
			options.displacement_error_threshold = displacement_error_threshold;
			options.max_num_subdivisions = max_num_subdivisions;
			options.num_smoothings = num_smoothings;
			options.camera_clip_planes_os = camera_clip_planes_os;
			options.subdivision_smoothing = subdivision_smoothing;

			const bool subdivided = DisplacementUtils::subdivideAndDisplace(
				this->getName(),
				task_manager,
				print_output,
				materials,
				triangles, // triangles_in_out
				quads,
				vertices, // vertices_in_out
				uvs, // uvs_in_out
				mean_curvature, // mean curvature out
				this->num_uv_sets,
				options,
				this->enable_shading_normals,
				world_params,
				should_cancel_callback
			);

			if(subdivided)
			{
				// All quads have been converted to tris by subdivideAndDisplace(), so we can clear the quads.
				this->quads.clearAndFreeMem();

				assert(num_uv_sets == 0 || ((uvs.size() % num_uv_sets) == 0));

				// Check data
#ifndef NDEBUG
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
			}
			if(verbose) print_output.print("\tDone.");	
		}
		else // else if max_num_subdivisions = 0
		{
			assert(max_num_subdivisions == 0);

			bool has_displacing_mat = false;
			for(size_t i = 0; i < materials.size(); ++i)
				if(materials[i]->displacing())
					has_displacing_mat = true;

			if(has_displacing_mat) // object.hasDisplacingMaterial())
			{
				DisplacementUtils::doDisplacementOnly( // updates vertices.pos
					this->getName(),
					task_manager,
					print_output,
					materials, // object.getMaterials(),
					triangles,
					quads,
					vertices,
					uvs,
					this->num_uv_sets,
					world_params
				);

				// Since we have done displacement, shading normals need to be recomputed.
				computeShadingNormalsAndMeanCurvature(
					task_manager,
					true, // update shading normals
					print_output, verbose
				);
			}
		}

		// Build inv_cross_magnitude for triangles
		{
			const size_t num_tris = this->triangles.size();
			for(size_t i=0; i<num_tris; ++i)
			{
				const RayMeshTriangle& tri = this->triangles[i];
				const Vec4f v0pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[0]].pos.x);
				const Vec4f v1pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[1]].pos.x);
				const Vec4f v2pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[2]].pos.x);
				triangles[i].inv_cross_magnitude = 1.f / ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();
			}
		}
	
		// Convert any quads to tris.
		const size_t num_quads = this->quads.size();
		if(num_quads > 0)
		{
			const size_t initial_num_tris = this->triangles.size();
			this->triangles.resize(initial_num_tris + num_quads * 2);

			for(size_t i=0; i<num_quads; ++i)
			{
				const RayMeshQuad& q = this->quads[i];
				const Vec4f v0pos = loadUnalignedVec4f(&vertices[q.vertex_indices[0]].pos.x);
				const Vec4f v1pos = loadUnalignedVec4f(&vertices[q.vertex_indices[1]].pos.x);
				const Vec4f v2pos = loadUnalignedVec4f(&vertices[q.vertex_indices[2]].pos.x);
				const Vec4f v3pos = loadUnalignedVec4f(&vertices[q.vertex_indices[3]].pos.x);

				RayMeshTriangle& tri_1 = this->triangles[initial_num_tris + i*2 + 0];
				RayMeshTriangle& tri_2 = this->triangles[initial_num_tris + i*2 + 1];

				tri_1.vertex_indices[0] = q.vertex_indices[0];
				tri_1.vertex_indices[1] = q.vertex_indices[1];
				tri_1.vertex_indices[2] = q.vertex_indices[2];
				tri_1.uv_indices[0] = q.uv_indices[0];
				tri_1.uv_indices[1] = q.uv_indices[1];
				tri_1.uv_indices[2] = q.uv_indices[2];
				tri_1.setRawTriMatIndex(q.getRawMatIndex());
				tri_1.inv_cross_magnitude = 1.f / ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();

				tri_2.vertex_indices[0] = q.vertex_indices[0];
				tri_2.vertex_indices[1] = q.vertex_indices[2];
				tri_2.vertex_indices[2] = q.vertex_indices[3];
				tri_2.uv_indices[0] = q.uv_indices[0];
				tri_2.uv_indices[1] = q.uv_indices[2];
				tri_2.uv_indices[2] = q.uv_indices[3];
				tri_2.setRawTriMatIndex(q.getRawMatIndex());
				tri_2.inv_cross_magnitude = 1.f / ::crossProduct(maskWToZero(v2pos - v0pos), maskWToZero(v3pos - v0pos)).length();
			}

			this->quads.clearAndFreeMem();
		}

		subdivide_and_displace_done = true;
	}

	// Decide whether this object may be clipped by the section planes:
	if(section_planes_os.empty())
		return false;

	for(size_t t=0; t<this->triangles.size(); ++t) // For each triangle
	{
		for(int v=0; v<3; ++v) // For each vertex
		{
			for(size_t i=0; i<section_planes_os.size(); ++i) // For each clipping plane
			{
				// If vertex position lies on back-face of section plane, then it will be clipped.
				if(section_planes_os[i].signedDistToPoint( this->vertices[this->triangles[t].vertex_indices[v]].pos.toVec4fPoint() ) < 0)
					return true;
			}
		}
	}
	return false;
#else
	throw glare::Exception("RayMesh::subdivideAndDisplace() disabled, as not Indigo build.");
#endif
}


// Used in cyberspace code
void RayMesh::buildTrisFromQuads()
{
	// Convert any quads to tris.
	const size_t num_quads = this->quads.size();
	if(num_quads > 0)
	{
		const size_t initial_num_tris = this->triangles.size();
		this->triangles.resize(initial_num_tris + num_quads * 2);

		for(size_t i=0; i<num_quads; ++i)
		{
			const RayMeshQuad& q = this->quads[i];
			const Vec4f v0pos = loadUnalignedVec4f(&vertices[q.vertex_indices[0]].pos.x);
			const Vec4f v1pos = loadUnalignedVec4f(&vertices[q.vertex_indices[1]].pos.x);
			const Vec4f v2pos = loadUnalignedVec4f(&vertices[q.vertex_indices[2]].pos.x);
			const Vec4f v3pos = loadUnalignedVec4f(&vertices[q.vertex_indices[3]].pos.x);

			RayMeshTriangle& tri_1 = this->triangles[initial_num_tris + i*2 + 0];
			RayMeshTriangle& tri_2 = this->triangles[initial_num_tris + i*2 + 1];

			tri_1.vertex_indices[0] = q.vertex_indices[0];
			tri_1.vertex_indices[1] = q.vertex_indices[1];
			tri_1.vertex_indices[2] = q.vertex_indices[2];
			tri_1.uv_indices[0] = q.uv_indices[0];
			tri_1.uv_indices[1] = q.uv_indices[1];
			tri_1.uv_indices[2] = q.uv_indices[2];
			tri_1.setRawTriMatIndex(q.getRawMatIndex());
			tri_1.inv_cross_magnitude = 1.f / ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();

			tri_2.vertex_indices[0] = q.vertex_indices[0];
			tri_2.vertex_indices[1] = q.vertex_indices[2];
			tri_2.vertex_indices[2] = q.vertex_indices[3];
			tri_2.uv_indices[0] = q.uv_indices[0];
			tri_2.uv_indices[1] = q.uv_indices[2];
			tri_2.uv_indices[2] = q.uv_indices[3];
			tri_2.setRawTriMatIndex(q.getRawMatIndex());
			tri_2.inv_cross_magnitude = 1.f / ::crossProduct(maskWToZero(v2pos - v0pos), maskWToZero(v3pos - v0pos)).length();
		}

		this->quads.clearAndFreeMem();
	}
}


Reference<RayMesh> RayMesh::getClippedCopy(const std::vector<Planef>& section_planes_os) const
{
	Reference<RayMesh> new_mesh = new RayMesh(
		name,
		enable_shading_normals,
		max_num_subdivisions,
		subdivide_pixel_threshold, 
		subdivision_smoothing, 
		subdivide_curvature_threshold,
		view_dependent_subdivision,
		displacement_error_threshold
	);

	new_mesh->num_smoothings = num_smoothings;

	// Copy the vertices
	new_mesh->vertices = vertices;
	new_mesh->mean_curvature = mean_curvature;

	// Copy the UVs
	new_mesh->num_uv_sets = num_uv_sets;
	new_mesh->uvs = uvs;

	// Copy dp/du, dp/dv
	//new_mesh->vert_derivs = vert_derivs;

	// Should be no quads
	assert(quads.empty());


	RayMesh::TriangleVectorType current_triangles = triangles;
	RayMesh::TriangleVectorType new_triangles;
	
	// For each plane
	for(size_t p=0; p<section_planes_os.size(); ++p)
	{
		new_triangles.resize(0);

		for(size_t t=0; t<current_triangles.size(); ++t)
		{
			float d[3]; // signed distance from plane to vertex i
			for(int i=0; i<3; ++i)
				d[i] = section_planes_os[p].signedDistToPoint(new_mesh->vertices[current_triangles[t].vertex_indices[i]].pos.toVec4fPoint());

			//printVar(d[0]);
			if(d[0] >= 0 && d[1] >= 0 && d[2] >= 0)
			{
				// All tri vertices are on the front side of the plane.
				// Copy over original triangle to output mesh directly.
				new_triangles.push_back(current_triangles[t]);
			}
			else if(d[0] <= 0 && d[1] <= 0 && d[2] <= 0)
			{
				// All tri vertices are on the back side of the plane.
			}
			else
			{
				// Find the single vertex by itself on one side of the clipping plane
				int v_i;
				if(d[0] * d[1] <= 0 && d[0] * d[2] <= 0)
					v_i = 0;
				else if(d[1] * d[0] <= 0 && d[1] * d[2] <= 0)
					v_i = 1;
				else
					v_i = 2;

				int v_i1 = (v_i + 1) % 3;
				int v_i2 = (v_i + 2) % 3;

				// Make a new vertex along the edge (v_i, v_{i+1})
				float t_1 = std::fabs(d[v_i] / (d[v_i] - d[v_i1]));
				assert(t_1 >= 0 && t_1 <= 1);

				// Make the new vertex
				RayMeshVertex new_vert_1;
				new_vert_1.pos =    Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].pos,    new_mesh->vertices[current_triangles[t].vertex_indices[v_i1]].pos,    t_1);
				new_vert_1.normal = Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].normal, new_mesh->vertices[current_triangles[t].vertex_indices[v_i1]].normal, t_1);
				float new_vert_H =  Maths::uncheckedLerp(new_mesh->mean_curvature[current_triangles[t].vertex_indices[v_i]],  new_mesh->mean_curvature[current_triangles[t].vertex_indices[v_i1]],  t_1);

				// Add it to the vertices list
				const unsigned int new_vert_1_index = (unsigned int)new_mesh->vertices.size();
				new_mesh->vertices.push_back(new_vert_1);
				new_mesh->mean_curvature.push_back(new_vert_H);

				// Make new UV coordinate pairs
				const unsigned int new_uv_1_index   = (unsigned int)new_mesh->uvs.size();
				for(unsigned int z=0; z<num_uv_sets; ++z)
					new_mesh->uvs.push_back(Maths::uncheckedLerp(
						new_mesh->uvs[current_triangles[t].uv_indices[v_i]  * num_uv_sets + z], 
						new_mesh->uvs[current_triangles[t].uv_indices[v_i1] * num_uv_sets + z], 
						t_1
					));

				// Add new dp_du etc.. for new vert
				/*{
					VertDerivs d;
					d.dp_du = Maths::uncheckedLerp(new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i]].dp_du, new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i1]].dp_du, t_1);
					d.dp_dv = Maths::uncheckedLerp(new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i]].dp_dv, new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i1]].dp_dv, t_1);
					new_mesh->vert_derivs.push_back(d);
				}*/

				// Make a new vertex along the edge (v_i, v_{i+2})
				float t_2 = std::fabs(d[v_i] / (d[v_i] - d[v_i2]));
				assert(t_2 >= 0 && t_2 <= 1);

				// Make the new vertex
				RayMeshVertex new_vert_2;
				new_vert_2.pos =    Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].pos,    new_mesh->vertices[current_triangles[t].vertex_indices[v_i2]].pos,    t_2);
				new_vert_2.normal = Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].normal, new_mesh->vertices[current_triangles[t].vertex_indices[v_i2]].normal, t_2);
				new_vert_H =		Maths::uncheckedLerp(new_mesh->mean_curvature[current_triangles[t].vertex_indices[v_i]],  new_mesh->mean_curvature[current_triangles[t].vertex_indices[v_i2]],  t_2);

				// Add it to the vertices list
				const unsigned int new_vert_2_index = (unsigned int)new_mesh->vertices.size();
				new_mesh->vertices.push_back(new_vert_2);

				new_mesh->mean_curvature.push_back(new_vert_H);

				// Make new UV coordinate pairs
				const unsigned int new_uv_2_index   = (unsigned int)new_mesh->uvs.size();
				for(unsigned int z=0; z<num_uv_sets; ++z)
					new_mesh->uvs.push_back(Maths::uncheckedLerp(
						new_mesh->uvs[current_triangles[t].uv_indices[v_i]  * num_uv_sets + z], 
						new_mesh->uvs[current_triangles[t].uv_indices[v_i2] * num_uv_sets + z], 
						t_2
					));

				// Add new dp_du etc.. for new vert
				/*{
					VertDerivs d;
					d.dp_du = Maths::uncheckedLerp(new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i]].dp_du, new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i2]].dp_du, t_1);
					d.dp_dv = Maths::uncheckedLerp(new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i]].dp_dv, new_mesh->vert_derivs[current_triangles[t].vertex_indices[v_i2]].dp_dv, t_1);
					new_mesh->vert_derivs.push_back(d);
				}*/

				// Make the triangle (v_i, v_new1, vnew2)
				if(d[v_i] > 0) // If v_i is on front side of plane
				{
					RayMeshTriangle tri = current_triangles[t];
					tri.vertex_indices[0] = current_triangles[t].vertex_indices[v_i];
					tri.vertex_indices[1] = new_vert_1_index;
					tri.vertex_indices[2] = new_vert_2_index;
					tri.uv_indices[0] = current_triangles[t].uv_indices[v_i];
					tri.uv_indices[1] = new_uv_1_index;
					tri.uv_indices[2] = new_uv_2_index;

					new_triangles.push_back(tri);
				}

				
				if(d[v_i] < 0)
				{
					// Make the triangle (v_new1, v_{i+1}, vnew2)
					{
						RayMeshTriangle tri = current_triangles[t];
						tri.vertex_indices[0] = new_vert_1_index;
						tri.vertex_indices[1] = current_triangles[t].vertex_indices[v_i1];
						tri.vertex_indices[2] = new_vert_2_index;
						tri.uv_indices[0] = new_uv_1_index;
						tri.uv_indices[1] = current_triangles[t].uv_indices[v_i1];
						tri.uv_indices[2] = new_uv_2_index;

						new_triangles.push_back(tri);
					}

					// Make the triangle (v_new2, v_{i+1}, v_{i+2})
					{
						RayMeshTriangle tri = current_triangles[t];
						tri.vertex_indices[0] = new_vert_2_index;
						tri.vertex_indices[1] = current_triangles[t].vertex_indices[v_i1];
						tri.vertex_indices[2] = current_triangles[t].vertex_indices[v_i2];
						tri.uv_indices[0] = new_uv_2_index;
						tri.uv_indices[1] = current_triangles[t].uv_indices[v_i1];
						tri.uv_indices[2] = current_triangles[t].uv_indices[v_i2];;

						new_triangles.push_back(tri);
					}
				}
			}
		}

		// Update current triangles
		current_triangles = new_triangles;

	} // End for each plane

	new_mesh->triangles = current_triangles;

	// Build inv_cross_magnitude for triangles.  Needs to be recomputed as triangles may have changed.
	{
		const size_t num_tris = new_mesh->triangles.size();
		for(size_t i=0; i<num_tris; ++i)
		{
			const RayMeshTriangle& tri = new_mesh->triangles[i];
			const Vec4f v0pos = loadUnalignedVec4f(&new_mesh->vertices[tri.vertex_indices[0]].pos.x);
			const Vec4f v1pos = loadUnalignedVec4f(&new_mesh->vertices[tri.vertex_indices[1]].pos.x);
			const Vec4f v2pos = loadUnalignedVec4f(&new_mesh->vertices[tri.vertex_indices[2]].pos.x);
			new_mesh->triangles[i].inv_cross_magnitude = 1.f / ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();
		}
	}

	return new_mesh;
}


Reference<RayMesh> RayMesh::getCopy() const
{
	Reference<RayMesh> new_mesh = new RayMesh(
		name,
		enable_shading_normals,
		max_num_subdivisions,
		subdivide_pixel_threshold,
		subdivision_smoothing,
		subdivide_curvature_threshold,
		view_dependent_subdivision,
		displacement_error_threshold
	);

	new_mesh->vertices = vertices;
	new_mesh->triangles = triangles;
	new_mesh->quads = quads;
	new_mesh->num_uv_sets = num_uv_sets;
	new_mesh->uvs = uvs;
	new_mesh->mean_curvature = mean_curvature;

	new_mesh->num_smoothings = num_smoothings;
	new_mesh->planar = planar;
	new_mesh->planar_normal = planar_normal;

	return new_mesh;
}


static inline const Vec4f triGeometricNormal(const RayMesh::VertexVectorType& verts, unsigned int v0, unsigned int v1, unsigned int v2)
{
	static_assert(offsetof(RayMeshVertex, pos) + sizeof(Vec4f) <= sizeof(RayMeshVertex), "in range");

	const Vec4f v0pos = loadUnalignedVec4f(&verts[v0].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
	const Vec4f v1pos = loadUnalignedVec4f(&verts[v1].pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&verts[v2].pos.x);

	const Vec4f e0 = maskWToZero(v1pos - v0pos); // Mask off garbage
	const Vec4f e1 = maskWToZero(v2pos - v0pos);
	return normalise(crossProduct(e0, e1));
}


bool RayMesh::isPlanar(Vec4f& normal_out) const
{
	assert(tritree); // Assert that build() has been called.
	normal_out = this->planar_normal;
	return this->planar;
}


#ifndef GEOMETRY_NO_TREE_BUILD_SUPPORT
void RayMesh::build(const BuildOptions& options, ShouldCancelCallback& should_cancel_callback, PrintOutput& print_output, bool verbose, glare::TaskManager& task_manager)
{
	Timer timer;

 	if(triangles.size() == 0)
		throw glare::Exception("No triangles in mesh.");

	if(tritree != NULL)
		return; // build() has already been called.

	//NOTE: disabled due to questionable speed-up offered.
	//mergeUVs(print_output, verbose);

	// Compute if this geometry is planar
	// TODO: this could be optimised, we don't need to compute the full normalised normal, and even if we did, we could use the precomputed inv normal len.
	if(options.compute_is_planar)
	{
		this->planar = true;
		const Vec4f normal_0 = triGeometricNormal(vertices, triangles[0].vertex_indices[0], triangles[0].vertex_indices[1], triangles[0].vertex_indices[2]);
		this->planar_normal = normal_0;
		for(size_t i=1; i<triangles.size(); ++i)
		{
			const Vec4f n = triGeometricNormal(vertices, triangles[i].vertex_indices[0], triangles[i].vertex_indices[1], triangles[i].vertex_indices[2]);
			if(!epsEqual(n, normal_0, 1.0e-5f))
			{
				this->planar = false;
				break;
			}
		}
	}
	else
		this->planar = false;
	// conPrint("RayMesh::build, name=" + name + ", planar: " + boolToString(planar));

	// Don't use SmallBVH for now, because that would require us using BVHObjectTree.
	// Instead just use low quality embree build option, which takes a similar amount of time to build,
	// and lets us use the embree scene tracing.
	//if(options.build_small_bvh)
	//{
	//	conPrint("Using SmallBVH");
	//	tritree = new js::SmallBVH(this);
	//}
	//else
	{
#ifdef NO_EMBREE
		// NO_EMBREE is used in substrata
		tritree = new js::BVH(this);
#else
		assert(options.embree_device);
		tritree = new EmbreeAccel(options.embree_device, this, /*do_fast_low_quality_build=*/options.build_small_bvh);
#endif
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

	try
	{
		//Timer timer;
		tritree->build(print_output, should_cancel_callback, task_manager);
		//conPrint("tritree->build: " + timer.elapsedString());
	}
	catch(glare::CancelledException&)
	{
		throw glare::CancelledException();
	}
	catch(glare::Exception& e)
	{
		throw glare::Exception("Exception while building mesh '" + name + "': " + e.what());
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory allocation failure while building mesh '" + name + "'.");
	}

	if(verbose) print_output.print("Done Building Mesh. (Time taken: " + timer.elapsedStringNPlaces(3) + ")");
}
#endif // GEOMETRY_NO_TREE_BUILD_SUPPORT


RTCSceneTy* RayMesh::getEmbreeScene()
{
#ifdef NO_EMBREE
	return NULL;
#else
	if(dynamic_cast<EmbreeAccel*>(this->tritree))
		return static_cast<EmbreeAccel*>(this->tritree)->embree_scene;
	else
		return NULL;
#endif
}


unsigned int RayMesh::getNumUVCoordSets() const
{
	return num_uv_sets; 
}


const RayMesh::UVCoordsType RayMesh::getUVCoords(const HitInfo& hitinfo, unsigned int uv_set_index) const
{
	if(uv_set_index >= num_uv_sets)
		return RayMesh::UVCoordsType(0, 0);

	assert(uv_set_index < num_uv_sets);
	assert(hitinfo.sub_elem_index < triangles.size());
	const unsigned int uv0idx = triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uv_sets + uv_set_index;
	const unsigned int uv1idx = triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uv_sets + uv_set_index;
	const unsigned int uv2idx = triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uv_sets + uv_set_index;

	assert(uv0idx < uvs.size());
	assert(uv1idx < uvs.size());
	assert(uv2idx < uvs.size());
	// Use SSE 4-vectors.  Seems to be a bit faster than scalar code (~3ns instead of ~3.7 for getUVCoords perf test)
	const Vec4f uv0 = loadUnalignedVec4f(&uvs[uv0idx].x);
	const Vec4f uv1 = loadUnalignedVec4f(&uvs[uv1idx].x);
	const Vec4f uv2 = loadUnalignedVec4f(&uvs[uv2idx].x);

	const Vec4f uv = uv0 * (1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y) + uv1 * hitinfo.sub_elem_coords.x + uv2 * hitinfo.sub_elem_coords.y;

	return RayMesh::UVCoordsType(uv[0], uv[1]);
}


const RayMesh::UVCoordsType RayMesh::getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoords_set, Matrix2f& duv_dalphabeta_out) const
{
	if(texcoords_set >= num_uv_sets)
	{
		duv_dalphabeta_out = Matrix2f::identity();
		return RayMesh::UVCoordsType(0, 0);
	}
	assert(texcoords_set < num_uv_sets);

	const unsigned int uv0idx = triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uv_sets + texcoords_set;
	const unsigned int uv1idx = triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uv_sets + texcoords_set;
	const unsigned int uv2idx = triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uv_sets + texcoords_set;

	const Vec4f uv0 = loadUnalignedVec4f(&uvs[uv0idx].x);
	const Vec4f uv1 = loadUnalignedVec4f(&uvs[uv1idx].x);
	const Vec4f uv2 = loadUnalignedVec4f(&uvs[uv2idx].x);

	duv_dalphabeta_out.e[0] = uv1[0] - uv0[0]; // du/dalpha  // TODO: vectorise
	duv_dalphabeta_out.e[1] = uv2[0] - uv0[0]; // du/dbeta
	duv_dalphabeta_out.e[2] = uv1[1] - uv0[1]; // dv/dalpha
	duv_dalphabeta_out.e[3] = uv2[1] - uv0[1]; // dv/dbeta

	const Vec4f uv = uv0 * (1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y) + uv1 * hitinfo.sub_elem_coords.x + uv2 * hitinfo.sub_elem_coords.y;

	return RayMesh::UVCoordsType(uv[0], uv[1]);
}


void RayMesh::getPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_du_out, Vec3Type& dp_dv_out) const
{
	assert(0);


	//const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	//const float alpha = hitinfo.sub_elem_coords.x;
	//const float beta  = hitinfo.sub_elem_coords.y;

	//const unsigned int v0 = triangles[hitinfo.sub_elem_index].vertex_indices[0];
	//const unsigned int v1 = triangles[hitinfo.sub_elem_index].vertex_indices[1];
	//const unsigned int v2 = triangles[hitinfo.sub_elem_index].vertex_indices[2];

	//Vec3f use_dp_du, use_dp_dv;
	///*if(this->num_uv_sets == 0)
	//{
	//	// If there is no UV mapping, then we can't return dp/du and dp/dv.  So just return dp/dalpha and dp/dbeta.
	//	use_dp_du = this->vertices[v1].pos - this->vertices[v0].pos;
	//	use_dp_dv = this->vertices[v2].pos - this->vertices[v0].pos;
	//}
	//else*/
	//{
	//	use_dp_du = vert_derivs[v0].dp_du*w + vert_derivs[v1].dp_du*alpha + vert_derivs[v2].dp_du*beta;
	//	use_dp_dv = vert_derivs[v0].dp_dv*w + vert_derivs[v1].dp_dv*alpha + vert_derivs[v2].dp_dv*beta;
	//}

	//dp_du_out = use_dp_du.toVec4fVector();
	//dp_dv_out = use_dp_dv.toVec4fVector();
}


void RayMesh::getIntrinsicCoordsPartialDerivs(const HitInfo& hitinfo, Vec3Type& dp_dalpha_out, Vec3Type& dp_dbeta_out) const
{
	const RayMeshTriangle& tri = triangles[hitinfo.sub_elem_index];
	const Vec4f v0pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[0]].pos.x);
	const Vec4f v1pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[1]].pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[2]].pos.x);
	dp_dalpha_out = maskWToZero(v1pos - v0pos); // Mask off garbage W values.
	dp_dbeta_out  = maskWToZero(v2pos - v0pos);
}


// This can be optimised quite a bit...
inline static float getTriArea(const RayMesh& mesh, unsigned int tri_index, const Matrix4f& to_parent)
{
	const RayMeshTriangle& tri = mesh.getTriangles()[tri_index];
	const Vec4f v0 = loadUnalignedVec4f(&mesh.getVertices()[tri.vertex_indices[0]].pos.x);
	const Vec4f v1 = loadUnalignedVec4f(&mesh.getVertices()[tri.vertex_indices[1]].pos.x);
	const Vec4f v2 = loadUnalignedVec4f(&mesh.getVertices()[tri.vertex_indices[2]].pos.x);

	const Vec4f e0(to_parent * maskWToZero(v1 - v0)); // Mask off garbage W values.
	const Vec4f e1(to_parent * maskWToZero(v2 - v0));

	return crossProduct(e0, e1).length() * 0.5f;
}


static const float MIN_TRIANGLE_AREA_TIMES_TWO = 2.0e-20f;


// Just copy data needed for phsyics/collision/picking, but not rendering data.
// Also doesn't do bounds checking of vertex indices, assumes are already checked
void RayMesh::fromIndigoMeshForPhysics(const Indigo::Mesh& mesh)
{
	// Copy Vertices
	const size_t num_verts = mesh.vert_positions.size();
	this->vertices.resize(num_verts);

	for(size_t i = 0; i < num_verts; ++i)
	{
		this->vertices[i].pos.set(mesh.vert_positions[i].x, mesh.vert_positions[i].y, mesh.vert_positions[i].z);
		this->vertices[i].normal.set(0.f, 0.f, 0.f);
	}
	vertex_shading_normals_provided = false;

	// Don't copy UVs
	this->num_uv_sets = 0;

	// Copy Triangles

	const size_t mesh_num_tris = mesh.triangles.size();

	this->triangles.resize(mesh_num_tris);

	for(size_t i = 0; i < mesh_num_tris; ++i)
	{
		const Indigo::Triangle& src_tri = mesh.triangles[i];

		const Vec4f v0pos = loadUnalignedVec4f(&vertices[src_tri.vertex_indices[0]].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
		const Vec4f v1pos = loadUnalignedVec4f(&vertices[src_tri.vertex_indices[1]].pos.x);
		const Vec4f v2pos = loadUnalignedVec4f(&vertices[src_tri.vertex_indices[2]].pos.x);
		const float cross_prod_len = ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();

		RayMeshTriangle& dest_tri = this->triangles[i];

		dest_tri.vertex_indices[0] = src_tri.vertex_indices[0];
		dest_tri.vertex_indices[1] = src_tri.vertex_indices[1];
		dest_tri.vertex_indices[2] = src_tri.vertex_indices[2];

		dest_tri.uv_indices[0] = 0;
		dest_tri.uv_indices[1] = 0;
		dest_tri.uv_indices[2] = 0;

		dest_tri.inv_cross_magnitude = 1.f / cross_prod_len;
		dest_tri.setMatIndexAndUseShadingNormals(src_tri.tri_mat_index, RayMesh_ShadingNormals::RayMesh_NoShadingNormals);
	}
}


void RayMesh::fromIndigoMesh(const Indigo::Mesh& mesh)
{
	if(mesh.num_uv_mappings > MAX_NUM_UV_SETS)
		throw glare::Exception("Too many UV sets: " + toString(mesh.num_uv_mappings) + ", max is " + toString(MAX_NUM_UV_SETS));
	this->setMaxNumTexcoordSets(mesh.num_uv_mappings);

	// Copy Vertices
	this->vertices.resize(mesh.vert_positions.size());

	if(mesh.vert_normals.size() == 0)
	{
		for(size_t i = 0; i < mesh.vert_positions.size(); ++i)
		{
			this->vertices[i].pos.set(mesh.vert_positions[i].x, mesh.vert_positions[i].y, mesh.vert_positions[i].z);
			this->vertices[i].normal.set(0.f, 0.f, 0.f);
		}

		vertex_shading_normals_provided = false;
	}
	else
	{
		assert(mesh.vert_normals.size() == mesh.vert_positions.size());

		for(size_t i = 0; i < mesh.vert_positions.size(); ++i)
		{
			this->vertices[i].pos.set(mesh.vert_positions[i].x, mesh.vert_positions[i].y, mesh.vert_positions[i].z);
			this->vertices[i].normal.set(mesh.vert_normals[i].x, mesh.vert_normals[i].y, mesh.vert_normals[i].z);

			assert(::isFinite(mesh.vert_normals[i].x) && ::isFinite(mesh.vert_normals[i].y) && ::isFinite(mesh.vert_normals[i].z));
		}

		vertex_shading_normals_provided = true;
	}

	this->mean_curvature.resize(mesh.vert_positions.size());

	// Copy UVs from Indigo::Mesh
	assert(mesh.num_uv_mappings == 0 || (mesh.uv_pairs.size() % mesh.num_uv_mappings == 0));


	this->uvs.resize(mesh.uv_pairs.size());

	if(mesh.num_uv_mappings > 0) // Need to check this to avoid a divide by zero later.
	{
		if(mesh.uv_layout == Indigo::MESH_UV_LAYOUT_VERTEX_LAYER)
		{
			for(size_t i = 0; i < mesh.uv_pairs.size(); ++i)
				this->uvs[i].set(mesh.uv_pairs[i].x, mesh.uv_pairs[i].y);
		}
		else if(mesh.uv_layout == Indigo::MESH_UV_LAYOUT_LAYER_VERTEX)
		{
			// Copy while re-ordering the UVs
			size_t num_uvs_per_layer = mesh.uv_pairs.size() / mesh.num_uv_mappings;

			for(uint32 layer=0; layer<mesh.num_uv_mappings; ++layer)
				for(size_t uv_i = 0; uv_i < num_uvs_per_layer; ++uv_i)
				{
					const Indigo::Vec2f& src_pair = mesh.uv_pairs[layer * num_uvs_per_layer + uv_i];
					// Write
					this->uvs[uv_i*mesh.num_uv_mappings + layer].set(src_pair.x, src_pair.y);
				}
		}
	}

	// Check all UVs are not NaNs, as NaN UVs cause NaN filtered texture values, which cause a crash in TextureUnit table look-up.  See https://bugs.glaretechnologies.com/issues/271
	const size_t uv_size = this->uvs.size();
	for(size_t i=0; i<uv_size; ++i)
	{
		if(!isFinite(this->uvs[i].x))
			this->uvs[i].x = 0;
		if(!isFinite(this->uvs[i].y))
			this->uvs[i].y = 0;
	}

	const unsigned int num_uv_groups = this->getNumUVsPerSet(); // Compute out of loop below.

	const RayMesh_ShadingNormals use_shading_normals_enum = this->enable_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals;

	// Copy Triangles
	this->triangles.resize(0);
	this->triangles.reserve(mesh.triangles.size());
	unsigned int dest_i = 0;
	for(size_t i = 0; i < mesh.triangles.size(); ++i)
	{
		const Indigo::Triangle& src_tri = mesh.triangles[i];

		// Check material index is in bounds
		//if(src_tri.tri_mat_index >= mesh.used_materials.size())
		//	throw ModelLoadingStreamHandlerExcep("Triangle material_index is out of bounds.  (material index=" + toString(mesh.triangles[i].tri_mat_index) + ")");

		// Check vertex indices are in bounds
		for(unsigned int v = 0; v < 3; ++v)
			if(src_tri.vertex_indices[v] >= getNumVerts())
				throw glare::Exception("Triangle vertex index is out of bounds.  (vertex index=" + toString(mesh.triangles[i].vertex_indices[v]) + ", num verts: " + toString(getNumVerts()) + ")");

		// Check uv indices are in bounds
		if(num_uv_sets > 0)
			for(unsigned int v = 0; v < 3; ++v)
				if(src_tri.uv_indices[v] >= num_uv_groups)
					throw glare::Exception("Triangle uv index is out of bounds.  (uv index=" + toString(mesh.triangles[i].uv_indices[v]) + ")");

		// Check the area of the triangle.
		// If the area is zero, then the geometric normal will be undefined, and it will lead to NaN shading normals.
		const Vec4f v0pos = loadUnalignedVec4f(&vertices[src_tri.vertex_indices[0]].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
		const Vec4f v1pos = loadUnalignedVec4f(&vertices[src_tri.vertex_indices[1]].pos.x);
		const Vec4f v2pos = loadUnalignedVec4f(&vertices[src_tri.vertex_indices[2]].pos.x);
		const float cross_prod_len = ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();

		if(cross_prod_len < MIN_TRIANGLE_AREA_TIMES_TWO)
		{
			// conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
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
		
			dest_tri.inv_cross_magnitude = 1.f / cross_prod_len; 
			dest_tri.setMatIndexAndUseShadingNormals(src_tri.tri_mat_index, use_shading_normals_enum);

			dest_i++;
		}
	}

	// Quads
	this->quads.resize(mesh.quads.size());
	for(size_t i = 0; i < mesh.quads.size(); ++i)
	{
		// Check material index is in bounds
		//if(mesh.quads[i].mat_index >= mesh.used_materials.size())
		//	throw ModelLoadingStreamHandlerExcep("Quad material_index is out of bounds.  (material index=" + toString(mesh.quads[i].mat_index) + ")");

		// Check vertex indices are in bounds
		for(unsigned int v = 0; v < 4; ++v)
			if(mesh.quads[i].vertex_indices[v] >= getNumVerts())
				throw glare::Exception("Quad vertex index is out of bounds.  (vertex index=" + toString(mesh.quads[i].vertex_indices[v]) + ")");

		// Check uv indices are in bounds
		if(num_uv_sets > 0)
			for(unsigned int v = 0; v < 4; ++v)
				if(mesh.quads[i].uv_indices[v] >= num_uv_groups)
					throw glare::Exception("Quad uv index is out of bounds.  (uv index=" + toString(mesh.quads[i].uv_indices[v]) + ")");


		this->quads[i].vertex_indices[0] = mesh.quads[i].vertex_indices[0];
		this->quads[i].vertex_indices[1] = mesh.quads[i].vertex_indices[1];
		this->quads[i].vertex_indices[2] = mesh.quads[i].vertex_indices[2];
		this->quads[i].vertex_indices[3] = mesh.quads[i].vertex_indices[3];

		this->quads[i].uv_indices[0] = mesh.quads[i].uv_indices[0];
		this->quads[i].uv_indices[1] = mesh.quads[i].uv_indices[1];
		this->quads[i].uv_indices[2] = mesh.quads[i].uv_indices[2];
		this->quads[i].uv_indices[3] = mesh.quads[i].uv_indices[3];

		this->quads[i].setMatIndexAndUseShadingNormals(mesh.quads[i].mat_index, use_shading_normals_enum);
	}
}


// For now, don't bother copying normals, uvs.  Just copy vert positions and make triangles.
// This is because this is just used for physics/collision/picking.
void RayMesh::fromBatchedMesh(const BatchedMesh& mesh)
{
	this->setMaxNumTexcoordSets(0);

	const size_t vert_size = mesh.vertexSize();
	const size_t num_verts = mesh.numVerts();

	const BatchedMesh::VertAttribute* pos_attr = mesh.findAttribute(BatchedMesh::VertAttribute_Position);
	if(!pos_attr)
		throw glare::Exception("Pos attribute not present.");
	if(pos_attr->component_type != BatchedMesh::ComponentType_Float)
		throw glare::Exception("Pos attribute must have float type.");
	const size_t pos_offset = pos_attr->offset_B;

	// Copy Vertices
	this->vertices.resize(num_verts);

	const uint8* src_vertex_data = mesh.vertex_data.data();
	for(size_t i = 0; i < num_verts; ++i)
	{
		std::memcpy(&this->vertices[i].pos, src_vertex_data + pos_offset + i * vert_size, sizeof(::Vec3f));

		this->vertices[i].normal.set(0.f, 0.f, 0.f);
	}

	this->vertex_shading_normals_provided = false;
	
	this->uvs.resize(0);

	const RayMesh_ShadingNormals use_shading_normals_enum = this->enable_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals;

	// Copy Triangles

	const BatchedMesh::ComponentType index_type = mesh.index_type;
	const size_t num_indices = mesh.numIndices();
	const size_t num_tris = num_indices / 3;

	const uint8*  const index_data_uint8  = (const uint8* )mesh.index_data.data();
	const uint16* const index_data_uint16 = (const uint16*)mesh.index_data.data();
	const uint32* const index_data_uint32 = (const uint32*)mesh.index_data.data();

	this->triangles.resize(num_tris);
	unsigned int dest_tri_i = 0;
	for(size_t b = 0; b < mesh.batches.size(); ++b)
	{
		const size_t tri_begin = mesh.batches[b].indices_start / 3;
		const size_t tri_end   = tri_begin + mesh.batches[b].num_indices / 3;
		const uint32 mat_index = mesh.batches[b].material_index;

		for(size_t t = tri_begin; t < tri_end; ++t)
		{
			uint32 vertex_indices[3];
			if(index_type == BatchedMesh::ComponentType_UInt8)
			{
				vertex_indices[0] = index_data_uint8[t*3 + 0];
				vertex_indices[1] = index_data_uint8[t*3 + 1];
				vertex_indices[2] = index_data_uint8[t*3 + 2];
			}
			else if(index_type == BatchedMesh::ComponentType_UInt16)
			{
				vertex_indices[0] = index_data_uint16[t*3 + 0];
				vertex_indices[1] = index_data_uint16[t*3 + 1];
				vertex_indices[2] = index_data_uint16[t*3 + 2];
			}
			else if(index_type == BatchedMesh::ComponentType_UInt32)
			{
				vertex_indices[0] = index_data_uint32[t*3 + 0];
				vertex_indices[1] = index_data_uint32[t*3 + 1];
				vertex_indices[2] = index_data_uint32[t*3 + 2];
			}
			else
			{
				throw glare::Exception("Invalid index type.");
			}

			// Check the area of the triangle.
			// If the area is zero, then the geometric normal will be undefined, and it will lead to NaN shading normals.
			const Vec4f v0pos = loadUnalignedVec4f(&vertices[vertex_indices[0]].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
			const Vec4f v1pos = loadUnalignedVec4f(&vertices[vertex_indices[1]].pos.x);
			const Vec4f v2pos = loadUnalignedVec4f(&vertices[vertex_indices[2]].pos.x);
			const float cross_prod_len = ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();

			// Add a triangle
			RayMeshTriangle& dest_tri = this->triangles[dest_tri_i];

			dest_tri.vertex_indices[0] = vertex_indices[0];
			dest_tri.vertex_indices[1] = vertex_indices[1];
			dest_tri.vertex_indices[2] = vertex_indices[2];

			dest_tri.uv_indices[0] = 0;
			dest_tri.uv_indices[1] = 0;
			dest_tri.uv_indices[2] = 0;

			dest_tri.inv_cross_magnitude = 1.f / cross_prod_len;
			dest_tri.setMatIndexAndUseShadingNormals(mat_index, use_shading_normals_enum);

			dest_tri_i++;
		}
	}

	// Trim triangles size down to actual number of added triangles
	assert(dest_tri_i <= this->triangles.size());
	this->triangles.resize(dest_tri_i);
}


const Indigo::MeshRef RayMesh::toIndigoMesh() const
{
	Indigo::MeshRef mesh = new Indigo::Mesh();
	mesh->vert_positions.resize(this->vertices.size());
	if(enable_shading_normals)
		mesh->vert_normals.resize(this->vertices.size());
	mesh->triangles.resize(this->triangles.size());
	mesh->num_uv_mappings = this->num_uv_sets;
	mesh->uv_pairs.resize(this->uvs.size());

	const RayMeshVertex* const src_verts = this->vertices.data();
	Indigo::Vec3f* const dest_verts = mesh->vert_positions.data();
	Indigo::Vec3f* const dest_normals = mesh->vert_normals.data();
	for(size_t i=0; i<this->vertices.size(); ++i)
	{
		dest_verts[i].x = src_verts[i].pos.x;
		dest_verts[i].y = src_verts[i].pos.y;
		dest_verts[i].z = src_verts[i].pos.z;
		if(enable_shading_normals)
		{
			dest_normals[i].x = src_verts[i].normal.x;
			dest_normals[i].y = src_verts[i].normal.y;
			dest_normals[i].z = src_verts[i].normal.z;
		}
	}

	const RayMeshTriangle* const src_tris = this->triangles.data();
	Indigo::Triangle* const dest_tris = mesh->triangles.data();
	for(size_t i=0; i<this->triangles.size(); ++i)
	{
		for(int c=0; c<3; ++c)
		{
			dest_tris[i].vertex_indices[c] = src_tris[i].vertex_indices[c];
			dest_tris[i].uv_indices[c]     = src_tris[i].uv_indices[c];
		}
		dest_tris[i].tri_mat_index     = src_tris[i].getTriMatIndex();
	}

	const Vec2f* const src_uvs = this->uvs.data();
	Indigo::Vec2f* const dest_uvs = mesh->uv_pairs.data();
	for(size_t i=0; i<this->uvs.size(); ++i)
	{
		dest_uvs[i].x = src_uvs[i].x;
		dest_uvs[i].y = src_uvs[i].y;
	}

	return mesh;
}


void RayMesh::saveToIndigoMeshOnDisk(const std::string& path, bool use_compression) const
{
	try
	{
		Indigo::MeshRef mesh = this->toIndigoMesh();

		Indigo::Mesh::writeToFile(toIndigoString(path), *mesh, use_compression);
	}
	catch(Indigo::IndigoException& e)
	{
		throw glare::Exception(toStdString(e.what()));
	}
}


void RayMesh::setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets)
{
	num_uv_sets = myMax(num_uv_sets, max_num_texcoord_sets);
}


void RayMesh::addVertex(const Vec3f& pos)
{
	vertices.push_back(RayMeshVertex(pos, Vec3f(0.f, 0.f, 0.f)));
	mean_curvature.push_back(0.f);
}


void RayMesh::addVertex(const Vec3f& pos, const Vec3f& normal)
{
	vertices.push_back(RayMeshVertex(pos, normal));
	mean_curvature.push_back(0.f);

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
}


void RayMesh::addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index)
{
	// Check the area of the triangle
	const Vec4f v0pos = loadUnalignedVec4f(&vertices[vertex_indices[0]].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
	const Vec4f v1pos = loadUnalignedVec4f(&vertices[vertex_indices[1]].pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&vertices[vertex_indices[2]].pos.x);
	const float cross_prod_len = ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos)).length();

	if(cross_prod_len < MIN_TRIANGLE_AREA_TIMES_TWO)
	{
		// conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	triangles.push_back(RayMeshTriangle());
	RayMeshTriangle& new_tri = triangles.back();
	for(unsigned int i = 0; i < 3; ++i)
	{
		new_tri.vertex_indices[i] = vertex_indices[i];
		new_tri.uv_indices[i]     = uv_indices[i];
	}

	new_tri.setMatIndexAndUseShadingNormals(material_index, this->enable_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals);
	new_tri.inv_cross_magnitude = 1.0f / cross_prod_len;
}


void RayMesh::addQuad(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index)
{
	quads.push_back(RayMeshQuad());
	RayMeshQuad& new_quad = quads.back();
	for(unsigned int i = 0; i < 4; ++i)
	{
		new_quad.vertex_indices[i] = vertex_indices[i];
		new_quad.uv_indices[i]     = uv_indices[i];
	}

	new_quad.setMatIndexAndUseShadingNormals(material_index, this->enable_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals);
}


unsigned int RayMesh::getMaterialIndexForTri(unsigned int tri_index) const
{
	assert(tri_index < triangles.size());
	return triangles[tri_index].getTriMatIndex();
}


void RayMesh::getSubElementSurfaceAreas(const Matrix4f& to_parent, std::vector<float>& surface_areas_out) const
{
	surface_areas_out.resize(triangles.size());

	for(size_t i = 0; i < surface_areas_out.size(); ++i)
		surface_areas_out[i] = getTriArea(*this, (unsigned int)i, to_parent);
}


void RayMesh::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, 
							   unsigned int& mat_index_out, Vec2f& uv0_out) const
{
	//------------------------------------------------------------------------
	//pick point using barycentric coords
	//------------------------------------------------------------------------

	//see siggraph montecarlo course 2003 pg 47
	const Vec3RealType s = std::sqrt(samples.x);
	const Vec3RealType t = samples.y;

	// Compute barycentric coords.  Note that u + v = s(1-t) + st = s(1 - t + t) = s
	const Vec3RealType u = s * (1 - t);
	const Vec3RealType v = s * t;
	const Vec3RealType w = 1 - s; // 1 - u - v

	hitinfo_out.sub_elem_index = sub_elem_index;
	hitinfo_out.sub_elem_coords.set(u, v);

	const RayMeshTriangle& tri = this->triangles[sub_elem_index];
	const Vec4f v0pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[0]].pos.x); // Use unaligned 4-vector load.  Loaded W component will be garbage.
	const Vec4f v1pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[1]].pos.x);
	const Vec4f v2pos = loadUnalignedVec4f(&vertices[tri.vertex_indices[2]].pos.x);
	const Vec4f normal = ::crossProduct(maskWToZero(v1pos - v0pos), maskWToZero(v2pos - v0pos));
	assert(normal[3] == 0 && epsEqual(std::fabs(tri.inv_cross_magnitude), 1.f / normal.length()));

	// length of returned normal vector should be equal to area of triangle.
	normal_out = normal * 0.5f;

	pos_out = setWToOne(v0pos * w + v1pos * u + v2pos * v); // W coord will be garbage, so set it.

	mat_index_out = tri.getTriMatIndex();

	if(num_uv_sets == 0)
	{
		uv0_out.set(0, 0);
	}
	else
	{
		const Vec4f uv0 = loadUnalignedVec4f(&uvs[tri.uv_indices[0] * num_uv_sets].x);
		const Vec4f uv1 = loadUnalignedVec4f(&uvs[tri.uv_indices[1] * num_uv_sets].x);
		const Vec4f uv2 = loadUnalignedVec4f(&uvs[tri.uv_indices[2] * num_uv_sets].x);

		const Vec4f uv = uv0 * w + uv1 * u + uv2 * v;

		uv0_out = RayMesh::UVCoordsType(uv[0], uv[1]);
	}
}


// NOTE: results pd will be invalid.
void RayMesh::sampleSurface(const SamplePair& samples, SampleResults& results_out) const
{
	assert(false); // We really should make this not renormalise!

	// Choose triangle
	const int t = myClamp((int)(samples.x * this->triangles.size()), 0, (int)this->triangles.size() - 1);

	SamplePair remapped_samples(samples.x * (float)this->triangles.size() - t, samples.y);

	unsigned int mat_index;
	Vec2f uv0;
	sampleSubElement(t, remapped_samples, results_out.pos, results_out.N_g, results_out.hitinfo, 
		mat_index, uv0);
}


void RayMesh::printTreeStats()
{
	tritree->printStats();
}


void RayMesh::printTraceStats()
{
	tritree->printTraceStats();
}


static const uint32_t mod3_table[] = { 0, 1, 2, 0, 1, 2 };

inline static uint32 mod3(uint32 x)
{
	return mod3_table[x];
}


inline static uint32 mod4(uint32 x)
{
	return x & 0x3;
}


// Per-tri and quad information computed by ComputePolyInfoTask.
struct PolyTempInfo
{
	Vec3f normal;
	//Vec3f dp_du;
	//Vec3f dp_dv;
};


struct ComputePolyInfoTaskClosure
{
	js::Vector<PolyTempInfo, 64>* poly_info;
	RayMesh::VertexVectorType* vertices;
	RayMesh::TriangleVectorType* triangles;
	RayMesh::QuadVectorType* quads;
	RayMesh::UVVectorType* uvs;
	int num_uv_sets;
	RayMesh::FloatVectorType* mean_curvature;
	//std::vector<RayMesh::VertDerivs>* vert_derivs;
	bool update_shading_normals;
	std::vector<int>* vert_num_polys;
	js::Vector<int, 16>* vert_polys;
	js::Vector<int, 16>* vert_polys_offset;
	size_t total_num_tris;
};


// Computes normal for quads, and normal, dp_du, and dp_dv for triangles.  Stores in closure.poly_info.
class ComputePolyInfoTask : public glare::Task
{
public:
	ComputePolyInfoTask(const ComputePolyInfoTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		js::Vector<PolyTempInfo, 64>& poly_info = *closure.poly_info;
		const RayMesh::VertexVectorType& vertices = *closure.vertices;
		const RayMesh::TriangleVectorType& triangles = *closure.triangles;
		const RayMesh::QuadVectorType& quads = *closure.quads;
		//const std::vector<Vec2f>& uvs = *closure.uvs;
		//const int num_uv_sets = closure.num_uv_sets;
		const size_t total_num_tris = closure.total_num_tris;
		
		for(size_t t = begin; t < end; ++t)
		{
			if(t >= total_num_tris) // If this refers to a quad, not a tri:
			{
				poly_info[t].normal = toVec3f(triGeometricNormal(
					vertices, 
					quads[t - total_num_tris].vertex_indices[0], 
					quads[t - total_num_tris].vertex_indices[1], 
					quads[t - total_num_tris].vertex_indices[2]
				));
			}
			else
			{
				const Vec3f tri_normal = toVec3f(triGeometricNormal(
					vertices, 
					triangles[t].vertex_indices[0], 
					triangles[t].vertex_indices[1], 
					triangles[t].vertex_indices[2]
				));
				/*
				Vec3f dp_du(0.f);
				Vec3f dp_dv(0.f);
				if(num_uv_sets > 0)
				{
					// Compute dp/du, dp/dv for this triangle.
					//const Vec3f N = tri_normal;
					const Vec3f v0 = vertices[triangles[t].vertex_indices[0]].pos;
					const Vec3f v1 = vertices[triangles[t].vertex_indices[1]].pos;
					const Vec3f v2 = vertices[triangles[t].vertex_indices[2]].pos;

					unsigned int v0idx = triangles[t].uv_indices[0] * num_uv_sets;
					unsigned int v1idx = triangles[t].uv_indices[1] * num_uv_sets;
					unsigned int v2idx = triangles[t].uv_indices[2] * num_uv_sets;

					const Vec2f& v0tex = uvs[v0idx];
					const Vec2f& v1tex = uvs[v1idx];
					const Vec2f& v2tex = uvs[v2idx];

					const Vec3f dp_dalpha = v1 - v0;
					const Vec3f dp_dbeta  = v2 - v0;

					// Compute d(alpha, beta) / d(u, v)
					const float du_dalpha =  v1tex.x - v0tex.x;
					const float dv_dalpha =  v1tex.y - v0tex.y;
					const float du_dbeta =  v2tex.x - v0tex.x;
					const float dv_dbeta =  v2tex.y - v0tex.y;

					Matrix2f A(du_dalpha, du_dbeta, dv_dalpha, dv_dbeta);
					if(A.determinant() != 0.f)
					{
						Matrix2f A_inv = A.inverse();

						// Compute dp/du and dp/dv
						// dp/du = dp/dalpha dalpha/du + dp/dbeta dbeta/du
						// dp/dv = dp/dalpha dalpha/dv + dp/dbeta dbeta/dv

						dp_du = dp_dalpha*A_inv.elem(0, 0) + dp_dbeta*A_inv.elem(1, 0);
						dp_dv = dp_dalpha*A_inv.elem(0, 1) + dp_dbeta*A_inv.elem(1, 1); 

						assert(isFinite(dp_du.x));
					}
					else 
					{
						// Else matrix was not invertible - dp/du and dp/dv are not well defined.  Just use dp/dalpha and dp/dbeta for now,
						// which will at least stop material from rendering black.  See https://bugs.glaretechnologies.com/issues/300.
						dp_du = dp_dalpha;
						dp_dv = dp_dbeta;
					}
				}*/

				poly_info[t].normal = tri_normal;
				//poly_info[t].dp_du = dp_du;
				//poly_info[t].dp_dv = dp_dv;
			}
		}
	}

	const ComputePolyInfoTaskClosure& closure;
	size_t begin, end;
};


// Computes shading normal (if update_shading_normals is true), and dp_du, dp_dv and mean curvature (H) for vertices.
class ComputeVertInfoTask : public glare::Task
{
public:
	ComputeVertInfoTask(const ComputePolyInfoTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const js::Vector<PolyTempInfo, 64>& poly_info = *closure.poly_info;
		RayMesh::VertexVectorType& vertices = *closure.vertices;
		RayMesh::FloatVectorType& mean_curvature = *closure.mean_curvature;
		const RayMesh::TriangleVectorType& triangles = *closure.triangles;
		const RayMesh::QuadVectorType& quads = *closure.quads;
		//const int num_uv_sets = closure.num_uv_sets;
		//std::vector<RayMesh::VertDerivs>& vert_derivs = *closure.vert_derivs;
		const int update_shading_normals = closure.update_shading_normals;
		const std::vector<int>& vert_num_polys = *closure.vert_num_polys;
		const js::Vector<int, 16>& vert_polys = *closure.vert_polys;
		const js::Vector<int, 16>& vert_polys_offset = *closure.vert_polys_offset;
		const int total_num_tris = (int)closure.total_num_tris;

		for(size_t v = begin; v < end; ++v)
		{
			const int num_polys = vert_num_polys[v];
			const int offset = vert_polys_offset[v];
			
			Vec3f H(0.f);
			Vec3f A(0.f);
			//Vec3f dp_du(0.f);
			//Vec3f dp_dv(0.f);
			Vec3f n(0.f);
			for(int i=0; i<num_polys; ++i)
			{
				const int p = vert_polys[offset + i];
				if(p < total_num_tris) // Only compute dp/du for tris, not quads
				{
					const RayMeshTriangle& tri = triangles[p];

					int c;
					if(tri.vertex_indices[0] == v)
						c = 0;
					else if(tri.vertex_indices[1] == v)
						c = 1;
					else
						c = 2;
					assert(tri.vertex_indices[c] == v);

					// Get positions of adjacent vertices in triangle
					const Vec3f& v_i_1 = vertices[tri.vertex_indices[mod3(c + 1)]].pos;
					const Vec3f& v_i_2 = vertices[tri.vertex_indices[mod3(c + 2)]].pos;

					const Vec3f e2 = v_i_2 - v_i_1;

					// If tri normal is normalised, then rotation of e2 by 90 degrees in the triangle plane is just n x e2.
					//assert(poly_info[t].normal.isUnitLength()); 
					H += crossProduct(poly_info[p].normal, e2);
					A += crossProduct(v_i_1, v_i_2);

					/*if(num_uv_sets > 0)
					{
						dp_du += poly_info[t].dp_du;
						dp_dv += poly_info[t].dp_dv;
					}*/
				}
				else
				{
					const int q = p - total_num_tris;
					assert(q >= 0 && q < (int)quads.size());
					const RayMeshQuad& quad = quads[q];

					int c;
					if(quad.vertex_indices[0] == v)
						c = 0;
					else if(quad.vertex_indices[1] == v)
						c = 1;
					else if(quad.vertex_indices[2] == v)
						c = 2;
					else
						c = 3;
					assert(quad.vertex_indices[c] == v);

					// Get positions of adjacent vertices in quad
					const Vec3f& v_i_1 = vertices[quad.vertex_indices[mod4(c + 1)]].pos;
					const Vec3f& v_i_2 = vertices[quad.vertex_indices[mod4(c + 3)]].pos;

					const Vec3f e2 = v_i_2 - v_i_1;

					// If tri normal is normalised, then rotation of e2 by 90 degrees in the triangle plane is just n x e2.
					//assert(poly_info[t].normal.isUnitLength()); 
					H += crossProduct(poly_info[p].normal, e2);
					A += crossProduct(v_i_1, v_i_2);
				}

				n += poly_info[p].normal;
			}

			if(update_shading_normals)
				vertices[v].normal = normalise(n);

			float H_p_len_dot_N = -0.25f * dot(H, vertices[v].normal);
			float A_p_len = (1.0f / 6.0f) * A.length();
			mean_curvature[v] = H_p_len_dot_N / A_p_len;

			/*if(num_polys > 0)
			{
				dp_du /= (float)num_polys;
				dp_dv /= (float)num_polys;
			}
			vert_derivs[v].dp_du = dp_du;
			vert_derivs[v].dp_dv = dp_dv;*/
		}
	}

	const ComputePolyInfoTaskClosure& closure;
	size_t begin, end;
};


// Compute shading normals. (if update_shading_normals is true)
// Computes mesh mean curvature.
// Computes dp/du and dp/dv. (used for bump mapping).
// NOTE: If we can work out when bump mapping and mean curvature is needed for each mesh, we can maybe skip calling this method altogether in some cases.
void RayMesh::computeShadingNormalsAndMeanCurvature(glare::TaskManager& task_manager, bool update_shading_normals, PrintOutput& print_output, bool verbose)
{
	if(verbose) print_output.print("Computing shading normals for mesh '" + this->getName() + "'.");
	Timer timer;

	// See 'Discrete Differential Geometry: An Applied Introduction'
	// http://mesh.brown.edu/DGP/pdfs/sg06-course01.pdf
	// Chapter 3, Section 2.3 'Mean Curvature'.
	// We will compute h_p.
	// Note that we currently ignore quads when computing curvature.  This is because all quads should have been converted to triangles before this method is called for the final time for each mesh.

	const size_t vertices_size  = vertices.size();
	const size_t triangles_size = triangles.size();
	const size_t quads_size     = quads.size();

	//Timer timer2;

	// Do a pass to count number of polygons adjacent to each vert.
	std::vector<int> vert_num_polys(vertices_size, 0);
	for(size_t t=0; t<triangles_size; ++t)
		for(int i = 0; i < 3; ++i)
			vert_num_polys[triangles[t].vertex_indices[i]]++;
	for(size_t t=0; t<quads_size; ++t)
		for(int i = 0; i < 4; ++i)
			vert_num_polys[quads[t].vertex_indices[i]]++;

	//conPrint("compute vert_num_tris elapsed: " + timer2.elapsedString());

	// Now do a counting sort, sorting the tri indices for each vertex by vertex index.
	// Compute the prefix sum for vert_num_tris, put it in vert_tris_offset:
	js::Vector<int, 16> vert_polys_offset(vertices_size);
	js::Vector<int, 16> vert_polys_offset_temp(vertices_size); // offsets that will be incremented later.
	int offset = 0;
	for(size_t v=0; v<vertices_size; ++v)
	{
		vert_polys_offset[v] = offset;
		vert_polys_offset_temp[v] = offset;
		offset += vert_num_polys[v];
	}

	// Now iterate over the tris again and put the indices into the correct sorted place.
	js::Vector<int, 16> vert_polys(offset);
	for(size_t t=0; t<triangles_size; ++t)
		for(int i = 0; i < 3; ++i)
		{
			const int vi = triangles[t].vertex_indices[i];
			const int v_offset = vert_polys_offset_temp[vi]++;
			vert_polys[v_offset] = (int)t;
		}
	for(size_t q=0; q<quads_size; ++q)
		for(int i = 0; i < 4; ++i)
		{
			const int vi = quads[q].vertex_indices[i];
			const int v_offset = vert_polys_offset_temp[vi]++;
			vert_polys[v_offset] = (int)(q + triangles_size);
		}
	//conPrint("timer 2 elapsed: " + timer2.elapsedString());

	//vert_derivs.resize(vertices_size);

	js::Vector<PolyTempInfo, 64> poly_info(triangles_size + quads_size);

	ComputePolyInfoTaskClosure closure;
	closure.poly_info = &poly_info;
	closure.vertices = &vertices;
	closure.triangles = &triangles;
	closure.quads = &quads;
	closure.uvs = &uvs;
	closure.num_uv_sets = num_uv_sets;
	closure.mean_curvature = &mean_curvature;
	//closure.vert_derivs = &vert_derivs;
	closure.update_shading_normals = update_shading_normals;
	closure.vert_num_polys = &vert_num_polys;
	closure.vert_polys = &vert_polys;
	closure.vert_polys_offset = &vert_polys_offset;
	closure.total_num_tris = triangles_size;
	
	// Compute poly_info
	task_manager.runParallelForTasks<ComputePolyInfoTask, ComputePolyInfoTaskClosure>(closure, 0, triangles_size + quads_size);

	// Compute vertex information
	task_manager.runParallelForTasks<ComputeVertInfoTask, ComputePolyInfoTaskClosure>(closure, 0, vertices_size);

	if(verbose) print_output.print("\tElapsed: " + timer.elapsedStringNPlaces(3));
}


float RayMesh::meanCurvature(const HitInfo& hitinfo) const
{
	assert(built());

	const float u = hitinfo.sub_elem_coords.x;
	const float v = hitinfo.sub_elem_coords.y;
	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;

	const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
	const float H_0 = mean_curvature[tri.vertex_indices[0]];
	const float H_1 = mean_curvature[tri.vertex_indices[1]];
	const float H_2 = mean_curvature[tri.vertex_indices[2]];

	return H_0 * w + H_1 * u + H_2 * v;
}


size_t RayMesh::getTotalMemUsage() const
{
	const size_t vert_mem = vertices.dataSizeBytes();
	const size_t tri_mem = triangles.dataSizeBytes();
	const size_t quad_mem = quads.dataSizeBytes();
	const size_t uv_mem = uvs.size()*sizeof(Vec2f);
	//const size_t vertderiv_mem = vert_derivs.size()*sizeof(VertDerivs);
	const size_t accel_mem = tritree->getTotalMemUsage();

	/*conPrint("---- RayMesh " + name + " ----");
	const int w = 7;
	conPrint("verts:          " + rightSpacePad(toString(vertices.size()), w)    + "(" + getNiceByteSize(vert_mem) + ")");
	conPrint("tri_mem:        " + rightSpacePad(toString(triangles.size()), w)   + "(" + getNiceByteSize(tri_mem) + ")");
	conPrint("quad_mem:       " + rightSpacePad(toString(quads.size()), w)       + "(" + getNiceByteSize(quad_mem) + ")");
	conPrint("uv_mem:         " + rightSpacePad(toString(uvs.size()), w)         + "(" + getNiceByteSize(uv_mem) + ")");
	//conPrint("vertderiv_mem:  " + rightSpacePad(toString(vert_derivs.size()), w) + "(" + getNiceByteSize(vertderiv_mem) + ")");
	conPrint("accel_mem:      " + rightSpacePad(toString(1), w)                  + "(" + getNiceByteSize(accel_mem) + ")");
	*/
	return vert_mem + tri_mem + quad_mem + uv_mem + /*vertderiv_mem +*/ accel_mem;
}
