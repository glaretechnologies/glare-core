/*=====================================================================
BVHObjectTreeTests.cpp
----------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "BVHObjectTreeTests.h"


#if BUILD_TESTS


#include "BVHObjectTree.h"
#include "../simpleraytracer/ray.h"
#include "../utils/TestUtils.h"
#include "../indigo/RendererSettings.h"
#include "../indigo/object.h"
#include "../utils/TaskManager.h"
#include "../utils/ShouldCancelCallback.h"
#include "../indigo/Diffuse.h"
#include "../indigo/DisplaceMatParameter.h"
#include "../indigo/SpectrumMatParameter.h"
#include "../indigo/PathTracerTests.h"
#include "../utils/StandardPrintOutput.h"


void BVHObjectTreeTests::test()
{
	// Disabled while we are doing Embree object tree tracing, since we don't have tracing for individual obs enabled currently.
#if 0
	{
		StandardPrintOutput print_output;
		glare::TaskManager task_manager;
		DummyShouldCancelCallback should_cancel_callback;

		Reference<Material> mat = PathTracerTests::createSimpleDiffuseMatWithAlbedo(0.6f);


		// Make a cube mesh
		Reference<RayMesh> raymesh = new RayMesh("quad", false);
		const unsigned int uv_indices[] = {0, 0, 0};

		// x=0 face
		unsigned int v_start = 0;
		{
			raymesh->addVertex(Vec3f(0,0,0));
			raymesh->addVertex(Vec3f(0,1,0));
			raymesh->addVertex(Vec3f(0,1,1));
			raymesh->addVertex(Vec3f(0,0,1));
			const unsigned int vertex_indices[]   = {v_start + 0, v_start + 1, v_start + 2};
			raymesh->addTriangle(vertex_indices, uv_indices, 0);
			const unsigned int vertex_indices_2[] = {v_start + 0, v_start + 2, v_start + 3};
			raymesh->addTriangle(vertex_indices_2, uv_indices, 0);
			v_start += 4;
		}
		// x=1 face
		{
			raymesh->addVertex(Vec3f(1,0,0));
			raymesh->addVertex(Vec3f(1,1,0));
			raymesh->addVertex(Vec3f(1,1,1));
			raymesh->addVertex(Vec3f(1,0,1));
			const unsigned int vertex_indices[]   = {v_start + 0, v_start + 1, v_start + 2};
			raymesh->addTriangle(vertex_indices, uv_indices, 0);
			const unsigned int vertex_indices_2[] = {v_start + 0, v_start + 2, v_start + 3};
			raymesh->addTriangle(vertex_indices_2, uv_indices, 0);
			v_start += 4;
		}
		// y=0 face
		{
			raymesh->addVertex(Vec3f(0,0,0));
			raymesh->addVertex(Vec3f(1,0,0));
			raymesh->addVertex(Vec3f(1,0,1));
			raymesh->addVertex(Vec3f(0,0,1));
			const unsigned int vertex_indices[]   = {v_start + 0, v_start + 1, v_start + 2};
			raymesh->addTriangle(vertex_indices, uv_indices, 0);
			const unsigned int vertex_indices_2[] = {v_start + 0, v_start + 2, v_start + 3};
			raymesh->addTriangle(vertex_indices_2, uv_indices, 0);
			v_start += 4;
		}
		// y=1 face
		{
			raymesh->addVertex(Vec3f(0,1,0));
			raymesh->addVertex(Vec3f(1,1,0));
			raymesh->addVertex(Vec3f(1,1,1));
			raymesh->addVertex(Vec3f(0,1,1));
			const unsigned int vertex_indices[]   = {v_start + 0, v_start + 1, v_start + 2};
			raymesh->addTriangle(vertex_indices, uv_indices, 0);
			const unsigned int vertex_indices_2[] = {v_start + 0, v_start + 2, v_start + 3};
			raymesh->addTriangle(vertex_indices_2, uv_indices, 0);
			v_start += 4;
		}
		// z=0 face
		{
			raymesh->addVertex(Vec3f(0,0,0));
			raymesh->addVertex(Vec3f(1,0,0));
			raymesh->addVertex(Vec3f(1,1,0));
			raymesh->addVertex(Vec3f(0,1,0));
			const unsigned int vertex_indices[]   = {v_start + 0, v_start + 1, v_start + 2};
			raymesh->addTriangle(vertex_indices, uv_indices, 0);
			const unsigned int vertex_indices_2[] = {v_start + 0, v_start + 2, v_start + 3};
			raymesh->addTriangle(vertex_indices_2, uv_indices, 0);
			v_start += 4;
		}
		// z=1 face
		{
			raymesh->addVertex(Vec3f(0,0,1));
			raymesh->addVertex(Vec3f(1,0,1));
			raymesh->addVertex(Vec3f(1,1,1));
			raymesh->addVertex(Vec3f(0,1,1));
			const unsigned int vertex_indices[]   = {v_start + 0, v_start + 1, v_start + 2};
			raymesh->addTriangle(vertex_indices, uv_indices, 0);
			const unsigned int vertex_indices_2[] = {v_start + 0, v_start + 2, v_start + 3};
			raymesh->addTriangle(vertex_indices_2, uv_indices, 0);
			v_start += 4;
		}
		

		BVHObjectTree bvh_ob_tree;

		RendererSettings settings;
		ObjectRef ob1;
		ObjectRef ob2;
		{
			ob1 = new Object(
				raymesh,
				js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, /*offset=*/Vec4f(0,0,0,0.f), Quatf::identity())),
				Object::Matrix3Type::identity(),
				std::vector<Reference<Material> >(1, mat),
				std::vector<EmitterScale>(1),
				std::vector<const IESDatum*>(1, (const IESDatum*)NULL)
			);
			
			ob1->buildGeometry(NULL, settings, should_cancel_callback, print_output, /*verbose=*/true, task_manager);

			bvh_ob_tree.objects.push_back(ob1.ptr());
		}

		{
			ob2 = new Object(
				raymesh,
				js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, /*offset=*/Vec4f(2.0f, 1.f,0,0.f), Quatf::identity())),
				Object::Matrix3Type::identity(),
				std::vector<Reference<Material> >(1, mat),
				std::vector<EmitterScale>(1),
				std::vector<const IESDatum*>(1, (const IESDatum*)NULL)
			);

			ob2->buildGeometry(NULL, settings, should_cancel_callback, print_output, /*verbose=*/true, task_manager);

			bvh_ob_tree.objects.push_back(ob2.ptr());
		}


		bvh_ob_tree.build(task_manager, should_cancel_callback, print_output, /*verbose=*/true);

		{
			const Ray ray(
				Vec4f(-0.5f, 0.4f, 0.1f, 1),
				normalise(Vec4f(1, 0.3f, 0, 0)),
				0.0f, // min_t
				4.f, // max_t
				false // shadow ray
			);

			const Object* hitob;
			HitInfo hitinfo;
			const float hitdist = bvh_ob_tree.traceRay(ray, 0.0, hitob, hitinfo);
			testAssert(hitob == ob1.ptr());
			testEpsEqual(hitdist, 0.522015333f);
		}

		// Test a trace with max distance before ob2.
		// We should not intersect the right child node.
		{
			const Ray ray(
				Vec4f(-0.5f, 0.4f, 0.1f, 1),
				normalise(Vec4f(1, 0.3f, 0, 0)),
				0.0f, // min_t
				2.f, // max_t
				false // shadow ray
			);

			const Object* hitob;
			HitInfo hitinfo;
			const float hitdist = bvh_ob_tree.traceRay(ray, 0.0, hitob, hitinfo);
			testAssert(hitob == ob1.ptr());
			testEpsEqual(hitdist, 0.522015333f);
		}

		// Test a ray going down to the left (in the neg x, neg y direction)
		{
			const Ray ray(
				Vec4f(3.5f, 1.5f, 0.1f, 1), // startpos
				-normalise(Vec4f(1, 0.3f, 0, 0)),
				0.0f, // min_t
				4.f, // max_t
				false // shadow ray
			);

			const Object* hitob;
			HitInfo hitinfo;
			const float hitdist = bvh_ob_tree.traceRay(ray, 0.0, hitob, hitinfo);
			testAssert(hitob == ob2.ptr());
			testEpsEqual(hitdist, 0.522015333f);
		}

		// Test a ray that starts inside the overall AABB and traces outwards
		{
			const Ray ray(
				Vec4f(1.5f, 1.f, 0.1f, 1),
				normalise(Vec4f(1, 0.3f, 0, 0)),
				0.0f, // min_t
				4.f, // max_t
				false // shadow ray
			);

			const Object* hitob;
			HitInfo hitinfo;
			const float hitdist = bvh_ob_tree.traceRay(ray, 0.0, hitob, hitinfo);
			testAssert(hitob == ob2.ptr());
			testEpsEqual(hitdist, 0.522015333f);
		}
	}
#endif
}

#endif // BUILD_TESTS
