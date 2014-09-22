/*=====================================================================
raymesh.cpp
-----------
File created by ClassTemplate on Wed Nov 10 02:56:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "../indigo/EmbreeAccel.h"
#include "../indigo/EmbreeInstance.h"


#include "raymesh.h"


#include "../maths/vec3.h"
#include "../maths/Matrix2.h"
#include "../physics/KDTree.h"
#include "../graphics/image.h"
#include "../graphics/TriBoxIntersection.h"
#include "../simpleraytracer/ModelLoadingStreamHandler.h"
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
#include "../utils/FileUtils.h"
#include "../utils/Timer.h"
#include "../indigo/DisplacementUtils.h"
#include <fstream>
#include <algorithm>
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Numeric.h"
#include "../indigo/PrintOutput.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include <unordered_map>


// #define INDIGO_OPENSUBDIV_SUPPORT 1
#if INDIGO_OPENSUBDIV_SUPPORT
#include <opensubdiv/hbr/mesh.h>
#include <opensubdiv/hbr/subdivision.h>
#include <opensubdiv/hbr/catmark.h>
#include <opensubdiv/far/meshFactory.h>
#include <opensubdiv/osd/cpuDispatcher.h>
#include <opensubdiv/osd/cudaDispatcher.h>
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
Geometry::DistType RayMesh::traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const
{
	return tritree->traceRay(
		ray,
		max_t,
		thread_context,
		hitinfo_out
	);
}


const js::AABBox RayMesh::getAABBoxWS() const
{
	if(tritree)
		return tritree->getAABBoxWS();
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


void RayMesh::getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	tritree->getAllHits(
		ray, // ray 
		thread_context, 
		hitinfos_out
		);
}


/*const RayMesh::Vec3Type RayMesh::getShadingNormal(const HitInfo& hitinfo) const
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
}*/


