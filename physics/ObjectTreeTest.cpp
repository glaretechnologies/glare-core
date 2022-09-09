/*=====================================================================
ObjectTreeTest.cpp
------------------
File created by ClassTemplate on Sat Apr 22 20:17:04 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "ObjectTreeTest.h"


#include "../indigo/hitinfo.h"
#include "../indigo/Vec3MatParameter.h"
#include "../simpleraytracer/raysphere.h"
#include "../maths/PCG32.h"
#include "../utils/TestUtils.h"
#include "../indigo/RendererSettings.h"
#include "../utils/Timer.h"
#include "../utils/TaskManager.h"
#include "../simpleraytracer/raymesh.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../dll/include/IndigoMesh.h"
#include "../indigo/Diffuse.h"
#include "../indigo/DisplaceMatParameter.h"
#include "../indigo/SpectrumMatParameter.h"
#include "../indigo/MeshLoader.h"
#include "../graphics/Map2D.h"
#include "../utils/StandardPrintOutput.h"


// Disabled due to removal of ObjectTree.  TODO: port tests over to test BVHObjectTree?

void js::ObjectTreeTest::doTests()
{}

#if 0


namespace js
{


#if (BUILD_TESTS)
void ObjectTreeTest::doSelfIntersectionAvoidanceTest()
{
	conPrint("ObjectTreeTest::doSelfIntersectionAvoidanceTest()");

	PCG32 rng(1);

	ObjectTree ob_tree;

	StandardPrintOutput print_output;
	glare::TaskManager task_manager;


	// Create basic material
	Reference<Material> mat(new Diffuse(
		Reference<SpectrumMatParameter>(NULL), // albedo spectrum
		Reference<DisplaceMatParameter>(NULL),
		Reference<DisplaceMatParameter>(NULL),
		Reference<SpectrumMatParameter>(NULL), // base emission
		Reference<SpectrumMatParameter>(NULL),
		Reference<Vec3MatParameter>(), // normal map param
		0,
		false,
		Material::MaterialArgs()
	));


	RayMesh* raymesh = new RayMesh("quad", false);
	const unsigned int uv_indices[] = {0, 0, 0};

	{
	raymesh->addVertex(Vec3f(0,0,0));
	raymesh->addVertex(Vec3f(0,1,0));
	raymesh->addVertex(Vec3f(0,1,1));
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh->addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	raymesh->addVertex(Vec3f(0,0,0));
	raymesh->addVertex(Vec3f(0,1,1));
	raymesh->addVertex(Vec3f(0,0,1));
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh->addTriangle(vertex_indices, uv_indices, 0);
	}

	Reference<Geometry> raymeshref(raymesh);

	RendererSettings settings;
	Object* ob1;
	Object* ob2;
	{
		ob1 = new Object(
			raymeshref,
			js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, Vec4f(0,0,0,1.f), Quatf::identity())),
			Object::Matrix3Type::identity(),
			std::vector<Reference<Material> >(1, mat),
			std::vector<EmitterScale>(1),
			std::vector<const IESDatum*>(1, (const IESDatum*)NULL)
			);

		
		ob1->buildGeometry(settings, print_output, true, task_manager);
		ob1->setObjectIndex(0);
		ob_tree.insertObject(ob1);
	}

	{
		ob2 = new Object(
			raymeshref,
			js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, Vec4f(1.0f,0,0,1.f), Quatf::identity())),
			Object::Matrix3Type::identity(),
			std::vector<Reference<Material> >(1, mat),
			std::vector<EmitterScale>(1),
			std::vector<const IESDatum*>(1, (const IESDatum*)NULL)
			);

		ob2->buildGeometry(settings, print_output, true, task_manager);
		ob2->setObjectIndex(1);
		ob_tree.insertObject(ob2);
	}

	ob_tree.build(task_manager, print_output, 
		false // verbose
	);

	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	// Start a ray on one quad, trace to the other quad.
	{
		Ray ray(
			Vec4f(0.0f, 0.25f, 0.1f, 1.0f), // start position
			Vec4f(1.0f, 0.0f, 0.0f, 0.0f), // dir
			1.0e-5f, // min_t
			std::numeric_limits<float>::max(), // max_t
			false // shadow ray
		);

		{
			HitInfo hitinfo;
			const Object* hitob;
			const ObjectTree::Real dist = ob_tree.traceRay(ray, 
				std::numeric_limits<float>::infinity(), // ray length
				0.0, hitob, hitinfo);

			testAssert(::epsEqual(dist, 1.0f));
			testAssert(hitob == ob2);
			testAssert(hitinfo.sub_elem_index == 0);
		}

		//const float nudge = 0.0001f;

		// Trace the same ray, but with a max distance of 1.0 - nudge.
		// So the ray shouldn't hit the other quad.
		//{
		//	HitInfo hitinfo;
		//	const bool hit = ob_tree.doesFiniteRayHit(ray, 
		//		1.0f - nudge, // max_t
		//		
		//		0.0, // time
		//		NULL,
		//		std::numeric_limits<unsigned int>::max() // ignore tri
		//		);

		//	testAssert(!hit);
		//}

		// Trace the same ray, but with a max distance of 1.0 + nudge.
		// The ray *should* hit the other quad.
		//{
		//	HitInfo hitinfo;
		//	const bool hit = ob_tree.doesFiniteRayHit(ray, 
		//		1.0f + nudge, // max_t
		//		
		//		0.0, // ignore object
		//		NULL,
		//		std::numeric_limits<unsigned int>::max() // ignore tri
		//		);

		//	testAssert(hit);
		//}
	}

	delete ob1;
	delete ob2;
}








void ObjectTreeTest::doTests()
{
	doSelfIntersectionAvoidanceTest();

	conPrint("ObjectTreeTest::doTests()");
	PCG32 rng(1);

	ObjectTree ob_tree;

	StandardPrintOutput print_output;
	glare::TaskManager task_manager;

	// Create basic material
	Reference<Material> mat(new Diffuse(
		Reference<SpectrumMatParameter>(NULL), // albedo spectrum
		Reference<DisplaceMatParameter>(NULL),
		Reference<DisplaceMatParameter>(NULL),
		Reference<SpectrumMatParameter>(NULL), // base emission
		Reference<SpectrumMatParameter>(NULL),
		Reference<Vec3MatParameter>(), // normal map param
		0,
		false,
		Material::MaterialArgs()
	));

	std::vector<Object*> objects;

	/// Add some random spheres ////
	const int N = 1000;
	for(int i=0; i<N; ++i)
	{
		Reference<Geometry> raysphere(new RaySphere(Vec4f(0,0,0,1), rng.unitRandom() * 0.05));

		const Vec4f pos(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f);

		Object* ob = new Object(
			raysphere,
			js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, pos, Quatf::identity())),
			Object::Matrix3Type::identity(),
			std::vector<Reference<Material> >(1, mat),
			std::vector<EmitterScale>(1),
			std::vector<const IESDatum*>(1, (const IESDatum*)NULL)
			);
		RendererSettings settings;
		ob->buildGeometry(settings, print_output, true, task_manager);
		ob->setObjectIndex(i);
		ob_tree.insertObject(ob);

		objects.push_back(ob);
	}
	ob_tree.build(task_manager, print_output,
		false // verbose
	);

	//ob_tree.printTree(0, 0, std::cout);
	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	//ObjectTreePerThreadData* obtree_context = ob_tree.allocContext();
	ObjectTreePerThreadData obtree_context;//(true);




	/// Do some random traces through the tree ///
	for(int i=0; i<10000; ++i)
	{
		const Ray ray(
			Vec4f(0,0,0,1) + Vec4f(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 0) - Vec4f(0.2f, 0.2f, 0.2f, 0)) * 1.4f,
			normalise(Vec4f(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0) - Vec4f(0.5f, 0.5f, 0.5f,0))),
			1.0e-5f, // min_t
			std::numeric_limits<float>::max(), // max_t
			false // shadow ray
		);

		float time = 0.0;

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//do traceRay() test
		//------------------------------------------------------------------------
		HitInfo hitinfo, hitinfo2;
		const js::ObjectTree::INTERSECTABLE_TYPE* hitob = (js::ObjectTree::INTERSECTABLE_TYPE*)0xF;
		const double t = ob_tree.traceRay(ray, 
			std::numeric_limits<float>::infinity(), // ray length
			time, hitob, hitinfo);
		const double t2 = ob_tree.traceRayAgainstAllObjects(ray, time, hitob, hitinfo2);
		testAssert(hitob != (js::ObjectTree::INTERSECTABLE_TYPE*)0xF);

		testAssert((t > 0.0) == (t2 > 0.0));

		if((t >= 0.0) || (t2 >= 0.0))
		{
			testAssert(::epsEqual(t, t2));
			testAssert(::epsEqual(hitinfo.sub_elem_coords.x, hitinfo2.sub_elem_coords.x));
			testAssert(::epsEqual(hitinfo.sub_elem_coords.y, hitinfo2.sub_elem_coords.y));
			//testAssert(t == t2);
			//testAssert(hitinfo.sub_elem_coords == hitinfo2.sub_elem_coords);
			testAssert(hitinfo.sub_elem_index == hitinfo2.sub_elem_index);
		}

		//------------------------------------------------------------------------
		//Do a doesFiniteRayHitAnything() test
		//------------------------------------------------------------------------
		//const ObjectTree::Real len = rng.unitRandom() * 1.5f;
		//const bool a = ob_tree.doesFiniteRayHit(ray, len, time, NULL, std::numeric_limits<unsigned int>::max());
		//const bool b = ob_tree.allObjectsDoesFiniteRayHitAnything(ray, len, time);
		//testAssert(a == b);

		//if(t >= 0.0)//if the trace hit something after distance t
		//{
		//	//then either we should have hit, or len < t
		//	testAssert(a || len < t);
		//}
	}

	// Delete objects
	for(unsigned int i=0; i<objects.size(); ++i)
		delete objects[i];


	/*{
	js::Triangle tri(Vec3(0,0,0), Vec3(0,1,0), Vec3(1,1,0));

	TriTree tritree;
	tritree.insertTri(tri);

	tritree.build();
	//context = tritree.allocPerThreadData();

	{
	SSE_ALIGN Ray ray(Vec3(0.2f, 0.8f, 1.0f), Vec3(0,0,-1));
	ray.buildRecipRayDir();
	dist = tritree.traceRay(ray, 1e9f, context, hitinfo);

	assert(::epsEqual(dist, 1.0f));
	assert(hitinfo.hittricoords.x > 0.0f && hitinfo.hittricoords.x < 1.0f);
	assert(hitinfo.hittricoords.y > 0.0f && hitinfo.hittricoords.y < 1.0f);
	assert(hitinfo.hittriindex == 0);
	}
	{
	SSE_ALIGN Ray ray(Vec3(0.0f, 0.0f, 1.0f), Vec3(0,0,-1));
	ray.buildRecipRayDir();
	dist = tritree.traceRay(ray, 1e9f, context, hitinfo);

	assert(epsEqual(dist, 1.0f));
	assert(epsEqual(hitinfo.hittricoords.x, 0.0f));
	assert(epsEqual(hitinfo.hittricoords.y, 0.0f));
	assert(hitinfo.hittriindex == 0);
	}
	{
	SSE_ALIGN Ray ray(Vec3(0.999999f, 0.999999f, 1.0f), Vec3(0,0,-1));
	ray.buildRecipRayDir();
	dist = tritree.traceRay(ray, 1e9f, context, hitinfo);

	assert(epsEqual(dist, 1.0f));
	assert(epsEqual(hitinfo.hittricoords.x, 0.0f));
	assert(epsEqual(hitinfo.hittricoords.y, 1.0f));
	assert(hitinfo.hittriindex == 0);

	dist2 = tritree.traceRayAgainstAllTris(ray, hitinfo2);
	assert(dist == dist2);
	assert(hitinfo == hitinfo2);
	}

	}

	//------------------------------------------------------------------------
	//try building up a random set of triangles and inserting into a tree
	//------------------------------------------------------------------------
	{
	TriTree tritree;
	const int NUM_TRIS = 1000;
	for(int i=0; i<NUM_TRIS; ++i)
	{
		js::Triangle tri(Vec3::randomVec(-2, 2), Vec3::randomVec(-2, 2), Vec3::randomVec(-2, 2));
		tritree.insertTri(tri);
	}

	tritree.build();

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 1000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		const float max_t = 1e9f;

		SSE_ALIGN Ray ray(Vec3::randomVec(-3, 3), normalise(Vec3::randomVec(-1, 1)));
		ray.buildRecipRayDir();
		dist = tritree.traceRayAgainstAllTris(ray, hitinfo);
		dist2 = tritree.traceRay(ray, max_t, context, hitinfo2);
		assert(dist == dist2);
		assert(hitinfo == hitinfo2);
	}
	}

	//------------------------------------------------------------------------
	//test out object tree
	//------------------------------------------------------------------------
	{
	ObjectTree obtree;
	const int NUM_OBJECTS = 1000;
	std::vector<TriTree*> tritrees;
	for(int i=0; i<NUM_OBJECTS; ++i)
	{
		TriTree* tritree = new TriTree;
		const int NUM_TRIS = 3;
		for(int i=0; i<NUM_TRIS; ++i)
		{
			const Vec3 v0 = Vec3::randomVec(-2, 2);
			js::Triangle tri(v0, v0 + Vec3::randomVec(-0.3f, 0.3f), v0 + Vec3::randomVec(-0.3f, 0.3f));
			tritree->insertTri(tri);
		}

		tritree->build();
		obtree.insertObject(tritree);
		tritrees.push_back(tritree);
	}
	obtree.build();
	js::ObjectTreePerThreadData* objecttree_context = obtree.allocContext();

	//------------------------------------------------------------------------
	//compare tests against all tritrees with tests against the object tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 1000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		const float max_t = 1e9f;

		SSE_ALIGN Ray ray(Vec3::randomVec(-3, 3), normalise(Vec3::randomVec(-1, 1)));
		ray.buildRecipRayDir();

		Intersectable* hitob = NULL;
		Intersectable* hitob2 = NULL;
		dist = obtree.traceRayAgainstAllObjects(ray, context, *objecttree_context, hitob, hitinfo);
		dist2 = obtree.traceRay(ray, context, *objecttree_context, hitob2, hitinfo2);

		assert(dist == dist2);
		assert(hitob == hitob2);
		assert(hitinfo == hitinfo2);

		//------------------------------------------------------------------------
		//test doesFiniteRayHit query
		//------------------------------------------------------------------------
		const float length = Random::unit() * 7.0f;
		bool hit1 = obtree.doesFiniteRayHitAnything(ray, length, context, *objecttree_context);
		bool hit2 = obtree.allObjectsDoesFiniteRayHitAnything(ray, length, context, *objecttree_context);
		assert(hit1 == hit2);
	}


	for(int i=0; i<(int)tritrees.size(); ++i)
	{
		delete tritrees[i];
	}


	}*/
}





