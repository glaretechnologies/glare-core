/*=====================================================================
treenode.cpp
------------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_treenode.h"


//#define OPENGL_DRAWABLE


#include "../maths/mathstypes.h"
#include "../simpleraytracer/ray.h"

#ifdef OPENGL_DRAWABLE
	#include <windows.h>
	#include <GL/gl.h>
#endif


namespace js
{




TreeNode::TreeNode(/*int depth*/)
:	//depth(depth_),
	positive_child(NULL),
	negative_child(NULL)
{
//	positive_child = new TreeNode(depth + 1);
//	negative_child = new TreeNode(depth + 1);

	dividing_axis = depth % 3;
	dividing_val= 0.0f;
}


TreeNode::~TreeNode()
{
	delete positive_child;
	delete negative_child;
}

/*void TreeNode::addTri(const js::Triangle& tri)
{
	tris.push_back(tri);
}*/



float rayAABBTrace(const Ray& ray, const Vec3& min, const Vec3& max)
{
	float largestneardist;
	float smallestfardist;

	//-----------------------------------------------------------------
	//test x
	//-----------------------------------------------------------------
	if(ray.unitdir.x > 0)
	{
		largestneardist = (min.x - ray.startpos.x) / ray.unitdir.x;
		smallestfardist = (max.x - ray.startpos.x) / ray.unitdir.x;
	}
	else
	{
		smallestfardist = (min.x - ray.startpos.x) / ray.unitdir.x;
		largestneardist = (max.x - ray.startpos.x) / ray.unitdir.x;
	}
		
	//-----------------------------------------------------------------
	//test y
	//-----------------------------------------------------------------
	float ynear;
	float yfar;

	if(ray.unitdir.y > 0)
	{
		ynear = (min.y - ray.startpos.y) / ray.unitdir.y;
		yfar = (max.y - ray.startpos.y) / ray.unitdir.y;
	}
	else
	{
		yfar = (min.y - ray.startpos.y) / ray.unitdir.y;
		ynear = (max.y - ray.startpos.y) / ray.unitdir.y;
	}

	if(ynear > largestneardist)
		largestneardist = ynear;

	if(yfar < smallestfardist)
		smallestfardist = yfar;

	if(largestneardist > smallestfardist)
		return -1.0f;//false;

	//-----------------------------------------------------------------
	//test z
	//-----------------------------------------------------------------
	float znear;
	float zfar;

	if(ray.unitdir.z > 0)
	{
		znear = (min.z - ray.startpos.z) / ray.unitdir.z;
		zfar = (max.z - ray.startpos.z) / ray.unitdir.z;
	}
	else
	{
		zfar = (min.z - ray.startpos.z) / ray.unitdir.z;
		znear = (max.z - ray.startpos.z) / ray.unitdir.z;
	}

	if(znear > largestneardist)
		largestneardist = znear;

	if(zfar < smallestfardist)
		smallestfardist = zfar;

	if(largestneardist > smallestfardist)
		return -1.0f;//return false;
	else
	{
		return myMax(0.0f, largestneardist);//largestneardist <= raystart.getDist(rayend);//Vec3(rayend - raystart).length());
	}
}


bool triIntersectsAABB(const js::Triangle& tri, const Vec3& min, const Vec3& max)
{
	//------------------------------------------------------------------------
	//for now just intersect aabb of triangle with aabb
	//------------------------------------------------------------------------

	//------------------------------------------------------------------------
	//compute tri AABB
	//------------------------------------------------------------------------
	Vec3 trimin = tri.v0();
	{
	for(int i=0; i<3; ++i)//for each component
		if(tri.v1()[i] < trimin[i])
			trimin[i] = tri.v1()[i];
	}
	{
	for(int i=0; i<3; ++i)//for each component
		if(tri.v2()[i] < trimin[i])
			trimin[i] = tri.v2()[i];
	}

	Vec3 trimax = tri.v0();
	{
	for(int i=0; i<3; ++i)//for each component
		if(tri.v1()[i] > trimax[i])
			trimax[i] = tri.v1()[i];
	}
	{
	for(int i=0; i<3; ++i)//for each component
		if(tri.v2()[i] > trimax[i])
			trimax[i] = tri.v2()[i];
	}

	assert(trimax[0] >= trimin[0]);
	assert(trimax[1] >= trimin[1]);
	assert(trimax[2] >= trimin[2]);


	//------------------------------------------------------------------------
	//test each separating axis
	//------------------------------------------------------------------------
	for(int i=0; i<3; ++i)
	{
		if(trimax[i] < min[i])
			return false;
		
		if(trimin[i] > max[i])
			return false;
	}

	return true;
}