const RayMesh::Vec3Type RayMesh::getGeometricNormal(const HitInfo& hitinfo) const
{
	assert(built());

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
	const RayMesh::Vec3Type N_g((e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
								(e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
								(e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);

	assert(N_g.isUnitLength());
	return N_g;
}


void RayMesh::getPosAndGeomNormal(const HitInfo& hitinfo, Vec3Type& pos_os_out, Vec3RealType& pos_os_rel_error_out, Vec3Type& N_g_out) const
{
	assert(built());

	const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
	const Vec3f e0(v1.pos - v0.pos);
	const Vec3f e1(v2.pos - v0.pos);
	N_g_out.set(			(e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
							(e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
							(e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);

	assert(N_g_out.isUnitLength());

	// Compute pos_os_out and pos_os_rel_error_out
	const Vec3RealType w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	pos_os_out = Vec3Type(
		v0.pos.x * w + v1.pos.x * hitinfo.sub_elem_coords.x + v2.pos.x * hitinfo.sub_elem_coords.y,
		v0.pos.y * w + v1.pos.y * hitinfo.sub_elem_coords.x + v2.pos.y * hitinfo.sub_elem_coords.y,
		v0.pos.z * w + v1.pos.z * hitinfo.sub_elem_coords.x + v2.pos.z * hitinfo.sub_elem_coords.y,
		1.f
	);

	// Compute absolute error in pos_os.
	float u = hitinfo.sub_elem_coords.x;
	float v = hitinfo.sub_elem_coords.y;
	// NOTE: assume delta_u = delta_v = delta_m.
	//float del_u = std::numeric_limits<float>::epsilon(); // abs value of Relative error in u
	//float del_v = std::numeric_limits<float>::epsilon(); // abs value of Relative error in v
	float del_m = std::numeric_limits<float>::epsilon(); // Machine epsilon
	float v0_norm = Numeric::L1Norm(v0.pos);
	float v1_norm = Numeric::L1Norm(v1.pos);
	float v2_norm = Numeric::L1Norm(v2.pos);

	float pos_os_norm = Numeric::L1Norm(pos_os_out); // pos_os_out.getDist(Vec4f(0,0,0,1));

	// Do the absolute error computation
	float a = v0_norm;
	float b = v1_norm;
	float c = v2_norm;
	//float eps = ((3*a + 4*c)*v + (3*a + 4*b)*u + 4*a)*del_m;  // Sage version
	//float eps = (u*(del_u + 4*del_m) + v*(del_v + 4*del_m) + 3*del_m)*a + (del_u + 3*del_m)*u*b + (del_v + 3*del_m)*v*c; // Version if del_u, del_v != del_m
	// Assuming |del_u| = |del_v| = del_m:
	float eps = ((5*(u + v) + 3)*a + 4*(u*b + v*c)) * del_m;

	// Convert to relative error.
	pos_os_rel_error_out = eps / pos_os_norm;
}


void RayMesh::getInfoForHit(const HitInfo& hitinfo, Vec3Type& N_g_os_out, Vec3Type& N_s_os_out, unsigned int& mat_index_out, Vec3Type& pos_os_out, Real& pos_os_rel_error_out, Vec2f& uv0_out) const
{
	assert(built());

	// NOTE: We'll manually inline the body of getPosAndGeomNormal() here in order to avoid the overhead of the virtual method call, and so we can reuse some variables.


	const float u = hitinfo.sub_elem_coords.x;
	const float v = hitinfo.sub_elem_coords.y;
	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;


	const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);

	// Compute N_g_os_out
	const Vec3f e0(v1.pos - v0.pos);
	const Vec3f e1(v2.pos - v0.pos);
	N_g_os_out.set(			(e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
							(e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
							(e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);

	assert(N_g_os_out.isUnitLength());


	// Compute pos_os_out and pos_os_rel_error_out
	pos_os_out = Vec3Type(
		v0.pos.x * w + v1.pos.x * u + v2.pos.x * v,
		v0.pos.y * w + v1.pos.y * u + v2.pos.y * v,
		v0.pos.z * w + v1.pos.z * u + v2.pos.z * v,
		1.f
	);

	// Compute absolute error in pos_os.
	
	// NOTE: assume delta_u = delta_v = delta_m.
	//float del_u = std::numeric_limits<float>::epsilon(); // abs value of Relative error in u
	//float del_v = std::numeric_limits<float>::epsilon(); // abs value of Relative error in v
	float del_m = std::numeric_limits<float>::epsilon(); // Machine epsilon
	float v0_norm = Numeric::L1Norm(v0.pos);
	float v1_norm = Numeric::L1Norm(v1.pos);
	float v2_norm = Numeric::L1Norm(v2.pos);

	float pos_os_norm = Numeric::L1Norm(pos_os_out); // pos_os_out.getDist(Vec4f(0,0,0,1));

	// Do the absolute error computation
	float a = v0_norm;
	float b = v1_norm;
	float c = v2_norm;
	//float eps = ((3*a + 4*c)*v + (3*a + 4*b)*u + 4*a)*del_m;  // Sage version
	//float eps = (u*(del_u + 4*del_m) + v*(del_v + 4*del_m) + 3*del_m)*a + (del_u + 3*del_m)*u*b + (del_v + 3*del_m)*v*c; // Version if del_u, del_v != del_m
	// Assuming |del_u| = |del_v| = del_m:
	float eps = ((5*(u + v) + 3)*a + 4*(u*b + v*c)) * del_m;

	// Convert to relative error.
	pos_os_rel_error_out = eps / pos_os_norm;


	mat_index_out = tri.getTriMatIndex();

	// Compute shading normal - set N_s_os_out.
	if(tri.getUseShadingNormals() == 0)
	{
		N_s_os_out = N_g_os_out;
	}
	else
	{
		const Vec3f& v0norm = v0.normal;
		const Vec3f& v1norm = v1.normal;
		const Vec3f& v2norm = v2.normal;

		N_s_os_out = Vec3Type(
			v0norm.x * w + v1norm.x * u + v2norm.x * v,
			v0norm.y * w + v1norm.y * u + v2norm.y * v,
			v0norm.z * w + v1norm.z * u + v2norm.z * v,
			0.f
		);
	}

	if(num_uv_sets == 0)
	{
		uv0_out.set(0, 0);
	}
	else
	{
		unsigned int v0idx = tri.uv_indices[0] * num_uv_sets;
		unsigned int v1idx = tri.uv_indices[1] * num_uv_sets;
		unsigned int v2idx = tri.uv_indices[2] * num_uv_sets;

		const Vec2f& v0tex = this->uvs[v0idx];
		const Vec2f& v1tex = this->uvs[v1idx];
		const Vec2f& v2tex = this->uvs[v2idx];

		uv0_out = RayMesh::TexCoordsType(
			v0tex.x * w + v1tex.x * hitinfo.sub_elem_coords.x + v2tex.x * hitinfo.sub_elem_coords.y,
			v0tex.y * w + v1tex.y * hitinfo.sub_elem_coords.x + v2tex.y * hitinfo.sub_elem_coords.y
		);
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


#if INDIGO_OPENSUBDIV_SUPPORT


class RayMeshOsdVertex {
public:
    RayMeshOsdVertex() : pos(0,0,0) {}
    RayMeshOsdVertex(int index) : pos(0,0,0) {}
    RayMeshOsdVertex(const RayMeshOsdVertex &src) : pos(src.pos) {}

	explicit RayMeshOsdVertex(const Vec3f& p) : pos(p) {}

    void AddWithWeight(const RayMeshOsdVertex & i, float weight, void * = 0)
	{
		pos += i.pos * weight;
		//uvs += i.uvs * weight;
	}
    void AddVaryingWithWeight(const RayMeshOsdVertex & i, float weight, void * = 0)
	{
		// pos += i.pos * weight;
	}
    void Clear(void * = 0)
	{
		pos.set(0, 0, 0);
		//uvs.set(0, 0);
	}
    void ApplyVertexEdit(const OpenSubdiv::HbrVertexEdit<RayMeshOsdVertex> & edit)
	{
		const float *src = edit.GetEdit();
        switch(edit.GetOperation()) {
        case OpenSubdiv::HbrHierarchicalEdit<RayMeshOsdVertex>::Set:
            pos.x = src[0];
            pos.y = src[1];
            pos.z = src[2];
            break;
        case OpenSubdiv::HbrHierarchicalEdit<RayMeshOsdVertex>::Add:
            pos.x += src[0];
            pos.y += src[1];
            pos.z += src[2];
            break;
        case OpenSubdiv::HbrHierarchicalEdit<RayMeshOsdVertex>::Subtract:
            pos.x -= src[0];
            pos.y -= src[1];
            pos.z -= src[2];
            break;
        }
	}
    void ApplyMovingVertexEdit(const OpenSubdiv::HbrMovingVertexEdit<RayMeshOsdVertex> &) { }

	Vec3f pos;
};


#endif // #if INDIGO_OPENSUBDIV_SUPPORT


bool RayMesh::subdivideAndDisplace(Indigo::TaskManager& task_manager, ThreadContext& context, const Object& object, const Matrix4f& object_to_camera, double pixel_height_at_dist_one, 
								   //const std::vector<Reference<Material> >& materials, 
	const std::vector<Plane<Vec3RealType> >& camera_clip_planes_os, const std::vector<Plane<Vec3RealType> >& section_planes_os, PrintOutput& print_output, bool verbose
	)
{
	if(subdivide_and_displace_done)
	{
		// Throw exception if we are supposed to do a view-dependent subdivision
		//if(subdivide_pixel_threshold > 0.0)
		if(view_dependent_subdivision && max_num_subdivisions > 0)
			throw GeometryExcep("Tried to do a view-dependent subdivision on instanced mesh '" + object.getGeometry().getName() + "'.");

		// else not an error, but we don't need to subdivide again.
	}
	else
	{
		if(merge_vertices_with_same_pos_and_normal)
			mergeVerticesWithSamePosAndNormal(print_output, verbose);
	
		computeShadingNormalsAndMeanCurvature(
			!vertex_shading_normals_provided, // update shading normals
			print_output, verbose
		);
		bool recompute_H = false; // This will be set to true if we need to recompute H (mean curvature) later.  This will be the case for subdiv and displacement or if there are any quads in the mesh.

		if(object.hasDisplacingMaterial() || max_num_subdivisions > 0)
		{
			if(verbose) print_output.print("Subdividing and displacing mesh '" + this->getName() + "', (max num subdivisions = " + toString(max_num_subdivisions) + ") ...");

			recompute_H = true;
			
#if INDIGO_OPENSUBDIV_SUPPORT
			const bool USE_OSD = true;
			if(USE_OSD)
			{
				// Register Osd compute kernels
				OpenSubdiv::OsdCpuKernelDispatcher::Register();

				//OpenSubdiv::OsdCudaKernelDispatcher::Register();

				// TEMP HACK: remove tris
				this->triangles.resize(0);

				Timer timer;

				// Make uv indices
				std::vector<int> fvar_indices(this->quads.size() * 4);
				for(size_t i=0; i<this->quads.size(); ++i)
				{
					fvar_indices[i * 4 + 0] = this->quads[i].uv_indices[0];
					fvar_indices[i * 4 + 1] = this->quads[i].uv_indices[1];
					fvar_indices[i * 4 + 2] = this->quads[i].uv_indices[2];
					fvar_indices[i * 4 + 3] = this->quads[i].uv_indices[3];
				}

				// printVar(fvar_indices.size());

				int fvar_widths[1] = { (int)fvar_indices.size() };

				OpenSubdiv::v1_1::HbrMesh<RayMeshOsdVertex> hbr_mesh(
					new OpenSubdiv::v1_1::HbrCatmarkSubdivision<RayMeshOsdVertex>(),
					1, // fvarcount
					&fvar_indices[0],
					fvar_widths
				);


				/// Hack for one quad ///
				/*hbr_mesh.NewVertex(0, RayMeshOsdVertex(Vec3f(0,0,0)));
				hbr_mesh.NewVertex(1, RayMeshOsdVertex(Vec3f(1,0,0)));
				hbr_mesh.NewVertex(2, RayMeshOsdVertex(Vec3f(1,1,0)));
				hbr_mesh.NewVertex(3, RayMeshOsdVertex(Vec3f(0,1,0)));

				int verts[4] = {0, 1, 2, 3};

				hbr_mesh.NewFace(
					4,
					verts,
					0 // index
				);*/
				/////////////////////////

				// Add vertices to HBR mesh
				for(size_t i=0; i<vertices.size(); ++i)
					hbr_mesh.NewVertex((int)i, RayMeshOsdVertex(vertices[i].pos));

				// Add quads
				for(size_t i=0; i<quads.size(); ++i)
				{
					int verts[4] = {quads[i].vertex_indices[0], quads[i].vertex_indices[1], quads[i].vertex_indices[2], quads[i].vertex_indices[3]};
					hbr_mesh.NewFace(
						4,
						verts,
						(int)i // index
					);
				}


				hbr_mesh.SetInterpolateBoundaryMethod( OpenSubdiv::v1_1::HbrMesh<RayMeshOsdVertex>::k_InterpolateBoundaryEdgeOnly );

				hbr_mesh.Finish();

				const int subdiv_level = max_num_subdivisions + 1;

				OpenSubdiv::v1_1::FarMeshFactory<RayMeshOsdVertex> factory(&hbr_mesh, subdiv_level);

				OpenSubdiv::v1_1::FarMesh<RayMeshOsdVertex>* far_mesh = factory.Create(
					/*new OpenSubdiv::v1_1::CpuKernelDispatcher(
						subdiv_level, // levels
						8 // num openmp threads
					)*/
				);

				far_mesh->Subdivide(subdiv_level);

				int firstvert = far_mesh->GetSubdivision()->GetFirstVertexOffset(max_num_subdivisions);
				int numverts = far_mesh->GetSubdivision()->GetNumVertices(max_num_subdivisions);

				// Read vertices
				this->vertices.resize(numverts);

				for(int i=0; i<numverts; ++i)
				{
					RayMeshOsdVertex& v = far_mesh->GetVertex(firstvert + i);

					this->vertices[i].pos = v.pos;
				}


				// Read off faces
				const std::vector<int>& face_vertices = far_mesh->GetFaceVertices(max_num_subdivisions);

				this->quads.resize(face_vertices.size() / 4);

				printVar(this->quads.size());
				//printVar(hbr_mesh.GetFVarWidths()[0]);

				for(size_t i=0; i<this->quads.size(); ++i)
				{
					this->quads[i].vertex_indices[0] = face_vertices[(i * 4) + 3] - firstvert;
					this->quads[i].vertex_indices[1] = face_vertices[(i * 4) + 2] - firstvert;
					this->quads[i].vertex_indices[2] = face_vertices[(i * 4) + 1] - firstvert;
					this->quads[i].vertex_indices[3] = face_vertices[(i * 4) + 0] - firstvert;
					this->quads[i].setMatIndex(0); // TEMP HACK
					this->quads[i].setUseShadingNormals(false); // TEMP HACK

					for(int z=0; z<4; ++z)
						this->quads[i].uv_indices[z] = 0; // TEMP HACK
				}

				conPrint("OpenSubdiv took " + timer.elapsedStringNPlaces(4));

				// TEMP NEW:
				computeShadingNormals(print_output, verbose);
			}
			else
			{
#endif // #if INDIGO_OPENSUBDIV_SUPPORT

				// Convert to single precision floating point planes
				/*std::vector<Plane<float> > camera_clip_planes_f(camera_clip_planes.size());
				for(unsigned int i=0; i<camera_clip_planes_f.size(); ++i)
					camera_clip_planes_f[i] = Plane<float>(toVec3f(camera_clip_planes[i].getNormal()), (float)camera_clip_planes[i].getD());*/

				js::Vector<RayMeshTriangle, 32> temp_tris;
				js::Vector<RayMeshVertex, 32> temp_verts;
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
					this->getName(),
					task_manager,
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
					this->enable_normal_smoothing,
					temp_tris,
					temp_verts,
					temp_uvs
					);

				triangles = temp_tris;
				vertices = temp_verts;
				uvs = temp_uvs;

				this->quads.clearAndFreeMem();

				assert(num_uv_sets == 0 || ((temp_uvs.size() % num_uv_sets) == 0));

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

#if INDIGO_OPENSUBDIV_SUPPORT
		}
#endif // #if INDIGO_OPENSUBDIV_SUPPORT
	
		// Convert any quads to tris.
		const size_t num_quads = this->quads.size();
		if(num_quads > 0)
		{
			const size_t initial_num_tris = this->triangles.size();
			this->triangles.resizeUninitialised(initial_num_tris + num_quads * 2);

			for(size_t i=0; i<num_quads; ++i)
			{
				const RayMeshQuad& q = this->quads[i];
				RayMeshTriangle& tri_1 = this->triangles[initial_num_tris + i*2 + 0];
				RayMeshTriangle& tri_2 = this->triangles[initial_num_tris + i*2 + 1];

				tri_1.vertex_indices[0] = q.vertex_indices[0];
				tri_1.vertex_indices[1] = q.vertex_indices[1];
				tri_1.vertex_indices[2] = q.vertex_indices[2];
				tri_1.uv_indices[0] = q.uv_indices[0];
				tri_1.uv_indices[1] = q.uv_indices[1];
				tri_1.uv_indices[2] = q.uv_indices[2];
				tri_1.setRawTriMatIndex(q.getRawMatIndex());

				tri_2.vertex_indices[0] = q.vertex_indices[0];
				tri_2.vertex_indices[1] = q.vertex_indices[2];
				tri_2.vertex_indices[2] = q.vertex_indices[3];
				tri_2.uv_indices[0] = q.uv_indices[0];
				tri_2.uv_indices[1] = q.uv_indices[2];
				tri_2.uv_indices[2] = q.uv_indices[3];
				tri_2.setRawTriMatIndex(q.getRawMatIndex());
			}

			this->quads.clearAndFreeMem();

			recompute_H = true;
		}

		// Recompute surface curvature if required.
		if(recompute_H)
		{
			computeShadingNormalsAndMeanCurvature(
				false, // update shading normals - these should have been computed already.
				print_output, verbose
			);
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
				if(section_planes_os[i].signedDistToPoint( this->vertices[this->triangles[t].vertex_indices[v]].pos ) < 0)
					return true;
			}
		}
	}
	return false;
}


Reference<RayMesh> RayMesh::getClippedCopy(const std::vector<Plane<float> >& section_planes_os) const
{
	Reference<RayMesh> new_mesh(new RayMesh(
		name,
		enable_normal_smoothing,
		max_num_subdivisions,
		subdivide_pixel_threshold, 
		subdivision_smoothing, 
		subdivide_curvature_threshold,
		merge_vertices_with_same_pos_and_normal,
		wrap_u,
		wrap_v,
		view_dependent_subdivision,
		displacement_error_threshold
	));

	// new_mesh->matname_to_index_map = matname_to_index_map;

	// Copy the vertices
	new_mesh->vertices = vertices;

	// Copy the UVs
	new_mesh->num_uv_sets = num_uv_sets;
	new_mesh->uvs = uvs;

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
				d[i] = section_planes_os[p].signedDistToPoint(new_mesh->vertices[current_triangles[t].vertex_indices[i]].pos);

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
				new_vert_1.H =      Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].H,      new_mesh->vertices[current_triangles[t].vertex_indices[v_i1]].H,		t_1);

				// Add it to the vertices list
				const unsigned int new_vert_1_index = (unsigned int)new_mesh->vertices.size();
				new_mesh->vertices.push_back(new_vert_1);

				// Make new UV coordinate pairs
				const unsigned int new_uv_1_index   = (unsigned int)new_mesh->uvs.size();
				for(unsigned int z=0; z<num_uv_sets; ++z)
					new_mesh->uvs.push_back(Maths::uncheckedLerp(
						new_mesh->uvs[current_triangles[t].uv_indices[v_i]  * num_uv_sets + z], 
						new_mesh->uvs[current_triangles[t].uv_indices[v_i1] * num_uv_sets + z], 
						t_1
					));

				// Make a new vertex along the edge (v_i, v_{i+2})
				float t_2 = std::fabs(d[v_i] / (d[v_i] - d[v_i2]));
				assert(t_2 >= 0 && t_2 <= 1);

				// Make the new vertex
				RayMeshVertex new_vert_2;
				new_vert_2.pos =    Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].pos,    new_mesh->vertices[current_triangles[t].vertex_indices[v_i2]].pos,    t_2);
				new_vert_2.normal = Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].normal, new_mesh->vertices[current_triangles[t].vertex_indices[v_i2]].normal, t_2);
				new_vert_2.H =		Maths::uncheckedLerp(new_mesh->vertices[current_triangles[t].vertex_indices[v_i]].H,	  new_mesh->vertices[current_triangles[t].vertex_indices[v_i2]].H,		t_2);

				// Add it to the vertices list
				const unsigned int new_vert_2_index = (unsigned int)new_mesh->vertices.size();
				new_mesh->vertices.push_back(new_vert_2);

				// Make new UV coordinate pairs
				const unsigned int new_uv_2_index   = (unsigned int)new_mesh->uvs.size();
				for(unsigned int z=0; z<num_uv_sets; ++z)
					new_mesh->uvs.push_back(Maths::uncheckedLerp(
						new_mesh->uvs[current_triangles[t].uv_indices[v_i]  * num_uv_sets + z], 
						new_mesh->uvs[current_triangles[t].uv_indices[v_i2] * num_uv_sets + z], 
						t_2
					));

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

	return new_mesh;
}


void RayMesh::build(const std::string& cache_dir_path, const RendererSettings& renderer_settings, PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager)
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
		tri.inv_cross_magnitude = 1.0f / ::crossProduct(v1.pos - v0.pos, v2.pos - v0.pos).length();
	}


	bool have_sse3 = false;
	bool try_spatial = false;
	bool embree_os_ok = EmbreeInstance::isNonNull();
	bool embree_mem_ok = false;
	bool embree_spatial = false;

	if(embree_os_ok && renderer_settings.use_embree) // Do some extra checks if embree accelerator desired
	{
		try
		{
			PlatformUtils::CPUInfo cpu_info;
			PlatformUtils::getCPUInfo(cpu_info);

			have_sse3 = cpu_info.sse3;
		}
		catch(PlatformUtils::PlatformUtilsExcep&)
		{
		}

		if(try_spatial)
		{
			// Estimate memory usage for the embree accelerator and try to allocate it
			size_t embree_spatial_mem = EmbreeAccel::estimateSpatialBuildMemoryUsage(triangles.size());
			//std::cout << "Estimated embree spatial build memory usage: " << (embree_spatial_mem / 1024) << " kb" << std::endl;
			try
			{
				char *big_mem = new char[embree_spatial_mem];
				if(big_mem != NULL)
				{
					delete [] big_mem;
					embree_mem_ok = true; // Flag that we can allocate enough memory,
					embree_spatial = true; //  for the spatial BVH builder
				}
			}
			catch(std::bad_alloc&)
			{
			}
		}

		if(!embree_spatial) // If we failed to allocate enough memory for the spatial BVH, try non-spatial
		{
			size_t embree_mem = EmbreeAccel::estimateBuildMemoryUsage(triangles.size());
			//std::cout << "Estimated embree non-spatial build memory usage: " << (embree_mem / 1024) << " kb" << std::endl;
			try
			{
				char* big_mem = new char[embree_mem];
				delete[] big_mem;
				embree_mem_ok = true; // Flag that we can allocate enough memory,
				embree_spatial = false; // but not for the spatial BVH builder
			}
			catch(std::bad_alloc&)
			{
				print_output.print("Warning: insufficient memory for fast BVH");
			}
		}
	}

	try
	{
		if(embree_os_ok && renderer_settings.use_embree && have_sse3 && embree_mem_ok && triangles.size() < (1 << 26))
		{
			EmbreeAccel *embree_accel = new EmbreeAccel(this, embree_spatial);
			tritree = embree_accel;
		}
		else
		{
			if((int)triangles.size() >= renderer_settings.bih_tri_threshold)
				tritree = new js::BVH(this);
			else
				tritree = new js::KDTree(this);
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
				cache_dir_path, 
				FileUtils::join("tree_cache", toString(tree_checksum) + ".tre")
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
				tritree->build(print_output, verbose, task_manager);
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
					cache_dir_path, 
					FileUtils::join("tree_cache", toString(tritree->checksum()) + ".tre")
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
			tritree->build(print_output, verbose, task_manager);
		}
		catch(js::TreeExcep& e)
		{
			throw GeometryExcep("Exception while building tree: " + e.what());
		}
	}

	if(verbose) print_output.print("Done Building Mesh '" + name + "'. (Time taken: " + timer.elapsedStringNPlaces(3) + ")");
}


unsigned int RayMesh::getNumUVCoordSets() const
{
	return num_uv_sets; 
}


const RayMesh::TexCoordsType RayMesh::getUVCoords(const HitInfo& hitinfo, unsigned int texcoords_set) const
{
	if(texcoords_set >= num_uv_sets)
		return RayMesh::TexCoordsType(0, 0);

	assert(texcoords_set < num_uv_sets);
	assert(hitinfo.sub_elem_index < triangles.size());
	const int uv0idx = triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uv_sets + texcoords_set;
	const int uv1idx = triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uv_sets + texcoords_set;
	const int uv2idx = triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uv_sets + texcoords_set;
	const Vec2f& v0tex = uvs[uv0idx];
	const Vec2f& v1tex = uvs[uv1idx];
	const Vec2f& v2tex = uvs[uv2idx];

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
	dp_dbeta_out  = v2pos - v0pos;

	//if(triangles[hitinfo.sub_elem_index].getUseShadingNormals() != 0)
	//{
	//	const RayMeshTriangle& tri = triangles[hitinfo.sub_elem_index];

	//	Vec4f v0norm;
	//	Vec4f v1norm;
	//	Vec4f v2norm;
	//	vertNormal( tri.vertex_indices[0] ).vectorToVec4f(v0norm);
	//	vertNormal( tri.vertex_indices[1] ).vectorToVec4f(v1norm);
	//	vertNormal( tri.vertex_indices[2] ).vectorToVec4f(v2norm);

	//	dNs_dalpha_out = v1norm - v0norm;
	//	dNs_dbeta_out = v2norm - v0norm;
	//}
	//else
	//{
	//	dNs_dalpha_out = dNs_dbeta_out = Vec3Type(0,0,0,0);
	//}
}


const RayMesh::TexCoordsType RayMesh::getUVCoordsAndPartialDerivs(const HitInfo& hitinfo, unsigned int texcoords_set, 
										TexCoordsRealType& du_dalpha_out, TexCoordsRealType& du_dbeta_out, 
										TexCoordsRealType& dv_dalpha_out, TexCoordsRealType& dv_dbeta_out
									   ) const
{
	if(texcoords_set >= num_uv_sets)
	{
		du_dalpha_out = 1; du_dbeta_out = 0;
		dv_dalpha_out = 0; dv_dbeta_out = 1;
		return RayMesh::TexCoordsType(0, 0);
	}
	assert(texcoords_set < num_uv_sets);

	unsigned int v0idx = triangles[hitinfo.sub_elem_index].uv_indices[0] * num_uv_sets + texcoords_set;
	unsigned int v1idx = triangles[hitinfo.sub_elem_index].uv_indices[1] * num_uv_sets + texcoords_set;
	unsigned int v2idx = triangles[hitinfo.sub_elem_index].uv_indices[2] * num_uv_sets + texcoords_set;

	const Vec2f& v0tex = this->uvs[v0idx];
	const Vec2f& v1tex = this->uvs[v1idx];
	const Vec2f& v2tex = this->uvs[v2idx];

	du_dalpha_out =  v1tex.x - v0tex.x;
	dv_dalpha_out =  v1tex.y - v0tex.y;
	du_dbeta_out  =  v2tex.x - v0tex.x;
	dv_dbeta_out  =  v2tex.y - v0tex.y;

	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;
	return RayMesh::TexCoordsType(
		v0tex.x * w + v1tex.x * hitinfo.sub_elem_coords.x + v2tex.x * hitinfo.sub_elem_coords.y,
		v0tex.y * w + v1tex.y * hitinfo.sub_elem_coords.x + v2tex.y * hitinfo.sub_elem_coords.y
	);
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
inline static float getTriArea(const RayMesh& mesh, unsigned int tri_index, const Matrix4f& to_parent)
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

	const unsigned int num_uv_groups = this->getNumUVGroups(); // Compute out of loop below.

	const RayMesh_ShadingNormals use_shading_normals_enum = this->enable_normal_smoothing ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals;

	// Triangles
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
				throw ModelLoadingStreamHandlerExcep("Triangle vertex index is out of bounds.  (vertex index=" + toString(mesh.triangles[i].vertex_indices[v]) + ", num verts: " + toString(getNumVerts()) + ")");

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
			dest_tri.setUseShadingNormals(use_shading_normals_enum);

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
		this->quads[i].setUseShadingNormals(use_shading_normals_enum);
	}
}


