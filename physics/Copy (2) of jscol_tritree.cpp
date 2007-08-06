/*=====================================================================
tritree.cpp
-----------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_tritree.h"


#include "jscol_treenode.h"
#include "../utils/stringutils.h"//TEMP
#include "../monte carlo tracer/globals.h"//TEMP

#ifdef DO_INTRINSICS
#include <xmmintrin.h>
#endif

#ifdef OPENGL_DRAWABLE
	#include <windows.h>
	#include <GL/gl.h>
#endif


namespace js
{


const int NODE_STACK_SIZE = 100;


TriTree::TriTree()
//:	rootnode(NULL)
:	root_aabb(Vec3(0,0,0), Vec3(0,0,0))
{
//	rootnode = new TreeNode(0);
	nodes.push_back(TreeNode());

	//const int rootaddr = (int)&(*nodes.begin());
	//const int rootmod = rootaddr % 64;

	numnodesbuilt = 0;

	//TEMP HACK:
//	nodestack.resize(NODE_STACK_SIZE);
}


TriTree::~TriTree()
{
//	delete rootnode;
}

void TriTree::insertTri(const js::Triangle& tri)
{
	//rootnode->addTri(tri);
	tris.push_back(tri);//js::EdgeTri(tri.v0(), tri.v1(), tri.v2()));
	edgetris.push_back(js::EdgeTri(tri.v0(), tri.v1(), tri.v2()));
}

void TriTree::reserveTriMem(int numtris)//not needed, but speeds things up
{
	tris.reserve(numtris);
	edgetris.reserve(numtris);
}

	//returns dist till hit tri, neg number if missed.
float TriTree::traceRay(const ::Ray& ray, js::EdgeTri*& hittri_out)
{
	//return TreeNode::traceRayIterative(nodes, leafgeom, ray, hit_tri_out, min, max);
	
	hittri_out = NULL;

	if(nodes.empty())
		return -1.0f;

	const Vec3 recip_unitraydir(1.0f / ray.unitdir.x, 1.0f / ray.unitdir.y, 1.0f / ray.unitdir.z);


	float aabb_enterdist, aabb_exitdist;
	//const bool hit_aabb = aabb.rayAABBTrace(ray, aabb_enterdist, aabb_exitdist);
	const bool hit_aabb = root_aabb.rayAABBTrace(ray.startpos, recip_unitraydir, aabb_enterdist, aabb_exitdist);

	if(!hit_aabb)
		return -1.0f;


	//TEMP TEST:
	//hittri_out = &leafgeom[0];
	//return aabb_enterdist;


	//NOTE: make this a tree node member var.
	//static StackFrame nodestack[NODE_STACK_SIZE];
	//NOTE: This fixed stack size is dodgy.

	nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);
	int stacktop = 0;//index of node on top of stack

	StackFrame frame;
	/*int current;
	float tmin;
	float tmax;*/

	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		frame = nodestack[stacktop];

		int current = frame.node;
		/*current = nodestack[stacktop].node;//index to current node
		tmin = nodestack[stacktop].tmin;
		tmax = nodestack[stacktop].tmax;*/
				
		//pop current node off stack	
		stacktop--;

		//TreeNode* current = frame.node;

		while(!nodes[current].isLeafNode())
		{
			//------------------------------------------------------------------------
			//prefetch child node memory
			//------------------------------------------------------------------------
#ifdef DO_INTRINSICS
			_mm_prefetch((const char *)(&nodes[current+1]), _MM_HINT_T0);	
			//_mm_prefetch((const char *)(&nodes[current+2]), _MM_HINT_T0);	
			//_mm_prefetch((const char *)(&nodes[current+3]), _MM_HINT_T0);	
			_mm_prefetch((const char *)(&nodes[nodes[current].positive_child]), _MM_HINT_T0);	
#endif			
			//while current node is not a leaf..

			//if have hit a tri before the AABB of this node, then ignore it.
			//if(closest_dist <= frame.tmin)
			//	continue;
		
//			nodes_touched++;

		
			//process children of this node

			//signed dist to split plane along ray direction from ray origin
			const float axis_sgnd_dist = nodes[current].dividing_val - ray.startpos[nodes[current].dividing_axis];
			//const float sgnd_dist_to_split = axis_sgnd_dist / ray.unitdir[nodes[current].dividing_axis];//recip_unitraydir[nodes[current].dividing_axis];/// ray.unitdir[current->dividing_axis];
			const float sgnd_dist_to_split = axis_sgnd_dist * recip_unitraydir[nodes[current].dividing_axis];/// ray.unitdir[current->dividing_axis];
			
			//nearnode is the halfspace that the ray origin is in
			int nearnode;
			int farnode;
			if(axis_sgnd_dist > 0.0f)
			{
				//nearnode = nodes[current].negative_child;
				nearnode = current + 1;
				farnode = nodes[current].positive_child;
			}
			else
			{
				nearnode = nodes[current].positive_child;
				//farnode = nodes[current].negative_child;
				farnode = current + 1;
			}

			if(sgnd_dist_to_split < 0.0f || //if heading away from split
				sgnd_dist_to_split > frame.tmax)//or ray segment ends b4 split
			{
				//whole interval is on near cell	
				
				current = nearnode;
			}
			else
			{
				if(frame.tmin > sgnd_dist_to_split)//else if ray seg starts after split
				{
					//whole interval is on far cell.
					current = farnode;
				}
				else
				{
					//ray hits plane - double recursion, into both near and far cells.
								
					stacktop++;
					assert(stacktop < nodestack.size());//nodestack.size());
					nodestack[stacktop] = StackFrame(farnode, sgnd_dist_to_split, frame.tmax);
					//nodestack.push_back(StackFrame(farnode, sgnd_dist_to_split, frame.tmax));
					
					
					/*nodestack.resize(nodestack.size() + 1);
					nodestack.back().node = farnode;
					nodestack.back().tmin = sgnd_dist_to_split;
					nodestack.back().tmax = frame.tmax;*/
					
					
					//process near child
					current = nearnode;			

					frame.tmax = sgnd_dist_to_split;
				}
			}

		}//end while current node is not a leaf..

		//'current' is a leaf node..

		//------------------------------------------------------------------------
		//intersect with leaf tris
		//------------------------------------------------------------------------
		assert(nodes[current].num_leaf_tris >= 0);

		//------------------------------------------------------------------------
		//prefetch all data
		//------------------------------------------------------------------------
		/*{
		for(int i=0; i<nodes[current].num_leaf_tris; ++i)
		{
			_mm_prefetch((const char *)(leafgeom[nodes[current].positive_child + i]), _MM_HINT_T0);		
		}
		}*/


		float closest_dist = 2.0e9f;

		//const js::Triangle* leaftri = leafgeom[nodes[current].positive_child];
		int triindex = nodes[current].positive_child;

		for(int i=0; i<nodes[current].num_leaf_tris; ++i)
		{
			//const int triindex = nodes[current].positive_child + i;
			
			assert(triindex >= 0 && triindex < leafgeom.size());


			//try prefetching next tri.
		//	_mm_prefetch((const char *)((const char *)leaftri + sizeof(js::Triangle)*(triindex + 1)), _MM_HINT_T0);		

			const float tridist = edgetris[leafgeom[triindex]].traceRayMolTrum(ray);//traceRay(ray);


			if(tridist >= 0.0f && tridist < closest_dist)
			{
				//hitleaf = true;
				closest_dist = tridist;
				hittri_out = &edgetris[leafgeom[triindex]];//nodes[current].leaftris[i];
			}

			triindex++;
		}


		/*const int leafindex = nodes[current].positive_child;

		js::Triangle* leaftri = &(*leafgeom[leafindex]);
		const js::Triangle* end = leaftri + nodes[current].num_leaf_tris;

		for(; leaftri != end; ++leaftri)
		{
			const float tridist = leaftri->traceRayMolTrum(ray);

			if(tridist >= 0.0f && tridist < closest_dist)
			{
				//hitleaf = true;
				closest_dist = tridist;
				hittri_out = leaftri;//leafgeom[triindex];//nodes[current].leaftris[i];
			}
		}*/


		if(closest_dist < 1e9f)//if hit a leaf tri
			return closest_dist;


	}//end while !nodestack.empty()

	return -1.0f;//missed all tris
}