void TreeNode::calcAABB(const std::vector<js::Triangle*>& tris)
{
	if(tris.size() == 0)
		return;

	min = tris[0]->v0();
	max = tris[0]->v0();

	for(int i=0; i<(int)tris.size(); ++i)//for each tri
		for(int v=0; v<3; ++v)//for each vert
			for(int c=0; c<3; ++c)//for each component
			{
				if(tris[i]->getVertex(v)[c] < min[c])
					min[c] = tris[i]->getVertex(v)[c];

				if(tris[i]->getVertex(v)[c] > max[c])
					max[c] = tris[i]->getVertex(v)[c];
			}

	assert(max[0] >= min[0]);
	assert(max[1] >= min[1]);
	assert(max[2] >= min[2]);
}

void TreeNode::build(const std::vector<js::Triangle*>& tris, int depth)
{
	if(tris.size() == 0)
		return;

//	min = tris[0].v0();
//	max = tris[0].v0();

	const int SPLIT_THRESHOLD = 5;

	if(tris.size() < SPLIT_THRESHOLD || depth >= 15)
	{
		//make this node a leaf node.
		leaftris = tris;
		//tris.clear();
		return;
	}

	//------------------------------------------------------------------------
	//make dividing axis the longest axis
	//------------------------------------------------------------------------
	if((max.x - min.x) >= (max.y - min.y) && (max.z - min.z))
		dividing_axis = 0;
	else
	{
		if((max.y - min.y) >= (max.z - min.z))
			dividing_axis = 1;
		else
			dividing_axis = 2;
	}

	


	//calc average value of vertex position along dividing axis.
	//calc min and max as well
	float average = 0.0f;

	assert(tris.size() >= 1);

	{		
	for(int i=0; i<(int)tris.size(); ++i)
		for(int v=0; v<3; ++v)
		{
			average += tris[i]->getVertex(v)[dividing_axis];
		}
	}

	average /= (float)(tris.size() * 3);

	dividing_val = average;

	//------------------------------------------------------------------------
	//clamp dividing value to lie inside this node's AABB
	//NOTE: this is symptomatic of a pretty fucked up node.
	//------------------------------------------------------------------------
	if(dividing_val < min[dividing_axis])
		dividing_val = min[dividing_axis];
	else if(dividing_val > max[dividing_axis])
		dividing_val = max[dividing_axis];

	//}
	//else
	//{
	//	average = 0.0f;
	//}

	

	negative_child = new TreeNode(depth + 1);
	negative_child->min = min;
	negative_child->max = max;

	negative_child->max[dividing_axis] = dividing_val;


	positive_child = new TreeNode(depth + 1);
	positive_child->min = min;
	positive_child->max = max;

	positive_child->min[dividing_axis] = dividing_val;


	std::vector<js::Triangle*> neg_tris;
	std::vector<js::Triangle*> pos_tris;


	//for each tri
	for(int i=0; i<(int)tris.size(); ++i)
	{
		//determine if tri is on positive, negative, or straddles dividing plane.
	
		//if positive, add to positive node's list

		//if negative, add to negative node's list

		if(triIntersectsAABB(*tris[i], negative_child->min, negative_child->max))
			//negative_child->addTri(tris[i]);
			neg_tris.push_back(tris[i]);

		if(triIntersectsAABB(*tris[i], positive_child->min, positive_child->max))
			//positive_child->addTri(tris[i]);
			pos_tris.push_back(tris[i]);
	}

	//TEMP DEBUG
	const int negnum = neg_tris.size();
	const int posnum = pos_tris.size();

	const int bothnum = negnum + posnum - (int)tris.size();

	const float straddling_fraction = (float)bothnum / (float)tris.size();
	if(straddling_fraction >= 0.4f)
	{
		//make this node a leaf
		leaftris = tris;
		delete negative_child;
		negative_child = NULL;
		delete positive_child;
		positive_child = NULL;

	}
	else
	{


		negative_child->build(neg_tris, depth + 1);
		positive_child->build(pos_tris, depth + 1);
	}

	//for each tri
	/*for(int i=0; i<(int)tris.size(); ++i)
	{
		//determine if tri is on positive, negative, or straddles dividing plane.
	
		//if straddles, add to this nodes list of tris.

		//if positive, add to positive node's list

		//if negative, add to negative node's list

	
		if(tris[i].getVertex(0)[dividing_axis] <= average)
		{
			if(tris[i].getVertex(1)[dividing_axis] <= average)
			{
				if(tris[i].getVertex(2)[dividing_axis] <= average)
				{
					if(!negative_child)
						negative_child = new TreeNode(depth + 1);

					negative_child->addTri(tris[i]);
				}
				else
					leaftris.push_back(tris[i]);
			}
			else
			{
				leaftris.push_back(tris[i]);
			}
		}
		else
		{
			if(tris[i].getVertex(1)[dividing_axis] > average)
			{
				if(tris[i].getVertex(2)[dividing_axis] > average)
				{
					if(!positive_child)
						positive_child = new TreeNode(depth + 1);

					positive_child->addTri(tris[i]);
				}
				else
					leaftris.push_back(tris[i]);
			}
			else
			{
				leaftris.push_back(tris[i]);
			}
		}
	}

	//tell children to build their lists.

	if(positive_child)
		positive_child->build();

	if(negative_child)
		negative_child->build();*/

	/*if(positive_child->getNumTris() > 0)
	{
		positive_child->build();
	}
	else
	{
		delete positive_child;
		positive_child = NULL;
	}


	if(negative_child->getNumTris() > 0)
	{
		negative_child->build();
	}
	else
	{
		delete negative_child;
		negative_child = NULL;
	}*/

}