void RayMesh::setMaxNumTexcoordSets(unsigned int max_num_texcoord_sets)
{
	num_uv_sets = myMax(num_uv_sets, max_num_texcoord_sets);
}


void RayMesh::addVertex(const Vec3f& pos)
{
	vertices.push_back(RayMeshVertex(pos, Vec3f(0.f, 0.f, 0.f), 0));
}


void RayMesh::addVertex(const Vec3f& pos, const Vec3f& normal)
{
	vertices.push_back(RayMeshVertex(pos, normal, 0));

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
	const float MIN_TRIANGLE_AREA = 1.0e-20f;
	if(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2])) < MIN_TRIANGLE_AREA)
	{
		//TEMP: conPrint("WARNING: Ignoring degenerate triangle. (triangle area: " + doubleToStringScientific(getTriArea(vertPos(vertex_indices[0]), vertPos(vertex_indices[1]), vertPos(vertex_indices[2]))) + ")");
		return;
	}

	triangles.push_back(RayMeshTriangle());
	RayMeshTriangle& new_tri = triangles.back();
	for(unsigned int i = 0; i < 3; ++i)
	{
		new_tri.vertex_indices[i] = vertex_indices[i];
		new_tri.uv_indices[i]     = uv_indices[i];
	}

	new_tri.setTriMatIndex(material_index);
	new_tri.setUseShadingNormals(this->enable_normal_smoothing ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals);
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

	new_quad.setMatIndex(material_index);
	new_quad.setUseShadingNormals(this->enable_normal_smoothing ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals);
}