void TriTree::build()
{
	/*std::vector<js::EdgeTri*> tripointers(tris.size());
	{
	for(int i=0; i<tris.size(); ++i)
		tripointers[i] = &tris[i];
	}*/



	assert(tris.size() > 0);

	if(tris.empty())
		return;

	//------------------------------------------------------------------------
	//calc root node's aabbox
	//------------------------------------------------------------------------
	conPrint("calcing root AABB.");
	{
	root_aabb.min = tris[0].getVertex(0);
	root_aabb.max = tris[0].getVertex(0);
	for(unsigned int i=0; i<tris.size(); ++i)
		for(int v=0; v<3; ++v)
			root_aabb.enlargeToHoldPoint(tris[i].getVertex(v));
	}
	
	assert(root_aabb.invariant());

	//TreeNode::calcAABB(tripointers, min, max);
	
	//TEMP: HACK:
	//const float EPS = 0.0f;
	//aabb.min -= Vec3(EPS, EPS, EPS);
	//aabb.max += Vec3(EPS, EPS, EPS);
	//rootnode->calcAABB(tripointers);
	//nodes[0].calcAABB(tripointers);

	//------------------------------------------------------------------------
	//compute max allowable depth
	//------------------------------------------------------------------------
	const int numtris = tris.size();
	const int max_depth = (int)(2.0f + logBase2((float)numtris) * 1.2f);

	//prealloc stack vector
	nodestack.resize(max_depth + 1);



	const int expected_numnodes = (int)((float)numtris * 6.0f);

	const int nodemem = expected_numnodes * sizeof(js::TreeNode);

	conPrint("reserving N nodes: " + ::toString(expected_numnodes) + "("
		+ ::getNiceByteSize(nodemem) + ")");


	nodes.reserve(expected_numnodes);


	//------------------------------------------------------------------------
	//reserve space for leaf geom array
	//------------------------------------------------------------------------
	leafgeom.reserve(numtris * 9);

	const int leafgeommem = leafgeom.capacity() * sizeof(TRI_INDEX);
	conPrint("leafgeom reserved: " + ::toString(leafgeom.capacity()) + "("
		+ ::getNiceByteSize(leafgeommem) + ")");




	//rootnode->build(tripointers, 0);

	std::vector<TRI_INDEX> roottris(tris.size());
	{
	for(unsigned int i=0; i<tris.size(); ++i)
		roottris[i] = i;
	}

	const int tris_size = tris.size();
	tri_boxes.reserve(tris.size());
	for(unsigned int i=0; i<tris.size(); ++i)
	{
		tri_boxes.push_back(AABBox(tris[i].v0(), tris[i].v0()));
		tri_boxes[i].enlargeToHoldPoint(tris[i].v1());
		tri_boxes[i].enlargeToHoldPoint(tris[i].v2());

	//tri_boxes[i].min = tris[i].v0();
		//tri_boxes[i].max = tris[i].v0();

		/*{
		for(int v=1; v<3; ++v)//for each other vert;
		{
			tri_boxes[i].enlargeToHoldPoint(tris[i].getVertex(v));

			for(int c=0; c<3; ++c)
			{
				if(tris[i].getVertex(v)[c] < tri_boxes[i].min[c])
					tri_boxes[i].min[c] = tris[i].getVertex(v)[c];
				
				if(tris[i].getVertex(v)[c] > tri_boxes[i].max[c])
					tri_boxes[i].max[c] = tris[i].getVertex(v)[c];
			}
		}*/

		/*assert(trimax[0] >= trimin[0]);
		assert(trimax[1] >= trimin[1]);
		assert(trimax[2] >= trimin[2]);*/

		assert(tri_boxes[i].invariant());
	}


	conPrint("sizeof(js::EdgeTri): " + ::toString(sizeof(js::EdgeTri)));



	doBuild(0, roottris, 0, max_depth, root_aabb);

	const int numnodes = nodes.size();
	const int leafgeomsize = leafgeom.size();

	conPrint("nodes used: " + ::toString(numnodes) + " (" + 
		::getNiceByteSize(numnodes * sizeof(js::TreeNode)) + ")");

	conPrint("leafgeom size: " + ::toString(leafgeomsize) + " (" + 
		::getNiceByteSize(leafgeomsize * sizeof(TRI_INDEX)) + ")");


	const int geomaddr = (int)&(*leafgeom.begin());
	const int geomaddrmod = geomaddr % 64;

}