void ObjectTreeTest::doSpeedTest()
{
	PCG32 rng(1);

	ObjectTree ob_tree;

	StandardPrintOutput print_output;
	glare::TaskManager task_manager;

	/// Add some random spheres ////
	const int N = 1000;
	for(int i=0; i<N; ++i)
	{
		Reference<Geometry> raysphere(new RaySphere(Vec4f(0,0,0,1), rng.unitRandom() * 0.05));

		const Vec4f pos(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f);

		Object* ob = new Object(
			raysphere,
			//pos, pos,
			js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, pos, Quatf::identity())),
			Object::Matrix3Type::identity(),
			std::vector<Reference<Material> >(),
			//std::vector<std::vector<int> >(),
			std::vector<EmitterScale>(),
			std::vector<const IESDatum*>()
			);
		RendererSettings settings;
		ob->buildGeometry(settings, print_output, true, task_manager);
		ob_tree.insertObject(ob);
	}
	ob_tree.build(task_manager, print_output,
		false // verbose
	);

	//ob_tree.printTree(0, 0, std::cout);
	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	//ObjectTreePerThreadData* obtree_context = ob_tree.allocContext();
	//ObjectTreePerThreadData obtree_context;//(true);


	{
	Timer testtimer;//start timer
	const int NUM_ITERS = 10000000;

	/// Do some random traces through the tree ///
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const Ray ray(
			Vec4f(0,0,0,1) + Vec4f(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0) - Vec4f(0.2f, 0.2f, 0.2f,0)) * 1.4f,
			normalise(Vec4f(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0) - Vec4f(0.5f, 0.5f, 0.5f,0))),
			1.0e-5f, // min_t
			std::numeric_limits<float>::max(), // max_t
			false // shadow ray
		);

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//do traceRay() test
		//------------------------------------------------------------------------
		HitInfo hitinfo;
		const js::ObjectTree::INTERSECTABLE_TYPE* hitob;
		/*const double t = */ob_tree.traceRay(ray, std::numeric_limits<float>::infinity(), // ray length
			/*time=*/0.0, hitob, hitinfo);
	}

	const double traces_per_sec = (double)NUM_ITERS / testtimer.getSecondsElapsed();
	printVar(traces_per_sec);

	}

	{
	Timer testtimer;//start timer
	const int NUM_ITERS = 10000000;

	/// Do some random traces through the tree ///
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const Ray ray(
			Vec4f(0,0,0,1) + Vec4f(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0) - Vec4f(0.2f, 0.2f, 0.2f,0)) * 1.4f,
			normalise(Vec4f(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0) - Vec4f(0.5f, 0.5f, 0.5f,0))),
			1.0e-5f, // min_t
			std::numeric_limits<float>::max(), // max_t
			false // shadow ray
		);

	}

	const double traces_per_sec = (double)NUM_ITERS / testtimer.getSecondsElapsed();
	printVar(traces_per_sec);

	}
}