void RayMesh::addTriangle(const unsigned int* vertex_indices, const unsigned int* uv_indices, unsigned int material_index, bool use_shading_normals)
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
	RayMeshTriangle& new_tri = triangles.back();
	for(unsigned int i = 0; i < 3; ++i)
	{
		new_tri.vertex_indices[i] = vertex_indices[i];
		new_tri.uv_indices[i]     = uv_indices[i];
	}

	new_tri.setTriMatIndex(material_index);
	new_tri.setUseShadingNormals(use_shading_normals ? RayMesh_UseShadingNormals : RayMesh_NoShadingNormals);
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


void RayMesh::sampleSubElement(unsigned int sub_elem_index, const SamplePair& samples, Pos3Type& pos_out, Vec3Type& normal_out, HitInfo& hitinfo_out, float recip_sub_elem_area_ws, Real& p_out, unsigned int& mat_index_out) const
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

	const RayMeshTriangle& tri(this->triangles[sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);
	const Vec3f e0(v1.pos - v0.pos);
	const Vec3f e1(v2.pos - v0.pos);
	normal_out.set((e0.y * e1.z - e0.z * e1.y) * tri.inv_cross_magnitude,
				   (e0.z * e1.x - e0.x * e1.z) * tri.inv_cross_magnitude,
				   (e0.x * e1.y - e0.y * e1.x) * tri.inv_cross_magnitude, 0);

	pos_out.set(v0.pos.x * (1 - s) + v1.pos.x * u + v2.pos.x * v,
				v0.pos.y * (1 - s) + v1.pos.y * u + v2.pos.y * v,
				v0.pos.z * (1 - s) + v1.pos.z * u + v2.pos.z * v, 1);

	p_out = recip_sub_elem_area_ws;
	mat_index_out = tri.getTriMatIndex();
}