class StackFrame
{
public:
	StackFrame(){}
	StackFrame(TreeNode* node_, float tmin_, float tmax_)
		:	node(node_), tmin(tmin_), tmax(tmax_) {}

	TreeNode* node;
	float tmin;
	float tmax;
};

float TreeNode::traceRayIterative(const ::Ray& ray, js::Triangle*& hittri_out)
{

	const float aabb_hitdist = rayAABBTrace(ray, this->min, this->max);

	if(aabb_hitdist < 0.0f)
		return -1.0f;


	//float tmin = 0.0f;
	//float tmax = 1e9f;

	hittri_out = NULL;
	//float closest_dist = 2.0e9f;

	const Vec3 recip_unitraydir(1.0f / ray.unitdir.x, 1.0f / ray.unitdir.y, 1.0f / ray.unitdir.z);


	std::vector<StackFrame> nodestack;
	nodestack.reserve(50);

	nodestack.push_back(StackFrame(this, aabb_hitdist, 1e9f));

	StackFrame frame;
	while(!nodestack.empty())
	{
		frame = nodestack.back();
		nodestack.pop_back();

		TreeNode* current = frame.node;

		while(current->negative_child)
		{
			//while current node is not a leaf..

			//if have hit a tri before the AABB of this node, then ignore it.
			//if(closest_dist <= frame.tmin)
			//	continue;
		
			nodes_touched++;

		
			//process children of this node

			//signed dist to split plane along ray direction from ray origin
			const float axis_sgnd_dist = current->dividing_val - ray.startpos[current->dividing_axis];
			const float dist_to_split = axis_sgnd_dist * recip_unitraydir[current->dividing_axis];/// ray.unitdir[current->dividing_axis];
			//const float dist_to_split = ()
			//							/ ray.unitdir[current->dividing_axis];

			//nearnode is the halfspace that the ray origin is in
			TreeNode* nearnode;
			TreeNode* farnode;

			if(axis_sgnd_dist > 0.0f)
			{
				nearnode = current->negative_child;
				farnode = current->positive_child;
			}
			else
			{
				nearnode = current->positive_child;
				farnode = current->negative_child;
			}

			//if(dist_to_split > tmax && dist_to_split < 0.0f)
			if(dist_to_split < 0.0f && dist_to_split < frame.tmax)
			{
				//whole interval is on near cell	
				
				//return nearnode->traceRay(ray, tmin, tmax, hittri_out);	
				//nodestack.push_back(StackFrame(nearnode, frame.tmin, frame.tmax));
				current = nearnode;
			}
			else
			{
				if(frame.tmin > dist_to_split)
				{
					//whole interval is on far cell.
					//return farnode->traceRay(ray, tmin, tmax, hittri_out);
					//nodestack.push_back(StackFrame(farnode, frame.tmin, frame.tmax));
					current = farnode;
				}
				else
				{
					//ray hits plane - double recursion, into both near and far cells.
								
					//if(dist_to_split < closest_dist)
					//{
						//process far tri
						nodestack.push_back(StackFrame(farnode, dist_to_split, frame.tmax));
					//}
					
					//push near tri onto stack second so it gets processed first.
					//process near tri
					//nodestack.push_back(StackFrame(nearnode, frame.tmin, dist_to_split));
					current = nearnode;			

					frame.tmax = dist_to_split;
				}
			}

		}//end while current node is not a leaf..

		//'current' is a leaf node..

		//------------------------------------------------------------------------
		//intersect with leaf tris
		//------------------------------------------------------------------------
		//float dist = 1e9f;
		//bool hitleaf = false;
		float closest_dist = 2.0e9f;
		for(int i=0; i<current->leaftris.size(); ++i)
		{
			const float tridist = current->leaftris[i]->traceRay(ray);

			if(tridist >= 0.0f && tridist < closest_dist)
			{
				//hitleaf = true;
				closest_dist = tridist;
				hittri_out = current->leaftris[i];
			}
		}

		//if(!hitleaf)
		//	dist = -1.0f;

		//return dist;

		if(closest_dist < 1e9f)//if hit a leaf tri
			return closest_dist;

	}//end while !nodestack.empty()

	return -1.0f;//missed all tris

	/*if(closest_dist > 1e9f)
		return -1.0f;
	else
		return closest_dist;*/
}


