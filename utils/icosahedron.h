/*=====================================================================
icosahedron.h
-------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Mar 04 20:29:28 +1300 2011
=====================================================================*/
#pragma once


#include <map>
#include <stack>
#include <vector>
#include <utility>

#include "../maths/vec3.h"

/*=====================================================================
icosahedron
-----------

=====================================================================*/
class icosahedron
{
public:
	icosahedron(uint32 num_subdivs = 0) { setSubdivision(num_subdivs); }

	std::vector<Vec3f> vertices;
	std::vector<uint32> indices;

	void setSubdivision(const uint32 num_subdivs) { icosahedralSubdivision(num_subdivs, vertices, indices); }

	static void icosahedralSubdivision(uint32 num_subdivs, std::vector<Vec3f>& sphere_vertices_out, std::vector<uint32>& sphere_indices_out)
	{
		const double p = 0.61803398874989484820458683436564; // 1 / phi
		const double icosahedron_verts[] =
		{
			 0, -p,  1,  1,  0,  p,  1,  0, -p, -1,  0, -p,
			-1,  0,  p, -p,  1,  0,  p,  1,  0,  p, -1,  0,
			-p, -1,  0,  0, -p, -1,  0,  p, -1,  0,  p,  1
		};
		const uint icosahedron_indices[] =
		{
			 1,  2,  6,  1,  7,  2,  3,  4,  5,  4,  3,  8,
			 6,  5, 11,  5,  6, 10,  9, 10,  2, 10,  9,  3,
			 7,  8,  9,  8,  7,  0, 11,  0,  1,  0, 11,  4,
			 6,  2, 10,  1,  6, 11,  3,  5, 10,  5,  4, 11,
			 2,  7,  9,  7,  1,  0,  3,  9,  8,  4,  8,  0
		};

		std::vector<subdiv_tri> triangles_out;
		std::stack<std::pair<subdiv_tri, uint32> > triangle_stack;
		for(uint32 i = 0; i < 20; ++i) // for each face of the icosahedron
		{
			const uint32 v0 = icosahedron_indices[i * 3], v1 = icosahedron_indices[i * 3 + 1], v2 = icosahedron_indices[i * 3 + 2];
			const subdiv_tri t_orig(normalise(Vec3d(icosahedron_verts[v0*3], icosahedron_verts[v0*3+1], icosahedron_verts[v0*3+2])),
									normalise(Vec3d(icosahedron_verts[v1*3], icosahedron_verts[v1*3+1], icosahedron_verts[v1*3+2])),
									normalise(Vec3d(icosahedron_verts[v2*3], icosahedron_verts[v2*3+1], icosahedron_verts[v2*3+2])));

			triangle_stack.push(std::pair<subdiv_tri, uint32>(t_orig, 0));
			while(!triangle_stack.empty())
			{
				const std::pair<subdiv_tri, uint32> current(triangle_stack.top()); triangle_stack.pop();

				if(current.second >= num_subdivs) triangles_out.push_back(current.first);
				else // subdivide and recurse
				{
					const subdiv_tri
						t0(current.first),
						t1(normalise(t0.v[0] + t0.v[1]), normalise(t0.v[1] + t0.v[2]), normalise(t0.v[2] + t0.v[0]));

					triangle_stack.push(std::make_pair(subdiv_tri(t1.v[1], t1.v[2], t1.v[0]), current.second + 1));
					triangle_stack.push(std::make_pair(subdiv_tri(t0.v[0], t1.v[0], t1.v[2]), current.second + 1));
					triangle_stack.push(std::make_pair(subdiv_tri(t0.v[1], t1.v[1], t1.v[0]), current.second + 1));
					triangle_stack.push(std::make_pair(subdiv_tri(t0.v[2], t1.v[2], t1.v[1]), current.second + 1));
				}
			}
		}

		sphere_vertices_out.clear();
		sphere_indices_out.clear();

		std::map<Vec3d, uint32> shared_vertices;
		for(std::vector<subdiv_tri>::const_iterator t = triangles_out.begin(); t != triangles_out.end(); ++t)
		for(int z = 0; z < 3; ++z)
		{
			const std::map<Vec3d, uint32>::iterator i = shared_vertices.find(t->v[z]);
			if(i == shared_vertices.end())
			{
				const uint32 vertex_idx = (uint32)sphere_vertices_out.size();
				shared_vertices.insert(std::pair<Vec3d, uint32>(t->v[z], vertex_idx));
				sphere_vertices_out.push_back(Vec3f((float)t->v[z].x, (float)t->v[z].y, (float)t->v[z].z));

				sphere_indices_out.push_back(vertex_idx);
			}
			else sphere_indices_out.push_back(i->second);
		}
	}

private:

	class subdiv_tri
	{
	public:
		subdiv_tri(const Vec3d v0_, const Vec3d v1_, const Vec3d v2_) { v[0] = v0_; v[1] = v1_; v[2] = v2_; }
		Vec3d v[3];
	};
};