double RayMesh::subElementSamplingPDF(unsigned int sub_elem_index, const Pos3Type& pos, float recip_sub_elem_area_ws) const
{
	return recip_sub_elem_area_ws;
}


// NOTE: results pd will be invalid.
void RayMesh::sampleSurface(const SamplePair& samples, SampleResults& results_out) const
{
	assert(false); // We really should make this not renormalise!

	// Choose triangle
	const int t = myClamp((int)(samples.x * this->triangles.size()), 0, (int)this->triangles.size() - 1);

	SamplePair remapped_samples(samples.x * (float)this->triangles.size() - t, samples.y);

	float p;
	unsigned int mat_index;
	sampleSubElement(t, remapped_samples, results_out.pos, results_out.N_g, results_out.hitinfo, 
		1.0f,  // TEMP HACK
		p,
		mat_index);

	/*sampleSubElement(t, remapped_samples, 
		1.f, // sub_elem_area_ws TEMP HACK - since this is wrong, the pd will be invalid.
		results_out
	);*/
}


void RayMesh::printTreeStats()
{
	tritree->printStats();
}


void RayMesh::printTraceStats()
{
	tritree->printTraceStats();
}


static inline const Vec3f triGeometricNormal(const RayMesh::VertexVectorType& verts, unsigned int v0, unsigned int v1, unsigned int v2)
{
	const Vec3f& p0 = verts[v0].pos, p1 = verts[v1].pos, p2 = verts[v2].pos;
	const Vec3f e0 = p1 - p0;
	const Vec3f e1 = p2 - p0;
	const Vec3f n  = crossProduct(e0, e1);

	return normalise(n);
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


void RayMesh::computeShadingNormalsAndMeanCurvature(bool update_shading_normals, PrintOutput& print_output, bool verbose)
{
	if(verbose) print_output.print("Computing shading normals for mesh '" + this->getName() + "'.");
	Timer timer;

	// See 'Discrete Differential Geometry: An Applied Introduction'
	// http://mesh.brown.edu/3DPGP-2007/pdfs/sg06-course01.pdf
	// Chapter 3, Section 2.3 'Mean Curvature'.
	// We will compute h_p.
	// Note that we currently ignore quads when computing curvature.
	std::vector<Vec3f> H(vertices.size(), Vec3f(0,0,0));
	std::vector<Vec3f> A(vertices.size(), Vec3f(0,0,0));

	if(update_shading_normals)
		for(size_t i = 0; i < vertices.size(); ++i)
			vertices[i].normal = Vec3f(0.f, 0.f, 0.f);

	for(size_t t = 0; t < triangles.size(); ++t)
	{
		const Vec3f tri_normal = triGeometricNormal(
					vertices, 
					triangles[t].vertex_indices[0], 
					triangles[t].vertex_indices[1], 
					triangles[t].vertex_indices[2]
				);

		if(update_shading_normals)
		{
			for(int i = 0; i < 3; ++i)
				vertices[triangles[t].vertex_indices[i]].normal += tri_normal;
		}

		for(int i = 0; i < 3; ++i)
		{
			// Get positions of vertices in triangle
			const Vec3f& v_i_1 = vertices[triangles[t].vertex_indices[mod3(i + 1)]].pos;
			const Vec3f& v_i_2 = vertices[triangles[t].vertex_indices[mod3(i + 2)]].pos;

			const Vec3f e2 = v_i_2 - v_i_1;

			// If tri normal is normalised, then rotation of e2 by 90 degrees in the triangle plane is just n x e2.
			assert(tri_normal.isUnitLength()); 
			H[triangles[t].vertex_indices[i]] += crossProduct(tri_normal, e2);
			A[triangles[t].vertex_indices[i]] += crossProduct(v_i_1, v_i_2);
		}
	}

	for(size_t q = 0; q < quads.size(); ++q)
	{
		if(update_shading_normals)
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
	}

	for(size_t i = 0; i < vertices.size(); ++i)
	{
		if(update_shading_normals)
			vertices[i].normal.normalise();

		float H_p_len_dot_N = -0.25f * dot(H[i], vertices[i].normal);
		float A_p_len = (1.0f / 6.0f) * A[i].length();

		vertices[i].H = H_p_len_dot_N / A_p_len;
	}

	if(verbose) print_output.print("\tElapsed: " + timer.elapsedString());
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
typedef std::unordered_map<RayMeshVertex, unsigned int, RayMeshVertexHash> VertToIndexMap;


void RayMesh::mergeVerticesWithSamePosAndNormal(PrintOutput& print_output, bool verbose)
{
	Timer timer;
	if(verbose)
	{
		print_output.print("Merging vertices for mesh '" + this->getName() + "'...");
		print_output.print("\tInitial num vertices: " + toString((unsigned int)vertices.size()));
	}

	VertToIndexMap new_vert_indices;
	RayMesh::VertexVectorType newverts;
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
		print_output.print("\tDone.  (Time taken: " + timer.elapsedStringNPlaces(3) + ")");
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


typedef std::unordered_map<Vec2f, unsigned int, UVHash> UVToIndexMap;


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
		print_output.print("\tDone.  (Time taken: " + timer.elapsedStringNPlaces(3) + ")");
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


float RayMesh::meanCurvature(const HitInfo& hitinfo) const
{
	assert(built());

	const float u = hitinfo.sub_elem_coords.x;
	const float v = hitinfo.sub_elem_coords.y;
	const float w = 1 - hitinfo.sub_elem_coords.x - hitinfo.sub_elem_coords.y;

	const RayMeshTriangle& tri(this->triangles[hitinfo.sub_elem_index]);
	const RayMeshVertex& v0(vertices[tri.vertex_indices[0]]);
	const RayMeshVertex& v1(vertices[tri.vertex_indices[1]]);
	const RayMeshVertex& v2(vertices[tri.vertex_indices[2]]);

	return v0.H * w + v1.H * u + v2.H * v;
}