bool TriTree::triIntersectsAABB(int triindex, const AABBox& aabb, int split_axis,
								bool is_neg_child)
{
	//------------------------------------------------------------------------
	//for now just intersect aabb of triangle with aabb
	//------------------------------------------------------------------------

	//------------------------------------------------------------------------
	//test each separating axis
	//------------------------------------------------------------------------
	for(int i=0; i<3; ++i)//for each axis
	{
		if(i == split_axis)
		{
			//ONLY tris hitting the MIN plane are considered in AABB
			/*if(trimax[i] < aabb.min[i])//if less than min
				return false;//not in box
			
			if(trimin[i] >= aabb.max[i])//if tris is touching max in this axis
			{
				if(aabb.min[i] == aabb.max[i])//must hit the zero width box
				{
				}
				else
					return false;//not in box
			}*/
			if(is_neg_child)
			{
				//then this box gets tris which hit the split, ie which hit
				//max[split_axis]

				if(tri_boxes[triindex].max[i] < aabb.min[i])
					return false;

				if(tri_boxes[triindex].min[i] > aabb.max[i])
					return false;
			}
			else
			{
				//then this box gets everything that touches it, EXCEPT the
				//tris which hit the split. (ie hit min[split_axis])
				if(tri_boxes[triindex].max[i] <= aabb.min[i])//reject even if touching
					return false;

				if(tri_boxes[triindex].min[i] > aabb.max[i])
					return false;
			}
		}
		else
		{
			//tris touching min or max planes are considered in aabb	
			if(tri_boxes[triindex].max[i] < aabb.min[i])
				return false;
			
			if(tri_boxes[triindex].min[i] > aabb.max[i])
				return false;
		}
	}

	return true;
}