void ObjectTreeTest::instancedMeshSpeedTest()
{
	conPrint("ObjectTreeTest::instancedMeshSpeedTest()");
	glare::TaskManager task_manager;

	PCG32 rng(1);

	//------------------------------------------------------------------------
	//load bunny mesh
	//------------------------------------------------------------------------
	Reference<RayMesh> raymesh(new RayMesh("raymesh", false));
	Indigo::Mesh indigoMesh;
	try
	{
		MeshLoader::loadMesh("D:\\programming\\models\\bunny\\reconstruction\\bun_zipper.ply", indigoMesh, 1.0);
		raymesh->fromIndigoMesh(indigoMesh);
	}
	catch(glare::Exception&)
	{
		testAssert(false);
	}

	//------------------------------------------------------------------------
	//insert random instances
	//------------------------------------------------------------------------
	RendererSettings settings;

	StandardPrintOutput print_output;
	Geometry::BuildOptions options;
	raymesh->build(
		options,
		print_output,
		true,
		task_manager
		);

	ObjectTree ob_tree;

	for(int i=0; i<200; ++i)
	{
		//Matrix3d rot = Matrix3d::identity();

		Object::Matrix3Type rot = Object::Matrix3Type::rotationMatrix(normalise(Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom())), rng.unitRandom() * 6.0f);

		rot.scale(0.3f);

		const Vec4f offset(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f);

		Object* object = new Object(
			Reference<Geometry>(raymesh.getPointer()),
			//offset, offset,
			js::Vector<TransformKeyFrame, 16>(1, TransformKeyFrame(0.0, offset, Quatf::identity())),
			rot,
			std::vector<Reference<Material> >(),
			//std::vector<std::vector<int> >(),
			std::vector<EmitterScale>(),
			std::vector<const IESDatum*>()
			);
		
		object->buildGeometry(settings, print_output, true, task_manager);

		ob_tree.insertObject(object);
	}

	//------------------------------------------------------------------------
	//compile tree
	//------------------------------------------------------------------------
	ob_tree.build(task_manager, print_output,
		false // verbose
	);

	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	//ObjectTreePerThreadData* obtree_context = ob_tree.allocContext();
	ObjectTreePerThreadData obtree_context;//(true);



	//------------------------------------------------------------------------
	//do traces
	//------------------------------------------------------------------------
	{
	Timer testtimer;//start timer
	const int NUM_ITERS = 3000000;
	int num_hits = 0;

	/// Do some random traces through the tree ///
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const Vec4f start(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),1);
		const Vec4f end(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),1);

		const Ray ray(
			start,
			normalise(end - start),
			1.0e-5f, // min_t
			std::numeric_limits<float>::max(), // max_t
			false // shadow ray
		);

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//Do a doesFiniteRayHitAnything() test
		//------------------------------------------------------------------------
		//const ObjectTree::Real len = Vec4f(end - start).length();//rng.unitRandom() * 1.5;
		//const bool a = ob_tree.doesFiniteRayHit(ray, len, start_time, NULL, std::numeric_limits<unsigned int>::max());
		//if(a)
		//	num_hits++;
	}

	const double frac_hit = (double)num_hits / (double)NUM_ITERS;
	const double traces_per_sec = (double)NUM_ITERS / testtimer.getSecondsElapsed();
	printVar(testtimer.getSecondsElapsed());
	printVar(frac_hit);
	printVar(traces_per_sec);

	}

}
#endif


} //end namespace js


#endif