float TreeNode::traceRay(const ::Ray& ray, float tmin, float tmax, 
						 js::Triangle*& hittri_out)
{
	hittri_out = NULL;

	nodes_touched++;

	//tmin is the dist along the ray that the current testing interval starts at
	//tmax is the dist along the ray that the current testing interval ends at

	assert((!negative_child && !positive_child) || negative_child && positive_child);

	if(!negative_child)
	{
		//this is a leaf node..

		//------------------------------------------------------------------------
		//intersect with leaf tris
		//------------------------------------------------------------------------
		float dist = 1e9f;
		bool hitleaf = false;
		for(int i=0; i<leaftris.size(); ++i)
		{
			const float tridist = leaftris[i]->traceRay(ray);

			if(tridist >= 0.0f && tridist < dist)
			{
				hitleaf = true;
				dist = tridist;
				hittri_out = leaftris[i];
			}
		}

		if(!hitleaf)
			dist = -1.0f;

		return dist;
	}

	//else this is not a leaf node...
	assert(negative_child && positive_child);

	//signed dist to split plane along ray direction from ray origin
	const float dist_to_split = (dividing_val - ray.startpos[dividing_axis])
								/ ray.unitdir[dividing_axis];

	//nearnode is the halfspace that the ray origin is in
	TreeNode* nearnode;
	TreeNode* farnode;

	if(ray.startpos[dividing_axis] < dividing_val)
	{
		nearnode = negative_child;
		farnode = positive_child;
	}
	else
	{
		nearnode = positive_child;
		farnode = negative_child;
	}

	//if(dist_to_split > tmax && dist_to_split < 0.0f)
	if(dist_to_split < 0.0f && dist_to_split < tmax)
	{
		//whole interval is on near cell		
		return nearnode->traceRay(ray, tmin, tmax, hittri_out);	 
	}
	else
	{
		if(tmin > dist_to_split)
		{
			//whole interval is on far cell.
			return farnode->traceRay(ray, tmin, tmax, hittri_out);
		}
		else
		{
			//ray hits plane - double recursion, into both near and far cells.
			js::Triangle* near_hit_tri = NULL;
			const float neardist = nearnode->traceRay(ray, tmin, dist_to_split, near_hit_tri);

			if(neardist >= 0.0f)//if hit something in near cell
			{
				hittri_out = near_hit_tri;
				return neardist;
			}
			else
			{
				//no hit in near cell, so test far cell
				return farnode->traceRay(ray, dist_to_split, tmax, hittri_out);
			}
		}
	}





/*	//------------------------------------------------------------------------
	//trace against straddling tris
	//------------------------------------------------------------------------
	float dist = 1e9f;
	bool hitleaf = false;
	for(int i=0; i<leaftris.size(); ++i)
	{
		const float tridist = leaftris[i].traceRay(ray);

		if(tridist >= 0.0f && tridist < dist)
		{
			hitleaf = true;
			dist = tridist;
		}
	}

	if(!hitleaf)
		dist = -1.0f;


	//signed dist to split plane along ray direction from ray origin
	const float dist_to_split = (dividing_val - ray.startpos[dividing_axis])
								/ ray.unitraydir[dividing_axis];

	//nearnode is the halfspace that the ray origin is in
	TreeNode* nearnode;
	TreeNode* farnode;

	if(ray.startpos[dividing_axis] < dividing_val)
	{
		nearnode = negative_child;
		farnode = positive_child;
	}
	else
	{
		nearnode = positive_child;
		farnode = negative_child;
	}

	//if(dist_to_split > tmax && dist_to_split < 0.0f)
	if(dist_to_split < 0.0f && dist_to_split < tmax)
	{
		//whole interval is on near cell
		
		if(!nearnode)
			return dist;

		const float neardist = nearnode->traceRay(ray, tmin, tmax);

		if(neardist >= 0.0f)//if hit something in near cell
		{
			if(dist >= 0.0f)//if hit a straddling tri
				return myMin(neardist, dist);
			else
				return neardist;
		}
		else
			return dist;	 
	}
	else
	{
		if(tmin > dist_to_split)
		{
			//whole interval is on far cell.
		
			if(!farnode)
				return dist;

			const float fardist = farnode->traceRay(ray, tmin, tmax);

			if(fardist >= 0.0f)//if hit something in far cell
			{
				if(dist >= 0.0f)//if hit a straddling tri
					return myMin(fardist, dist);
				else
					return fardist;
			}
			else
				return dist;
		}
		else
		{
			//ray hits plane - double recursion, into both near and far cells.
			float neardist = -1.0f;

			if(nearnode)
				neardist = nearnode->traceRay(ray, tmin, dist_to_split);

			if(neardist >= 0.0f)//if hit something in near cell
			{
				if(dist >= 0.0f)//if hit a straddling tri
					return myMin(neardist, dist);//return smallest hit dist
				else
					return neardist;
			}
			else
			{
				//no hit in near cell, so test far cell
				float fardist = -1.0f;

				if(farnode)
					fardist = farnode->traceRay(ray, dist_to_split, tmax);

				if(fardist >= 0.0f)//if hit something in far cell
				{
					if(dist >= 0.0f)//if hit a straddling tri
						return myMin(fardist, dist);
					else
						return fardist;
				}
				else
					return dist;
			}
		}
	}




	assert(0);



*/


	/*if(ray.startpos[dividing_axis] < dividing_val)
	{
		if(negative_child)
		{
			const float dist = negative_child->traceRay(ray);//trace thru negative halfspace

			if(dist >= 0.0f)//if hit something
				return dist;
		}

		if(ray.unitraydir[dividing_axis] > 0.0f)
		{
			//if heading to positive half
			if(positive_child)
			{
				const float dist = positive_child->traceRay(ray);//trace thru positive half

				if(dist >= 0.0f)//if hit something
					return dist;
			}
		}

		return -1.0f;
	}*/







}