float TriTree::getCostForSplit(int cur, const std::vector<TRI_INDEX>& nodetris,
					  int axis, float splitval, const AABBox& aabb)
{

	AABBox negbox(aabb.min, aabb.max);
	negbox.max[axis] = splitval;

	AABBox posbox(aabb.min, aabb.max);
	posbox.min[axis] = splitval;

	int num_in_neg_bin = 0;
	int num_in_pos_bin = 0;

	//for each tri
	for(unsigned int i=0; i<nodetris.size(); ++i)
	{
		if(triIntersectsAABB(nodetris[i], negbox, axis, true))
			num_in_neg_bin++;

		if(triIntersectsAABB(nodetris[i], posbox, axis, false))
			num_in_pos_bin++;
	}

/*	const float cost = negbox.getSurfaceArea() * 
						log( myMax((float)num_in_neg_bin, 1.0f) ) * 1.4f
					+ posbox.getSurfaceArea() * 
						log( myMax((float)num_in_pos_bin, 1.0f) ) * 1.4f;*/

	const float cost = negbox.getSurfaceArea() * (float)num_in_neg_bin
					+ posbox.getSurfaceArea() * (float)num_in_pos_bin;
	
		
	const float leaf_intsct_cost = 1.0f;
	const float node_traverse_cost = 1.0f;
	const float av_tris_per_leaf = 4.0f;


//	const float cost = negbox.getSurfaceArea() * log((float)num_in_neg_bin*5.0f + 1.0f)
//		+ posbox.getSurfaceArea() * log((float)num_in_pos_bin*5.0f + 1.0f);

	/*float negcost;
	if(num_in_neg_bin > 0)
	{
		const float expected_subtree_depth = ::logBase2((float)num_in_neg_bin / av_tris_per_leaf);

		negcost = negbox.getSurfaceArea() * (
		expected_subtree_depth * node_traverse_cost//tree traversal cost
		 + av_tris_per_leaf*leaf_intsct_cost);
	}
	else
	{
		negcost = negbox.getSurfaceArea() * node_traverse_cost;
	}

	float poscost;

	if(num_in_pos_bin > 0)
	{
		const float expected_subtree_depth = ::logBase2((float)num_in_pos_bin / av_tris_per_leaf);

		poscost = posbox.getSurfaceArea() * (
		expected_subtree_depth * node_traverse_cost
		+ av_tris_per_leaf*leaf_intsct_cost);
	}
	else
	{
		poscost = posbox.getSurfaceArea() * node_traverse_cost;
	}


	const float cost = negcost + poscost;*/


	return cost;

}