void TreeNode::draw() const
{
#ifdef OPENGL_DRAWABLE
	if(!positive_child)
	{
		//this is a leaf node
		return;
	}

	glBegin(GL_QUADS);

	Vec3 normal(0.0f, 0.0f, 0.0f);
	normal[dividing_axis] = 1.0f;

	glNormal3fv(normal.data());

	const float ALPHA = 0.2f;

	if(dividing_axis == 0)
	{
		glColor4f(1.0f, 0.0f, 0.0f, ALPHA);
		glVertex3f(dividing_val, min.y, max.z);
		glVertex3f(dividing_val, max.y, max.z);
		glVertex3f(dividing_val, max.y, min.z);
		glVertex3f(dividing_val, min.y, min.z);
	}
	else if(dividing_axis == 1)
	{
		glColor4f(0.0f, 1.0f, 0.0f, ALPHA);
		glVertex3f(min.x, dividing_val, max.z);
		glVertex3f(max.x, dividing_val, max.z);
		glVertex3f(max.x, dividing_val, min.z);
		glVertex3f(min.x, dividing_val, min.z);
	}
	else if(dividing_axis == 2)
	{
		glColor4f(0.0f, 0.0f, 1.0f, ALPHA);
		glVertex3f(min.x, max.y, dividing_val);
		glVertex3f(max.x, max.y, dividing_val);
		glVertex3f(max.x, min.y, dividing_val);
		glVertex3f(min.x, min.y, dividing_val);
	}
	else
	{
		assert(0);
	}


	
	glEnd();



	if(positive_child)
		positive_child->draw();

	if(negative_child)
		negative_child->draw();

#endif
}


int TreeNode::getNumLeafTrisInSubtree() const
{
	if(positive_child)
	{
		assert(negative_child);
		return positive_child->getNumLeafTrisInSubtree() + 
				negative_child->getNumLeafTrisInSubtree();
	}
	else
	{
		return this->leaftris.size();
	}
}

 int TreeNode::nodes_touched = 0;


} //end namespace js