void TriTree::doBuild(int cur, //index of current node getting built
						const std::vector<TRI_INDEX>& nodetris,//tris for current node
						int depth, //depth of current node
						int maxdepth, //max permissible depth
						const AABBox& cur_aabb)//AABB of current node
{
	//const int sz = sizeof(std::vector<TreeNode>);//sizeof(TreeNode);
	//const int sz2 = sizeof(TreeNode);//sizeof(TreeNode);

	/*if(depth == 6)
	{
		const int totalnum = 1 << 6;
		conPrint(::toString(numnodesbuilt) + "/" + ::toString(totalnum) + " nodes at depth 6 built.");

		numnodesbuilt++;
	}*/




	
	if(nodetris.size() == 0)
	{
		//no tris were allocated to this node.
		//so make it a leaf node with 0 tris
		nodes[cur].num_leaf_tris = 0;
		return;
	}

	//------------------------------------------------------------------------
	//test for termination of splitting
	//------------------------------------------------------------------------
	const int SPLIT_THRESHOLD = 2;

	if(nodetris.size() <= SPLIT_THRESHOLD || depth >= maxdepth)
	{
		//make this node a leaf node.
	//	nodes[cur].leaftris = tris;

		nodes[cur].num_leaf_tris = (int)nodetris.size();
		nodes[cur].positive_child = (int)leafgeom.size();
		//positive_child is also the index into leaf geom.

		for(unsigned int i=0; i<nodetris.size(); ++i)
			leafgeom.push_back(nodetris[i]);

		return;
	}

	float smallest_cost = 2e12f;
	int best_axis = -1;
	float best_div_val = -666.0f;

	const int num_tris = nodetris.size();

	const bool do_exhaustive_search = false;
	if(nodetris.size() < 10 && do_exhaustive_search)
	{
		for(int axis=0; axis<3; ++axis)
			for(unsigned int t=0; t<nodetris.size(); ++t)//for each tri
			{
				float minbound = tri_boxes[nodetris[t]].min[axis];
				float maxbound = tri_boxes[nodetris[t]].max[axis];
				//getTriExtremes(*tris[t], axis, minbound, maxbound);
	

				//make sure split is in current node box.
				if(minbound < cur_aabb.min[axis])
					minbound = cur_aabb.min[axis];
				if(maxbound > cur_aabb.max[axis])
					maxbound = cur_aabb.max[axis];
				
				
				//try smaller side of tri: minbound

				{
				const float cost = getCostForSplit(cur, nodetris, axis, minbound, cur_aabb);

				if(cost < smallest_cost)
				{
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = minbound;
				}
				}

				//try larger side of tri: maxbound

				{
				const float cost = getCostForSplit(cur, nodetris, axis, maxbound, cur_aabb);

				if(cost < smallest_cost)
				{
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = maxbound;
				}
				}

				{
				const float cost = getCostForSplit(cur, nodetris, axis, cur_aabb.min[axis], cur_aabb);

				if(cost < smallest_cost)
				{
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = cur_aabb.min[axis];
				}
				}

				{
				const float cost = getCostForSplit(cur, nodetris, axis, cur_aabb.max[axis], cur_aabb);

				if(cost < smallest_cost)
				{
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = cur_aabb.max[axis];
				}
				}
			}//end for each tri


	}
	else
	{
		for(int axis=0; axis<3; ++axis)
		{
			const float minval = cur_aabb.min[axis];
			const float maxval = cur_aabb.max[axis];

				//float splitfrac = 0.0f; splitfrac < 1.0f; splitfrac += 0.098f)
			const int NUM_SPLIT_VALS = 7;
			for(int i=0; i<NUM_SPLIT_VALS; ++i)
			{
				const float splitfrac = (float)i / (float)(NUM_SPLIT_VALS - 1);

				const float splitval = minval + (maxval - minval) * splitfrac;

				const float cost = getCostForSplit(cur, nodetris, axis, splitval, cur_aabb);

				if(cost < smallest_cost)
				{
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = splitval;
				}
			}
		}
	}


	assert(best_axis >= 0 && best_axis <= 2);
	assert(best_div_val != -666.0f);

	assert(best_div_val >= cur_aabb.min[best_axis]);
	assert(best_div_val <= cur_aabb.max[best_axis]);
	
	nodes[cur].dividing_axis = best_axis;
	nodes[cur].dividing_val = best_div_val;





	//------------------------------------------------------------------------
	//make dividing axis the longest axis
	//------------------------------------------------------------------------
	/*if((cur_aabb.max.x - cur_aabb.min.x) >= (cur_aabb.max.y - cur_aabb.min.y) && (cur_aabb.max.z - cur_aabb.min.z))
		nodes[cur].dividing_axis = 0;
	else
	{
		if((cur_aabb.max.y - cur_aabb.min.y) >= (cur_aabb.max.z - cur_aabb.min.z))
			nodes[cur].dividing_axis = 1;
		else
			nodes[cur].dividing_axis = 2;
	}

	//------------------------------------------------------------------------
	//do midpoint split
	//------------------------------------------------------------------------
	nodes[cur].dividing_val = (cur_aabb.min[nodes[cur].dividing_axis] + 
								cur_aabb.max[nodes[cur].dividing_axis]) * 0.5f;
	
*/

		//calc average value of vertex position along dividing axis.
		//calc min and max as well
		/*float average = 0.0f;

		assert(tris.size() >= 1);

		{		
		for(int i=0; i<(int)tris.size(); ++i)
			for(int v=0; v<3; ++v)
			{
				average += tris[i]->getVertex(v)[nodes[cur].dividing_axis];
			}
		}

		average /= (float)(tris.size() * 3);

		nodes[cur].dividing_val = average;*/

		//------------------------------------------------------------------------
		//clamp dividing value to lie inside this node's AABB
		//NOTE: this is symptomatic of a pretty fucked up node.
		//------------------------------------------------------------------------
		/*if(nodes[cur].dividing_val < nodes[cur].min[nodes[cur].dividing_axis])
			nodes[cur].dividing_val = nodes[cur].min[nodes[cur].dividing_axis];
		else if(nodes[cur].dividing_val > nodes[cur].max[nodes[cur].dividing_axis])
			nodes[cur].dividing_val = nodes[cur].max[nodes[cur].dividing_axis];

		//TEMP:
		nodes[cur].dividing_val = (nodes[cur].min[nodes[cur].dividing_axis] + 
									nodes[cur].max[nodes[cur].dividing_axis]) * 0.5f;*/
								


	//}
	//else
	//{
	//	average = 0.0f;
	//}

	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	AABBox negbox(cur_aabb.min, cur_aabb.max);
	negbox.max[nodes[cur].dividing_axis] = nodes[cur].dividing_val;

	AABBox posbox(cur_aabb.min, cur_aabb.max);
	posbox.min[nodes[cur].dividing_axis] = nodes[cur].dividing_val;


	//------------------------------------------------------------------------
	//assign tris to neg and pos child node bins
	//------------------------------------------------------------------------
	std::vector<TRI_INDEX> neg_tris;
	neg_tris.reserve(nodetris.size() / 2);
	std::vector<TRI_INDEX> pos_tris;
	pos_tris.reserve(nodetris.size() / 2);

	for(unsigned int i=0; i<nodetris.size(); ++i)//for each tri
	{
		bool inserted = false;
		//if(triIntersectsAABB(*tris[i], nodes[nodes[cur].negative_child].min, nodes[nodes[cur].negative_child].max))
		if(triIntersectsAABB(nodetris[i], negbox, nodes[cur].dividing_axis, true))
		{
			neg_tris.push_back(nodetris[i]);
			inserted = true;
		}

		//if(triIntersectsAABB(*tris[i], nodes[nodes[cur].positive_child].min, nodes[nodes[cur].positive_child].max))
		if(triIntersectsAABB(nodetris[i], posbox, nodes[cur].dividing_axis, false))
		{
			pos_tris.push_back(nodetris[i]);
			inserted = true;
		}

		assert(inserted);	//check we haven't 'lost' any tris
	}
	
	//------------------------------------------------------------------------
	//create negative child node, next in the array.
	//------------------------------------------------------------------------
	nodes.push_back(TreeNode());
	const int negchild_index = nodes.size() - 1;

	//build left node, recursive call.
	doBuild(negchild_index/*nodes[cur].negative_child*/, neg_tris, depth + 1, maxdepth, negbox);


	nodes.push_back(TreeNode());
	nodes[cur].positive_child = nodes.size() - 1;//positive_child = new TreeNode(depth + 1);

	doBuild(nodes[cur].positive_child, pos_tris, depth + 1, maxdepth, posbox);

}














void TriTree::draw() const
{
#ifdef OPENGL_DRAWABLE
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);			
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);

	rootnode->draw();
#endif
}


void TriTree::printTree(int cur, int depth, std::ostream& out)
{

	if(nodes[cur].isLeafNode())
	{
		for(int i=0; i<depth; ++i)
			out << "  ";

		out << "leaf node (num leaf tris: " << nodes[cur].num_leaf_tris << ")\n";
	
	}
	else
	{	
		//process neg child
		this->printTree(cur + 1, depth + 1, out);

		for(int i=0; i<depth; ++i)
			out << "  ";

		out << "interior node (split axis: "  << nodes[cur].dividing_axis << ", split val: "
				<< nodes[cur].dividing_val << ")\n";
		
		//process pos child
		this->printTree(nodes[cur].positive_child, depth + 1, out);
	
	}
}

} //end namespace jscol







