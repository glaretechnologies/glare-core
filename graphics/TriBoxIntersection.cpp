/*=====================================================================
TriBoxIntersection.cpp
----------------------
File created by ClassTemplate on Fri Mar 21 16:49:56 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "TriBoxIntersection.h"

#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"


#if 0
void TriBoxIntersection::clipPolyAgainstPlane(const Vec3f* points, unsigned int num_points, unsigned int plane_axis, float d, float normal, Vec3f* points_out, unsigned int& num_points_out)
{
	assert(plane_axis < 3);

	num_points_out = 0;
	
	/*bool prev_inside = points[0][plane_axis] * normal <= d;
	if(prev_inside)
		points_out[num_points_out++] = points[0]; // Write it to output vertex list

	// For each edge...
	for(unsigned int edge=1; edge<num_points; ++edge)
	{
		const Vec3f& prev = points[edge - 1];
		const Vec3f& current = points[edge];

		if(current[plane_axis] * normal <= d)
		{
			// If current vertex is inside...
			points_out[num_points_out++] = current; // Write it to output vertex list

			if(!prev_inside) // If previous vertex was outside...
			{
				// Output intersection point
				const float frac = (d - prev[plane_axis] * normal) / (current[plane_axis] * normal - prev[plane_axis] * normal);
				assert(Maths::inRange(frac, 0.0f, 1.0f));
				points_out[num_points_out++] = prev + (current - prev) * frac;
			}

			prev_inside = true;
		}
		else
		{
			// Current is outside
			
			if(prev_inside) // If previous vertex was inside...
			{
				// Output intersection point
				const float frac = (d - prev[plane_axis] * normal) / (current[plane_axis] * normal - prev[plane_axis] * normal);
				assert(Maths::inRange(frac, 0.0f, 1.0f));
				points_out[num_points_out++] = prev + (current - prev) * frac;
			}

			prev_inside = false;
		}
	}*/

	/*float current_dist;
	float next_dist = points[0][plane_axis] * normal - d;

	// For each edge...
	for(unsigned int edge=0; edge<num_points; ++edge)
	{
		const Vec3f& current = points[edge];
		const Vec3f& next = points[(edge + 1) % num_points];

		current_dist = next_dist;
		next_dist = next[plane_axis] * normal - d;

		if(current_dist <= 0.0f)
		{
			// If current vertex is inside...
			points_out[num_points_out++] = current; // Write it to output vertex list

			if(next_dist > 0.0f) // If next vertex is outside...
			{
				// Output intersection point
				const float frac = current_dist / (current_dist - next_dist); //(d - current[plane_axis] * normal) / (next[plane_axis] * normal - current[plane_axis] * normal);
				assert(Maths::inRange(frac, 0.0f, 1.0f));
				points_out[num_points_out++] = current + (next - current) * frac;
			}
		}
		else
		{
			// Current is outside
			
			if(next_dist <= 0.0f) //next[plane_axis] * normal <= d) // If next vertex is inside...
			{
				// Output intersection point
				const float frac = current_dist / (current_dist - next_dist); // const float frac = (d - current[plane_axis] * normal) / (next[plane_axis] * normal - current[plane_axis] * normal);
				assert(Maths::inRange(frac, 0.0f, 1.0f));
				points_out[num_points_out++] = current + (next - current) * frac;
			}
		}
	}*/

	// For each edge...
	for(unsigned int edge=0; edge<num_points; ++edge)
	{
		const Vec3f& current = points[edge];
		const Vec3f& next = points[(edge + 1) % num_points];

		if(current[plane_axis] * normal <= d)
		{
			// If current vertex is inside...
			points_out[num_points_out++] = current; // Write it to output vertex list

			if(next[plane_axis] * normal > d) // If next vertex is outside...
			{
				// Output intersection point
				const float frac = (d - current[plane_axis] * normal) / (next[plane_axis] * normal - current[plane_axis] * normal);
				assert(Maths::inRange(frac, 0.0f, 1.0f));
				points_out[num_points_out++] = current + (next - current) * frac;
			}
		}
		else
		{
			// Current is outside
			
			if(next[plane_axis] * normal <= d) // If next vertex is inside...
			{
				// Output intersection point
				const float frac = (d - current[plane_axis] * normal) / (next[plane_axis] * normal - current[plane_axis] * normal);
				assert(Maths::inRange(frac, 0.0f, 1.0f));
				points_out[num_points_out++] = current + (next - current) * frac;
			}
		}
	}
}
#endif


void TriBoxIntersection::clipPolyToPlaneHalfSpace(const Planef& plane, const std::vector<Vec3f>& polygon_verts, std::vector<Vec3f>& polygon_verts_out)
{
	polygon_verts_out.resize(0);
	if(polygon_verts.size() == 0)
		return;

	float current_dist;
	float next_dist = plane.signedDistToPoint(polygon_verts[0].toVec4fPoint());

	// For each edge...
	for(unsigned int edge=0; edge<polygon_verts.size(); ++edge)
	{
		const Vec3f& current = polygon_verts[edge];
		const Vec3f& next = polygon_verts[(edge + 1) % polygon_verts.size()];

		current_dist = next_dist;
		next_dist = plane.signedDistToPoint(next.toVec4fPoint());
		assert(epsEqual(current_dist, plane.signedDistToPoint(current.toVec4fPoint())));
		assert(epsEqual(next_dist, plane.signedDistToPoint(next.toVec4fPoint())));

		if(current_dist <= 0.0f)
		{
			// If current vertex is inside...
			polygon_verts_out.push_back(current); // Write it to output vertex list

			if(next_dist > 0.0f) // If next vertex is outside...
			{
				// Output intersection point
				const float t = current_dist / (current_dist - next_dist);
				polygon_verts_out.push_back(lerp(current, next, t));
			}
		}
		else
		{
			// Current is outside
			
			if(next_dist <= 0.0f) // If next vertex is inside...
			{
				// Output intersection point
				const float t = current_dist / (current_dist - next_dist);
				polygon_verts_out.push_back(lerp(current, next, t));
			}
		}
	}
}

void TriBoxIntersection::clipPolyToPlaneHalfSpaces(const std::vector<Planef>& planes, const std::vector<Vec3f>& polygon_verts, std::vector<Vec3f>& temp_vert_buffer, std::vector<Vec3f>& polygon_verts_out)
{
	temp_vert_buffer = polygon_verts;
	for(unsigned int i=0; i<planes.size(); ++i)
	{
		clipPolyToPlaneHalfSpace(planes[i], temp_vert_buffer, polygon_verts_out);
		temp_vert_buffer = polygon_verts_out;
	}
}


/*void TriBoxIntersection::clipPolyToPlaneHalfSpace(const Plane<float>& plane, const Vec3f* points, unsigned int num_points, unsigned int max_num_points_out, Vec3f* points_out, unsigned int& num_points_out)
{
	num_points_out = 0;
	if(num_points == 0)
		return;

	float current_dist;
	float next_dist = plane.signedDistToPoint(points[0]);

// For each edge...
	for(unsigned int edge=0; edge<num_points; ++edge)
	{
		const Vec3f& current = points[edge];
		const Vec3f& next = points[(edge + 1) % num_points];

		//const float current_dist = plane.signedDistToPoint(current);
		//const float next_dist = plane.signedDistToPoint(next);
		current_dist = next_dist;
		next_dist = plane.signedDistToPoint(next);
		assert(current_dist == plane.signedDistToPoint(current));
		assert(next_dist == plane.signedDistToPoint(next));

		if(current_dist <= 0.0f) //current[plane_axis] * normal <= d)
		{
			assert(num_points_out < max_num_points_out);

			// If current vertex is inside...
			points_out[num_points_out++] = current; // Write it to output vertex list

			if(next_dist > 0.0f) // next[plane_axis] * normal > d) // If next vertex is outside...
			{
				// Output intersection point
				//const float frac = (d - current[plane_axis] * normal) / (next[plane_axis] * normal - current[plane_axis] * normal);
				const float t = current_dist / (current_dist - next_dist);
				assert(num_points_out < max_num_points_out);
				points_out[num_points_out++] = lerp(current, next, t); //current + (next - current) * frac;
			}
		}
		else
		{
			// Current is outside
			
			if(next_dist <= 0.0f) // next[plane_axis] * normal <= d) // If next vertex is inside...
			{
				// Output intersection point
				//const float frac = (d - current[plane_axis] * normal) / (next[plane_axis] * normal - current[plane_axis] * normal);
				const float t = current_dist / (current_dist - next_dist);
				assert(num_points_out < max_num_points_out);
				points_out[num_points_out++] = lerp(current, next, t); //current + (next - current) * frac;
			}
		}
	}
}*/


#if 0
void TriBoxIntersection::slowGetClippedTriAABB(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out)
{
	Vec3f v_a[9] = {v0, v1, v2};	
	Vec3f v_b[9];

	unsigned int current_num_verts = 3;
	for(unsigned int axis=0; axis<3; ++axis)
	{
		TriBoxIntersection::clipPolyAgainstPlane(v_a, current_num_verts, axis, -aabb.min_[axis], -1.0f, v_b, current_num_verts);
		assert(current_num_verts <= 9);
		TriBoxIntersection::clipPolyAgainstPlane(v_b, current_num_verts, axis, aabb.max_[axis], 1.0f, v_a, current_num_verts);
		assert(current_num_verts <= 9);
	}
	
	assert(current_num_verts <= 9);

	// Ok, so final polygon vertices are in v_a.

	// Compute bounds of polygon
	//clipped_tri_aabb_out.min_ = v_a[0];
	//clipped_tri_aabb_out.max_ = v_a[0];
	const SSE_ALIGN PaddedVec3f v_a_0 = v_a[0];

	__m128 clipped_aabb_min = _mm_load_ps(&v_a_0.x);
	__m128 clipped_aabb_max = clipped_aabb_min;

	for(unsigned int i=1; i<current_num_verts; ++i)
	{
		const SSE_ALIGN PaddedVec3f v = v_a[i];
		//clipped_tri_aabb_out.enlargeToHoldAlignedPoint(v);
		clipped_aabb_min = _mm_min_ps(clipped_aabb_min, _mm_load_ps(&v.x));
		clipped_aabb_max = _mm_max_ps(clipped_aabb_max, _mm_load_ps(&v.x));
	}

	// Make sure the clipped_tri_aabb_out doesn't extend past aabb, due to numerical errors
	_mm_store_ps(
		&clipped_tri_aabb_out.min_.x,
		_mm_max_ps(
			clipped_aabb_min,
			_mm_load_ps(&aabb.min_.x)
			)
		);
	_mm_store_ps(
		&clipped_tri_aabb_out.max_.x,
		_mm_min_ps(
			clipped_aabb_max,
			_mm_load_ps(&aabb.max_.x)
			)
		);
}







class Edge
{
public:
	SSE_ALIGN PaddedVec3f start;
	SSE_ALIGN PaddedVec3f end;
};


void checkEdges(Edge* edges)
{
	assert(PaddedVec3f::epsEqual(edges[0].end, edges[1].start));
	assert(PaddedVec3f::epsEqual(edges[1].end, edges[2].start));
	assert(PaddedVec3f::epsEqual(edges[2].end, edges[0].start));
}

void TriBoxIntersection::getClippedTriAABB(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out)
{
	assert(aabb.invariant());
	
	SSE_ALIGN Edge edges[3];

	edges[0].start = v0;
	edges[0].end = v1;

	edges[1].start = v1;
	edges[1].end = v2;

	edges[2].start = v2;
	edges[2].end = v0;

	checkEdges(edges);


	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		int fully_outside_edge_index = -1; // There should be at most 1 edges fully outside the plane

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
		
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside halfspace
			{
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					// Then edge straddles clipping plane.  So clip off edge outside plane, by moving the end point towards the start point.
					const SSE_ALIGN PaddedVec3f edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3f recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float t = (aabb.min_[axis] - edge.start[axis]) * recip_edge[axis];
					assert(Maths::inRange(t, 0.f, 1.f));

					//edge.end = edge.start + (edge.end - edge.start) * t;
					addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);

					assert(epsEqual(edge.end[axis], aabb.min_[axis])); // Check edge end lies on plane
				}
				else
				{
					// Both start and end are outside halfspace
					assert(fully_outside_edge_index == -1);
					fully_outside_edge_index = e;
				}
			}
			else
			{
				// Else end is inside halfspace

				if(edge.start[axis] < aabb.min_[axis]) // If start is outside
				{
					const SSE_ALIGN PaddedVec3f edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3f recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * recip_edge[axis];
					assert(Maths::inRange(neg_t, -1.f, 0.f));

					//edge.start = edge.end + (edge.end - edge.start) * neg_t;
					addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);

					assert(epsEqual(edge.start[axis], aabb.min_[axis])); // Check edge start lies on plane
				}
				// Else both start and end are inside, no clipping needed
			}
		}

		if(fully_outside_edge_index != -1)
		{
			Edge& edge = edges[fully_outside_edge_index];

			assert(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis]);
			
			// Move the edge, by using the new vertices from the prev and next edges
			/*const Edge& next_edge = edges[(fully_outside_edge_index + 1) % 3];
			const Edge& prev_edge = edges[(fully_outside_edge_index + 2) % 3];
			edge.start = prev_edge.end;
			edge.end = next_edge.start;*/
			edge.start[axis] = aabb.min_[axis];
			edge.end[axis] = aabb.min_[axis];

			assert(epsEqual(edge.start[axis], aabb.min_[axis]) && epsEqual(edge.end[axis], aabb.min_[axis])); // Check edge lies on plane	
		}
		//checkEdges(edges);

		/// Clip against max plane ///

		fully_outside_edge_index = -1;

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];

			if(edge.end[axis] > aabb.max_[axis]) // If end is outside halfspace
			{
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					// Get edgevec
					const SSE_ALIGN PaddedVec3f edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3f recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float t = (aabb.max_[axis] - edge.start[axis]) * recip_edge[axis];
					assert(Maths::inRange(t, 0.f, 1.f));

					//edge.end = edge.start + (edge.end - edge.start) * t;
					addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);

					assert(epsEqual(edge.end[axis], aabb.max_[axis])); // Check edge end lies on plane
				}
				else
				{
					assert(fully_outside_edge_index == -1);
					fully_outside_edge_index = e;
				}
			}
			else
			{
				// Else end is inside halfspace
			
				if(edge.start[axis] > aabb.max_[axis]) // If start is outside
				{
					// Get edgevec
					const SSE_ALIGN PaddedVec3f edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3f recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * recip_edge[axis];
					assert(Maths::inRange(neg_t, -1.f, 0.f));

					//edge.start = edge.end + (edge.end - edge.start) * neg_t;
					addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);

					assert(epsEqual(edge.start[axis], aabb.max_[axis])); // Check edge start lies on plane
				}
			}
		}

		if(fully_outside_edge_index != -1)
		{
			Edge& edge = edges[fully_outside_edge_index];

			assert(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis]);

			// Move the edge, by using the new vertices from the prev and next edges
			/*const Edge& next_edge = edges[(fully_outside_edge_index + 1) % 3];
			const Edge& prev_edge = edges[(fully_outside_edge_index + 2) % 3];
			edge.start = prev_edge.end;
			edge.end = next_edge.start;*/

			edge.start[axis] = aabb.min_[axis];
			edge.end[axis] = aabb.min_[axis];

			assert(epsEqual(edge.start[axis], aabb.max_[axis]) && epsEqual(edge.end[axis], aabb.max_[axis])); // Check edge lies on plane	
		}
		//checkEdges(edges);
	}
/*SSE_ALIGN Edge edges[3];
	{
	edges[0].start = v0;
	edges[0].end = v1;
	SSE_ALIGN PaddedVec3f edgevec = v1 - v0;
	reciprocalSSE(&edgevec.x, &edges[0].recip_edge.x);

	edges[1].start = v1;
	edges[1].end = v2;
	edgevec = v2 - v1;
	reciprocalSSE(&edgevec.x, &edges[1].recip_edge.x);

	edges[2].start = v2;
	edges[2].end = v0;
	edgevec = v0 - v2;
	reciprocalSSE(&edgevec.x, &edges[2].recip_edge.x);
	}

	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_edge[axis];
					
					//const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					//addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);

					edge.end = edge.start + (edge.end - edge.start) * t;

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
			
			if(edge.start[axis] < aabb.min_[axis]) // If start is outside
				if(edge.end[axis] >= aabb.min_[axis]) // If end is inside
				{
					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_edge[axis];
					
					//const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					//addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);

					edge.start = edge.end + (edge.end - edge.start) * neg_t;

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				//edge.unit_edge = normalise(edge.end - edge.start);
				const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
				reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
			}
		}

		/// Clip against max plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] > aabb.max_[axis]) // If end is outside
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					const float t = (aabb.max_[axis] - edge.start[axis]) * edge.recip_edge[axis];
					
					edge.end = edge.start + (edge.end - edge.start) * t;
					
					//addScaledVec4SSE(&edge.start.x, &edge.unit_edge.x, t, &edge.end.x);

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
			
			if(edge.start[axis] > aabb.max_[axis]) // If start is outside
				if(edge.end[axis] <= aabb.max_[axis]) // If end is inside
				{
					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * edge.recip_edge[axis];
					
					edge.start = edge.end + (edge.end - edge.start) * neg_t;
					
					//addScaledVec4SSE(&edge.end.x, &edge.unit_edge.x, neg_t, &edge.start.x);

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
				reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
			}
		}
	}*/
/*
	SSE_ALIGN Edge edges[3];
	// Fill in edges
	edges[0].start = v0;
	edges[0].end = v1;
	//edges[0].unit_edge = normalise(v1 - v0);
	//edges[0].recip_unit_edge.set(1.0f / edges[0].unit_edge.x, 1.0f / edges[0].unit_edge.y, 1.0f / edges[0].unit_edge.z);
	SSE_ALIGN PaddedVec3 edge = v1 - v0;
	reciprocalSSE(&edges[0].unit_edge.x, &edges[0].recip_unit_edge.x);

	edges[1].start = v1;
	edges[1].end = v2;
	edges[1].unit_edge = normalise(v2 - v1);
	//edges[1].recip_unit_edge.set(1.0f / edges[1].unit_edge.x, 1.0f / edges[1].unit_edge.y, 1.0f / edges[1].unit_edge.z);
	reciprocalSSE(&edges[1].unit_edge.x, &edges[1].recip_unit_edge.x);

	edges[2].start = v2;
	edges[2].end = v0;
	edges[2].unit_edge = normalise(v0 - v2);
	//edges[2].recip_unit_edge.set(1.0f / edges[2].unit_edge.x, 1.0f / edges[2].unit_edge.y, 1.0f / edges[2].unit_edge.z);
	reciprocalSSE(&edges[2].unit_edge.x, &edges[2].recip_unit_edge.x);

	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_unit_edge[axis];
					//edge.end = edge.start + edge.unit_edge * t;
					addScaledVec4SSE(&edge.start.x, &edge.unit_edge.x, t, &edge.end.x);
				}
			
			if(edge.start[axis] < aabb.min_[axis]) // If start is outside
				if(edge.end[axis] >= aabb.min_[axis]) // If end is inside
				{
					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_unit_edge[axis];
					//edge.start = edge.end + edge.unit_edge * neg_t;
					addScaledVec4SSE(&edge.end.x, &edge.unit_edge.x, neg_t, &edge.start.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				edge.unit_edge = normalise(edge.end - edge.start);
				reciprocalSSE(&edge.unit_edge.x, &edge.recip_unit_edge.x);
			}
		}

		/// Clip against max plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] > aabb.max_[axis]) // If end is outside
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					const float t = (aabb.max_[axis] - edge.start[axis]) * edge.recip_unit_edge[axis];
					//edge.end = edge.start + edge.unit_edge * t;
					addScaledVec4SSE(&edge.start.x, &edge.unit_edge.x, t, &edge.end.x);
				}
			
			if(edge.start[axis] > aabb.max_[axis]) // If start is outside
				if(edge.end[axis] <= aabb.max_[axis]) // If end is inside
				{
					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * edge.recip_unit_edge[axis];
					//edge.start = edge.end + edge.unit_edge * neg_t;
					addScaledVec4SSE(&edge.end.x, &edge.unit_edge.x, neg_t, &edge.start.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				edge.unit_edge = normalise(edge.end - edge.start);
				reciprocalSSE(&edge.unit_edge.x, &edge.recip_unit_edge.x);
			}
		}
	}
*/

	// Build final AABB
	/*clipped_tri_aabb_out.min_ = edges[0].start;
	clipped_tri_aabb_out.max_ = edges[0].start;

	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[0].end);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[1].start);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[1].end);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[2].start);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[2].end);*/



	__m128 clipped_aabb_min = _mm_load_ps(&edges[0].start.x);
	__m128 clipped_aabb_max = clipped_aabb_min;

	__m128 v = _mm_load_ps(&edges[0].end.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);

	v = _mm_load_ps(&edges[1].start.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);
	v = _mm_load_ps(&edges[1].end.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);

	v = _mm_load_ps(&edges[2].start.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);
	v = _mm_load_ps(&edges[2].end.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);

	// Make sure the clipped_tri_aabb_out doesn't extend past aabb, due to numerical errors
	/*_mm_store_ps(
		&clipped_tri_aabb_out.min_.x,
		_mm_max_ps(
			clipped_aabb_min,
			_mm_load_ps(&aabb.min_.x)
			)
		);
	_mm_store_ps(
		&clipped_tri_aabb_out.max_.x,
		//_mm_max_ps(
		//	clipped_aabb_min, // We want the max to be at least as large as the min bound.
			_mm_min_ps(
				clipped_aabb_max,
				_mm_load_ps(&aabb.max_.x)
				)
		//	)
		);*/

	_mm_store_ps(&clipped_tri_aabb_out.min_.x, clipped_aabb_min);
	_mm_store_ps(&clipped_tri_aabb_out.max_.x, clipped_aabb_max);



	assert(aabb.containsAABBox(clipped_tri_aabb_out));
	assert(clipped_tri_aabb_out.invariant());








/*


			Edge& edge = edges[e];
			Edge& next_edge = edges[(e + 1) % 3];
			Edge& prev_edge = edges[e == 0 ? 2 : e - 1];

			//Vec3f& edge_end = edge_ends[edge];
			//Vec3f& edge_start = edge_starts[edge];
			// Compute signed distance to plane for edge start vertex
			// Compute signed distance to plane for edge end vertex


			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Compute new start position for this edge
				const float prev_t = (aabb.min_[axis] - prev_edge.start[axis]) * prev_edge.recip_unit_edge[axis];
				edge.start = prev_edge.start + prev_edge.unit_edge * prev_t;

				// Compute new end position for this edge
				const float next_neg_t = (aabb.min_[axis] - next_edge.end[axis]) * next_edge.recip_unit_edge[axis];
				edge.end = next_edge.end + next_edge.unit_edge * next_neg_t;

				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				edge.unit_edge = normalise(edge.end - edge.start);
				edge.recip_unit_edge.set(1.0f / edge.unit_edge.x, 1.0f / edge.unit_edge.y, 1.0f / edge.unit_edge.z);
			}
			else if(edge.start[axis] >= aabb.min_[axis] && edge.end[axis] >= aabb.min_[axis]) // Else if both verts are inside plane
			{
				// just keep edge as it is
			}
			else if(edge.end[axis] >= aabb.min_[axis]) // Else end is outside
			{
				const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_unit_edge[axis];
				
				edge.end = edge.start + edge.unit_edge * t;
			}
			else // Else start is outside
			{
				const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_unit_edge[axis];
				
				edge.end = edge.start + edge.unit_edge * neg_t;
			}
		} // End for edge edge

		// max plane //
	}
*/




	/*clipped_tri_aabb_out = js::AABBox(
		Vec3f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
		Vec3f(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max())
		);

	const Vec3f verts[3] = {v0, v1, v2};

	for(int i=0; i<3; ++i)
	{
		const Vec3f& v_start = verts[i];
		const Vec3f& v_end = verts[(i + 1) % 3];

		const Vec3f edge = v_end - v_start;

		const Vec3f t_min(
			(aabb.min_.x - v_start.x) / edge.x,
			(aabb.min_.y - v_start.y) / edge.y,
			(aabb.min_.z - v_start.z) / edge.z
			);

		const Vec3f t_max(
			(aabb.max_.x - v_start.x) / edge.x,
			(aabb.max_.y - v_start.y) / edge.y,
			(aabb.max_.z - v_start.z) / edge.z
			);

		const Vec3f t_near(
			myMin(t_min.x, t_max.x),
			myMin(t_min.y, t_max.y),
			myMin(t_min.z, t_max.z)
			);

		const Vec3f t_far(
			myMax(t_min.x, t_max.x),
			myMax(t_min.y, t_max.y),
			myMax(t_min.z, t_max.z)
			);

		const float neg_inf = logf(0.0);
		const float pos_inf = -neg_inf;
	
		const float t_lower = myMax(
			isNAN(t_near.x) ? neg_inf : t_near.x, 
			isNAN(t_near.y) ? neg_inf : t_near.y,
			isNAN(t_near.z) ? neg_inf : t_near.z
			);
		const float t_upper = myMin(
			isNAN(t_far.x) ? pos_inf : t_far.x, 
			isNAN(t_far.y) ? pos_inf : t_far.y, 
			isNAN(t_far.z) ? pos_inf : t_far.z
			);

		if(t_lower <= t_upper)
		{
			// We have an intersection with the AABB
			SSE_ALIGN js::AABBox edge_intersection_aabb(v_start + edge * t_lower, v_start + edge * t_upper);
			clipped_tri_aabb_out.enlargeToHoldAABBox(edge_intersection_aabb);
		}
	}*/
}
#endif


#if 0

class Edge
{
public:
	SSE_ALIGN PaddedVec3 start;
	SSE_ALIGN PaddedVec3 end;
	//SSE_ALIGN PaddedVec3 recip_edge;
	//float edge_len_multiplier;
};


void TriBoxIntersection::getClippedTriAABB(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out)
{
	assert(aabb.invariant());


	/*SSE_ALIGN Edge edges[3];
	{
	edges[0].start = v0;
	edges[0].end = v1;
	SSE_ALIGN PaddedVec3 edgevec = v1 - v0;
	reciprocalSSE(&edgevec.x, &edges[0].recip_edge.x);
	edges[0].edge_len_multiplier = 1.0f;

	edges[1].start = v1;
	edges[1].end = v2;
	edgevec = v2 - v1;
	reciprocalSSE(&edgevec.x, &edges[1].recip_edge.x);
	edges[1].edge_len_multiplier = 1.0f;

	edges[2].start = v2;
	edges[2].end = v0;
	edgevec = v0 - v2;
	reciprocalSSE(&edgevec.x, &edges[2].recip_edge.x);
	edges[2].edge_len_multiplier = 1.0f;
	}


	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		int fully_outside_edge_index = -1;

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
		
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside
			{
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;

					const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_edge[axis] * edge.edge_len_multiplier;
					//edge.end = edge.start + (edge.end - edge.start) * t;
					addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);
					edge.edge_len_multiplier *= t;
				}
				else
					fully_outside_edge_index = e;
			}
			
			if(edge.start[axis] < aabb.min_[axis]) // If start is outside
			{
				if(edge.end[axis] >= aabb.min_[axis]) // If end is inside
				{
					SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;

					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_edge[axis] * edge.edge_len_multiplier;
					//edge.start = edge.end + (edge.end - edge.start) * neg_t;
					addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);
					edge.edge_len_multiplier *= -neg_t;
				}
				else
					fully_outside_edge_index = e;
			}
		}

		if(fully_outside_edge_index != -1)
		{
			Edge& edge = edges[fully_outside_edge_index];
			
			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(fully_outside_edge_index + 1) % 3];
				const Edge& prev_edge = edges[(fully_outside_edge_index + 2) % 3];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;

				SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
				reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				edge.edge_len_multiplier = 1.0f;
			}
		}

		/// Clip against max plane ///

		fully_outside_edge_index = -1;

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];

			if(edge.end[axis] > aabb.max_[axis]) // If end is outside
			{
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;

					const float t = (aabb.max_[axis] - edge.start[axis]) * edge.recip_edge[axis] * edge.edge_len_multiplier;				
					//edge.end = edge.start + (edge.end - edge.start) * t;
					addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);
					edge.edge_len_multiplier *= t;
				}
				else
					fully_outside_edge_index = e;
			}
			
			if(edge.start[axis] > aabb.max_[axis]) // If start is outside
			{
				if(edge.end[axis] <= aabb.max_[axis]) // If end is inside
				{
					// Get edgevec
					SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3 recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * edge.recip_edge[axis] * edge.edge_len_multiplier;
					//edge.start = edge.end + (edge.end - edge.start) * neg_t;
					addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);
					edge.edge_len_multiplier *= -neg_t;
				}
				else
					fully_outside_edge_index = e;
			}
		}

		if(fully_outside_edge_index != -1)
		{
			Edge& edge = edges[fully_outside_edge_index];
			
			// If both verts are outside plane...
			if(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(fully_outside_edge_index + 1) % 3];
				const Edge& prev_edge = edges[(fully_outside_edge_index + 2) % 3];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;

				SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
				reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				edge.edge_len_multiplier = 1.0f;
			}
		}
	}*/
	
	SSE_ALIGN Edge edges[3];
	{
	edges[0].start = v0;
	edges[0].end = v1;

	edges[1].start = v1;
	edges[1].end = v2;

	edges[2].start = v2;
	edges[2].end = v0;
	}


	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		int fully_outside_edge_index = -1; // There should be at most 1 edges fully outside the plane

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
		
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside
			{
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					// Then edge straddles clipping plane.  So clip off edge outside plane, by moving the end point towards the start point.
					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3 recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float t = (aabb.min_[axis] - edge.start[axis]) * recip_edge[axis];
					assert(Maths::inRange(t, 0.f, 1.f));

					//edge.end = edge.start + (edge.end - edge.start) * t;
					addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);
				}
				else
				{
					assert(fully_outside_edge_index == e || fully_outside_edge_index == -1);
					fully_outside_edge_index = e;
				}
			}
			
			if(edge.start[axis] < aabb.min_[axis]) // If start is outside
			{
				if(edge.end[axis] >= aabb.min_[axis]) // If end is inside
				{
					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3 recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * recip_edge[axis];
					assert(Maths::inRange(neg_t, -1.f, 0.f));

					//edge.start = edge.end + (edge.end - edge.start) * neg_t;
					addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);
				}
				else
				{
					assert(fully_outside_edge_index == e || fully_outside_edge_index == -1);
					fully_outside_edge_index = e;
				}
			}
		}

		if(fully_outside_edge_index != -1)
		{
			Edge& edge = edges[fully_outside_edge_index];

			assert(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis]);
			
			// If both verts are outside plane...
			//if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			//{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(fully_outside_edge_index + 1) % 3];
				const Edge& prev_edge = edges[(fully_outside_edge_index + 2) % 3];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
			//}
		}

		/// Clip against max plane ///

		fully_outside_edge_index = -1;

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];

			if(edge.end[axis] > aabb.max_[axis]) // If end is outside
			{
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					// Get edgevec
					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3 recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float t = (aabb.max_[axis] - edge.start[axis]) * recip_edge[axis];
					assert(Maths::inRange(t, 0.f, 1.f));

					//edge.end = edge.start + (edge.end - edge.start) * t;
					addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);
				}
				else
				{
					assert(fully_outside_edge_index == e || fully_outside_edge_index == -1);
					fully_outside_edge_index = e;
				}
			}
			
			if(edge.start[axis] > aabb.max_[axis]) // If start is outside
			{
				if(edge.end[axis] <= aabb.max_[axis]) // If end is inside
				{
					// Get edgevec
					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					SSE_ALIGN PaddedVec3 recip_edge;
					reciprocalSSE(&edgevec.x, &recip_edge.x);

					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * recip_edge[axis];
					assert(Maths::inRange(neg_t, -1.f, 0.f));

					//edge.start = edge.end + (edge.end - edge.start) * neg_t;
					addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);
				}
				else
				{
					assert(fully_outside_edge_index == e || fully_outside_edge_index == -1);
					fully_outside_edge_index = e;
				}
			}
		}

		if(fully_outside_edge_index != -1)
		{
			Edge& edge = edges[fully_outside_edge_index];

			assert(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis]);

			// If both verts are outside plane...
			//if(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis])
			//{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(fully_outside_edge_index + 1) % 3];
				const Edge& prev_edge = edges[(fully_outside_edge_index + 2) % 3];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
			//}
		}
	}
/*SSE_ALIGN Edge edges[3];
	{
	edges[0].start = v0;
	edges[0].end = v1;
	SSE_ALIGN PaddedVec3 edgevec = v1 - v0;
	reciprocalSSE(&edgevec.x, &edges[0].recip_edge.x);

	edges[1].start = v1;
	edges[1].end = v2;
	edgevec = v2 - v1;
	reciprocalSSE(&edgevec.x, &edges[1].recip_edge.x);

	edges[2].start = v2;
	edges[2].end = v0;
	edgevec = v0 - v2;
	reciprocalSSE(&edgevec.x, &edges[2].recip_edge.x);
	}

	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_edge[axis];
					
					//const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					//addScaledVec4SSE(&edge.start.x, &edgevec.x, t, &edge.end.x);

					edge.end = edge.start + (edge.end - edge.start) * t;

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
			
			if(edge.start[axis] < aabb.min_[axis]) // If start is outside
				if(edge.end[axis] >= aabb.min_[axis]) // If end is inside
				{
					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_edge[axis];
					
					//const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					//addScaledVec4SSE(&edge.end.x, &edgevec.x, neg_t, &edge.start.x);

					edge.start = edge.end + (edge.end - edge.start) * neg_t;

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				//edge.unit_edge = normalise(edge.end - edge.start);
				const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
				reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
			}
		}

		/// Clip against max plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] > aabb.max_[axis]) // If end is outside
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					const float t = (aabb.max_[axis] - edge.start[axis]) * edge.recip_edge[axis];
					
					edge.end = edge.start + (edge.end - edge.start) * t;
					
					//addScaledVec4SSE(&edge.start.x, &edge.unit_edge.x, t, &edge.end.x);

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
			
			if(edge.start[axis] > aabb.max_[axis]) // If start is outside
				if(edge.end[axis] <= aabb.max_[axis]) // If end is inside
				{
					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * edge.recip_edge[axis];
					
					edge.start = edge.end + (edge.end - edge.start) * neg_t;
					
					//addScaledVec4SSE(&edge.end.x, &edge.unit_edge.x, neg_t, &edge.start.x);

					const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
					reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				const SSE_ALIGN PaddedVec3 edgevec = edge.end - edge.start;
				reciprocalSSE(&edgevec.x, &edge.recip_edge.x);
			}
		}
	}*/
/*
	SSE_ALIGN Edge edges[3];
	// Fill in edges
	edges[0].start = v0;
	edges[0].end = v1;
	//edges[0].unit_edge = normalise(v1 - v0);
	//edges[0].recip_unit_edge.set(1.0f / edges[0].unit_edge.x, 1.0f / edges[0].unit_edge.y, 1.0f / edges[0].unit_edge.z);
	SSE_ALIGN PaddedVec3 edge = v1 - v0;
	reciprocalSSE(&edges[0].unit_edge.x, &edges[0].recip_unit_edge.x);

	edges[1].start = v1;
	edges[1].end = v2;
	edges[1].unit_edge = normalise(v2 - v1);
	//edges[1].recip_unit_edge.set(1.0f / edges[1].unit_edge.x, 1.0f / edges[1].unit_edge.y, 1.0f / edges[1].unit_edge.z);
	reciprocalSSE(&edges[1].unit_edge.x, &edges[1].recip_unit_edge.x);

	edges[2].start = v2;
	edges[2].end = v0;
	edges[2].unit_edge = normalise(v0 - v2);
	//edges[2].recip_unit_edge.set(1.0f / edges[2].unit_edge.x, 1.0f / edges[2].unit_edge.y, 1.0f / edges[2].unit_edge.z);
	reciprocalSSE(&edges[2].unit_edge.x, &edges[2].recip_unit_edge.x);

	for(unsigned int axis=0; axis<3; ++axis)
	{
		/// Clip against min plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] < aabb.min_[axis]) // If end is outside
				if(edge.start[axis] >= aabb.min_[axis]) // If start is inside
				{
					const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_unit_edge[axis];
					//edge.end = edge.start + edge.unit_edge * t;
					addScaledVec4SSE(&edge.start.x, &edge.unit_edge.x, t, &edge.end.x);
				}
			
			if(edge.start[axis] < aabb.min_[axis]) // If start is outside
				if(edge.end[axis] >= aabb.min_[axis]) // If end is inside
				{
					const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_unit_edge[axis];
					//edge.start = edge.end + edge.unit_edge * neg_t;
					addScaledVec4SSE(&edge.end.x, &edge.unit_edge.x, neg_t, &edge.start.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				edge.unit_edge = normalise(edge.end - edge.start);
				reciprocalSSE(&edge.unit_edge.x, &edge.recip_unit_edge.x);
			}
		}

		/// Clip against max plane ///

		// For each edge...
		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			if(edge.end[axis] > aabb.max_[axis]) // If end is outside
				if(edge.start[axis] <= aabb.max_[axis]) // If start is inside
				{
					const float t = (aabb.max_[axis] - edge.start[axis]) * edge.recip_unit_edge[axis];
					//edge.end = edge.start + edge.unit_edge * t;
					addScaledVec4SSE(&edge.start.x, &edge.unit_edge.x, t, &edge.end.x);
				}
			
			if(edge.start[axis] > aabb.max_[axis]) // If start is outside
				if(edge.end[axis] <= aabb.max_[axis]) // If end is inside
				{
					const float neg_t = (aabb.max_[axis] - edge.end[axis]) * edge.recip_unit_edge[axis];
					//edge.start = edge.end + edge.unit_edge * neg_t;
					addScaledVec4SSE(&edge.end.x, &edge.unit_edge.x, neg_t, &edge.start.x);
				}
		}

		for(unsigned int e=0; e<3; ++e)
		{
			Edge& edge = edges[e];
			
			// If both verts are outside plane...
			if(edge.start[axis] > aabb.max_[axis] && edge.end[axis] > aabb.max_[axis])
			{
				// Move the edge, by using the new vertices from the prev and next edges
				const Edge& next_edge = edges[(e + 1) % 3];
				const Edge& prev_edge = edges[e == 0 ? 2 : e - 1];
				edge.start = prev_edge.end;
				edge.end = next_edge.start;
				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				edge.unit_edge = normalise(edge.end - edge.start);
				reciprocalSSE(&edge.unit_edge.x, &edge.recip_unit_edge.x);
			}
		}
	}
*/

	// Build final AABB
	/*clipped_tri_aabb_out.min_ = edges[0].start;
	clipped_tri_aabb_out.max_ = edges[0].start;

	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[0].end);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[1].start);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[1].end);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[2].start);
	clipped_tri_aabb_out.enlargeToHoldAlignedPoint(edges[2].end);*/



	__m128 clipped_aabb_min = _mm_load_ps(&edges[0].start.x);
	__m128 clipped_aabb_max = clipped_aabb_min;

	__m128 v = _mm_load_ps(&edges[0].end.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);

	v = _mm_load_ps(&edges[1].start.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);
	v = _mm_load_ps(&edges[1].end.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);

	v = _mm_load_ps(&edges[2].start.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);
	v = _mm_load_ps(&edges[2].end.x);
	clipped_aabb_min = _mm_min_ps(clipped_aabb_min, v);
	clipped_aabb_max = _mm_max_ps(clipped_aabb_max, v);

	// Make sure the clipped_tri_aabb_out doesn't extend past aabb, due to numerical errors
	/*_mm_store_ps(
		&clipped_tri_aabb_out.min_.x,
		_mm_max_ps(
			clipped_aabb_min,
			_mm_load_ps(&aabb.min_.x)
			)
		);
	_mm_store_ps(
		&clipped_tri_aabb_out.max_.x,
		//_mm_max_ps(
		//	clipped_aabb_min, // We want the max to be at least as large as the min bound.
			_mm_min_ps(
				clipped_aabb_max,
				_mm_load_ps(&aabb.max_.x)
				)
		//	)
		);*/

	_mm_store_ps(&clipped_tri_aabb_out.min_.x, clipped_aabb_min);
	_mm_store_ps(&clipped_tri_aabb_out.max_.x, clipped_aabb_max);



	assert(aabb.containsAABBox(clipped_tri_aabb_out));
	assert(clipped_tri_aabb_out.invariant());








/*


			Edge& edge = edges[e];
			Edge& next_edge = edges[(e + 1) % 3];
			Edge& prev_edge = edges[e == 0 ? 2 : e - 1];

			//Vec3f& edge_end = edge_ends[edge];
			//Vec3f& edge_start = edge_starts[edge];
			// Compute signed distance to plane for edge start vertex
			// Compute signed distance to plane for edge end vertex


			// If both verts are outside plane...
			if(edge.start[axis] < aabb.min_[axis] && edge.end[axis] < aabb.min_[axis])
			{
				// Compute new start position for this edge
				const float prev_t = (aabb.min_[axis] - prev_edge.start[axis]) * prev_edge.recip_unit_edge[axis];
				edge.start = prev_edge.start + prev_edge.unit_edge * prev_t;

				// Compute new end position for this edge
				const float next_neg_t = (aabb.min_[axis] - next_edge.end[axis]) * next_edge.recip_unit_edge[axis];
				edge.end = next_edge.end + next_edge.unit_edge * next_neg_t;

				// Compute new unit_edge and recip_unit_edge
				//TEMP:
				edge.unit_edge = normalise(edge.end - edge.start);
				edge.recip_unit_edge.set(1.0f / edge.unit_edge.x, 1.0f / edge.unit_edge.y, 1.0f / edge.unit_edge.z);
			}
			else if(edge.start[axis] >= aabb.min_[axis] && edge.end[axis] >= aabb.min_[axis]) // Else if both verts are inside plane
			{
				// just keep edge as it is
			}
			else if(edge.end[axis] >= aabb.min_[axis]) // Else end is outside
			{
				const float t = (aabb.min_[axis] - edge.start[axis]) * edge.recip_unit_edge[axis];
				
				edge.end = edge.start + edge.unit_edge * t;
			}
			else // Else start is outside
			{
				const float neg_t = (aabb.min_[axis] - edge.end[axis]) * edge.recip_unit_edge[axis];
				
				edge.end = edge.start + edge.unit_edge * neg_t;
			}
		} // End for edge edge

		// max plane //
	}
*/




	/*clipped_tri_aabb_out = js::AABBox(
		Vec3f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
		Vec3f(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max())
		);

	const Vec3f verts[3] = {v0, v1, v2};

	for(int i=0; i<3; ++i)
	{
		const Vec3f& v_start = verts[i];
		const Vec3f& v_end = verts[(i + 1) % 3];

		const Vec3f edge = v_end - v_start;

		const Vec3f t_min(
			(aabb.min_.x - v_start.x) / edge.x,
			(aabb.min_.y - v_start.y) / edge.y,
			(aabb.min_.z - v_start.z) / edge.z
			);

		const Vec3f t_max(
			(aabb.max_.x - v_start.x) / edge.x,
			(aabb.max_.y - v_start.y) / edge.y,
			(aabb.max_.z - v_start.z) / edge.z
			);

		const Vec3f t_near(
			myMin(t_min.x, t_max.x),
			myMin(t_min.y, t_max.y),
			myMin(t_min.z, t_max.z)
			);

		const Vec3f t_far(
			myMax(t_min.x, t_max.x),
			myMax(t_min.y, t_max.y),
			myMax(t_min.z, t_max.z)
			);

		const float neg_inf = logf(0.0);
		const float pos_inf = -neg_inf;
	
		const float t_lower = myMax(
			isNAN(t_near.x) ? neg_inf : t_near.x, 
			isNAN(t_near.y) ? neg_inf : t_near.y,
			isNAN(t_near.z) ? neg_inf : t_near.z
			);
		const float t_upper = myMin(
			isNAN(t_far.x) ? pos_inf : t_far.x, 
			isNAN(t_far.y) ? pos_inf : t_far.y, 
			isNAN(t_far.z) ? pos_inf : t_far.z
			);

		if(t_lower <= t_upper)
		{
			// We have an intersection with the AABB
			SSE_ALIGN js::AABBox edge_intersection_aabb(v_start + edge * t_lower, v_start + edge * t_upper);
			clipped_tri_aabb_out.enlargeToHoldAABBox(edge_intersection_aabb);
		}
	}*/
}

static const float flt_plus_inf = -logf(0);	// let's keep C and C++ compilers happy.
const float SSE_ALIGN
	ps_cst_plus_inf[4]	= {  flt_plus_inf,  flt_plus_inf,  flt_plus_inf,  flt_plus_inf },
	ps_cst_minus_inf[4]	= { -flt_plus_inf, -flt_plus_inf, -flt_plus_inf, -flt_plus_inf };


void TriBoxIntersection::getClippedTriAABB_SSE(const Vec3f& v0_, const Vec3f& v1_, const Vec3f& v2_, const js::AABBox& aabb, js::AABBox& clipped_tri_aabb_out)
{
	const SSE_ALIGN PaddedVec3 verts[3] = {v0_, v1_, v2_};

	const SSE4Vec
		plus_inf	= load4Vec(ps_cst_plus_inf),
		minus_inf	= load4Vec(ps_cst_minus_inf);

	clipped_tri_aabb_out = js::AABBox(
		Vec3f(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
		Vec3f(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max())
		);

	for(int i=0; i<3; ++i)
	{
		const PaddedVec3& v_start_ = verts[i];
		const PaddedVec3& v_end_ = verts[(i + 1) % 3];

		const SSE4Vec v_start = load4Vec(&v_start_.x);
		
		// Get the current edge vector
		const SSE4Vec edge = sub4Vec(load4Vec(&v_end_.x), v_start); // components may be zero

		const SSE4Vec recip_edge = div4Vec(load4Vec(one_4vec), edge); // components may be +Inf
		SSE_ALIGN PaddedVec3 recip_edge_;
		store4Vec(recip_edge, &recip_edge_.x); // Save reciprocal components for later

		// Get 't' at AABB min and max planes
		const SSE4Vec min_t = mult4Vec( sub4Vec(load4Vec(&aabb.min_.x), v_start), recip_edge ); // components may be NaN, or +-Inf
		const SSE4Vec max_t = mult4Vec( sub4Vec(load4Vec(&aabb.max_.x), v_start), recip_edge );

		// Filter out NaNs caused by 0 * Inf
		// "Note that if only one value is a NaN for this instruction, the source operand (second operand) value 
		// (either NaN or valid floating-point value) is written to the result."
		// So if min_t is a NaN, then filtered_min_t_a := plus_inf
		// If max_t is a NaN, then filtered_max_t_a := plus_inf
		// In either case, filtered_t_far = max(filtered_min_t_a, filtered_max_t_a) = whichever one of min_t, max_t is not a NaN = +Inf
		// Then when the min() along all axes of t_far is taken, the +Inf will be effectively ignored.
		const SSE4Vec filtered_min_t_a = min4Vec(min_t, plus_inf);
		const SSE4Vec filtered_max_t_a = min4Vec(max_t, plus_inf);

		const SSE4Vec filtered_min_t_b = max4Vec(min_t, minus_inf);
		const SSE4Vec filtered_max_t_b = max4Vec(max_t, minus_inf);

		const SSE4Vec filtered_t_far = max4Vec(filtered_min_t_a, filtered_max_t_a);
		const SSE4Vec filtered_t_near = min4Vec(filtered_min_t_b, filtered_max_t_b);

		// Take intersection of all intervals.
		// TODO: do this with SSE

		SSE_ALIGN PaddedVec3 t_near;
		store4Vec(filtered_t_near, &t_near.x);

		SSE_ALIGN PaddedVec3 t_far;
		store4Vec(filtered_t_far, &t_far.x);
	
		const float t_lower = myMax(t_near.x, t_near.y, t_near.z);
		const float t_upper = myMin(t_far.x, t_far.y, t_far.z);

		if(t_lower <= t_upper &&
			((recip_edge_.x != flt_plus_inf) || (v_start_.x >= aabb.min_.x && v_start_.x <= aabb.max_.x)) &&
			((recip_edge_.y != flt_plus_inf) || (v_start_.y >= aabb.min_.y && v_start_.y <= aabb.max_.y)) &&
			((recip_edge_.z != flt_plus_inf) || (v_start_.z >= aabb.min_.z && v_start_.z <= aabb.max_.z))
			)
		{
			// then this edge does intersect the AABB

			const SSE4Vec lower_corner = add4Vec(v_start, mult4Vec(edge, loadScalarCopy(&t_lower)));
			store4Vec(min4Vec(load4Vec(&clipped_tri_aabb_out.min_.x), lower_corner), &clipped_tri_aabb_out.min_.x);

			const SSE4Vec upper_corner = add4Vec(v_start, mult4Vec(edge, loadScalarCopy(&t_upper)));
			store4Vec(max4Vec(load4Vec(&clipped_tri_aabb_out.max_.x), upper_corner), &clipped_tri_aabb_out.max_.x);
		}
	}

	// Make sure clipped_tri_aabb_out does not extend past aabb
	//store4Vec( max4Vec( load4Vec(&aabb.min_.x), load4Vec(&clipped_tri_aabb_out.min_.x)), &clipped_tri_aabb_out.min_.x);
	//store4Vec( min4Vec( load4Vec(&aabb.max_.x), load4Vec(&clipped_tri_aabb_out.max_.x)), &clipped_tri_aabb_out.max_.x);
}

#endif



void TriBoxIntersection::test()
{
	conPrint("TriBoxIntersection::test()");

	/// test clipPolyToPlaneHalfSpace /// 
	/*{
		Vec3f v_out[32];
		unsigned int num_verts_out;

		{
		const Vec3f v[3] = { Vec3f(0.0f, 0.0f, 0.0f), Vec3f(2.0, 2.0f, 0.0f), Vec3f(-2.0f, 2.0f, 0.0f) };
		clipPolyToPlaneHalfSpace(
			Plane<float>(Vec3f(0.f, 1.f, 0.f), 1.0), 
			v, 
			3, 
			3, 
			v_out, 
			num_verts_out
			);
		testAssert(num_verts_out == 3);
		testAssert(v_out[0] == v[0]);
		testAssert(epsEqual(Vec3d(1.0, 1.0, 0.0), toVec3d(v_out[1])));
		testAssert(epsEqual(Vec3d(-1.0, 1.0, 0.0), toVec3d(v_out[2])));
		}
	}*/

	/// test clipPolyAgainstPlane ///
	//Vec3f v_out[32];
	//unsigned int num_verts_out;


	

	/*{
	const Vec3f v[3] = { Vec3f(0.0f, 0.0f, 0.0f), Vec3f(2.0, 2.0f, 0.0f), Vec3f(-2.0f, 2.0f, 0.0f) };
	clipPolyAgainstPlane(v, 3, 1, 1.0f, 1.0f, v_out, num_verts_out);
	testAssert(num_verts_out == 3);
	testAssert(v_out[0] == v[0]);
	testAssert(epsEqual(Vec3d(1.0, 1.0, 0.0), toVec3d(v_out[1])));
	testAssert(epsEqual(Vec3d(-1.0, 1.0, 0.0), toVec3d(v_out[2])));
	}

	{
	const Vec3f v[3] = { Vec3f(0.0f, 0.0f, 0.0f), Vec3f(2.0, 2.0f, 0.0f), Vec3f(-2.0f, 2.0f, 0.0f) };
	clipPolyAgainstPlane(v, 3, 1, 10.0f, 1.0f, v_out, num_verts_out);
	testAssert(num_verts_out == 3);
	testAssert(v_out[0] == v[0]);
	testAssert(v_out[1] == v[1]);
	testAssert(v_out[2] == v[2]);
	}

	{
	const Vec3f v[3] = { Vec3f(0.0f, 0.0f, 0.0f), Vec3f(2.0, 2.0f, 0.0f), Vec3f(-2.0f, 2.0f, 0.0f) };
	clipPolyAgainstPlane(v, 3, 1, -1.0f, 1.0f, v_out, num_verts_out);
	testAssert(num_verts_out == 0);
	}

	{
	const Vec3f v[3] = { Vec3f(0.0f, 0.0f, 0.0f), Vec3f(2.0, 5.0f, 0.0f), Vec3f(-2.0f, 2.0f, 0.0f) };
	clipPolyAgainstPlane(v, 3, 1, 4.0f, 1.0f, v_out, num_verts_out);
	testAssert(num_verts_out == 4);
	testAssert(v_out[0] == v[0]);
	testAssert(v_out[3] == v[2]);
	}*/



	/// Test slowGetClippedTriAABB etc. ///
/*	const SSE_ALIGN PaddedVec3f v0(1., -1., 0.);
	const SSE_ALIGN PaddedVec3f v1(2., 4., 0.);
	const SSE_ALIGN PaddedVec3f v2(-1.f, 5., 0.);

	js::AABBox bounds_a;
	slowGetClippedTriAABB(
		v0, v1, v2,
		js::AABBox(Vec3f(-1., 1., 0.), Vec3f(3., 3., 0.)),
		bounds_a
		);

	testAssert(epsEqual(toVec3d(bounds_a.min_), Vec3d(-1./3., 1., 0.)));
	testAssert(epsEqual(toVec3d(bounds_a.max_), Vec3d(9./5., 3., 0.)));
*/
	/*getClippedTriAABB(
		v0, v1, v2,
		js::AABBox(Vec3f(-1., 1., 0.), Vec3f(3., 3., 0.)),
		bounds_a
		);

	testAssert(epsEqual(toVec3d(bounds_a.min_), Vec3d(-1./3., 1., 0.)));
	testAssert(epsEqual(toVec3d(bounds_a.max_), Vec3d(9./5., 3., 0.)));*/

	/*getClippedTriAABB_SSE(
		v0, v1, v2,
		js::AABBox(Vec3f(-1., 1., 0.), Vec3f(3., 3., 0.)),
		bounds_a
		);

	testAssert(epsEqual(toVec3d(bounds_a.min_), Vec3d(-1./3., 1., 0.)));
	testAssert(epsEqual(toVec3d(bounds_a.max_), Vec3d(9./5., 3., 0.)));*/

	/*// Do speed test
	const int N = 1000000;
	{
	Timer timer;
	for(int i=0; i<N; ++i)
	{
		js::AABBox bounds_a;
		slowGetClippedTriAABB(
			v0, v1, v2,
			js::AABBox(Vec3f(-1., 1., 0.), Vec3f(3., 3., 0.)),
			bounds_a
			);

		testAssert(epsEqual(toVec3d(bounds_a.min_), Vec3d(-1./3., 1., 0.)));
		testAssert(epsEqual(toVec3d(bounds_a.max_), Vec3d(9./5., 3., 0.)));
	}
	printVar(timer.getSecondsElapsed());
	}*/
	/*{
	Timer timer;
	for(int i=0; i<N; ++i)
	{
		js::AABBox bounds_a;
		getClippedTriAABB(
			v0, v1, v2,
			js::AABBox(Vec3f(-1., 1., 0.), Vec3f(3., 3., 0.)),
			bounds_a
			);

		testAssert(epsEqual(toVec3d(bounds_a.min_), Vec3d(-1./3., 1., 0.)));
		testAssert(epsEqual(toVec3d(bounds_a.max_), Vec3d(9./5., 3., 0.)));
	}
	printVar(timer.getSecondsElapsed());
	}*/
	/*{
	Timer timer;
	for(int i=0; i<N; ++i)
	{
		js::AABBox bounds_a;
		getClippedTriAABB_SSE(
			v0, v1, v2,
			js::AABBox(Vec3f(-1., 1., 0.), Vec3f(3., 3., 0.)),
			bounds_a
			);

		testAssert(epsEqual(toVec3d(bounds_a.min_), Vec3d(-1./3., 1., 0.)));
		testAssert(epsEqual(toVec3d(bounds_a.max_), Vec3d(9./5., 3., 0.)));
	}
	const double elapsed_time = timer.getSecondsElapsed();
	const double clock_freq = 2.4e9;
	const double elapsed_cycles = elapsed_time * clock_freq;
	const double cycles_per_iteration = elapsed_cycles / (double)N;
	printVar(elapsed_time);
	printVar(elapsed_cycles);
	printVar(cycles_per_iteration);
	}*/


}









#if 0


/********************************************************/
/* AABB-triangle overlap test code                      */
/* by Tomas Akenine-Möller                              */
/* Function: int triBoxOverlap(float boxcenter[3],      */
/*          float boxhalfsize[3],float triverts[3][3]); */
/* History:                                             */
/*   2001-03-05: released the code in its first version */
/*   2001-06-18: changed the order of the tests, faster */
/*                                                      */
/* Acknowledgement: Many thanks to Pierre Terdiman for  */
/* suggestions and discussions on how to optimize code. */
/* Thanks to David Hunt for finding a ">="-bug!         */
/********************************************************/


#define X 0
#define Y 1
#define Z 2

#define CROSS(dest,v1,v2) \
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0]; 

#define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])

#define SUB(dest,v1,v2) \
          dest[0]=v1[0]-v2[0]; \
          dest[1]=v1[1]-v2[1]; \
          dest[2]=v1[2]-v2[2]; 

#define FINDMINMAX(x0,x1,x2,min,max) \
  min = max = x0;   \
  if(x1<min) min=x1;\
  if(x1>max) max=x1;\
  if(x2<min) min=x2;\
  if(x2>max) max=x2;

int planeBoxOverlap(float normal[3],float d, float maxbox[3])
{
  int q;
  float vmin[3],vmax[3];
  for(q=X;q<=Z;q++)
  {
    if(normal[q]>0.0f)
    {
      vmin[q]=-maxbox[q];
      vmax[q]=maxbox[q];
    }
    else
    {
      vmin[q]=maxbox[q];
      vmax[q]=-maxbox[q];
    }
  }
  if(DOT(normal,vmin)+d>0.0f) return 0;
  if(DOT(normal,vmax)+d>=0.0f) return 1;
  
  return 0;
}


/*======================== X-tests ========================*/
#define AXISTEST_X01(a, b, fa, fb)			   \
	p0 = a*v0[Y] - b*v0[Z];			       	   \
	p2 = a*v2[Y] - b*v2[Z];			       	   \
        if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
	rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

#define AXISTEST_X2(a, b, fa, fb)			   \
	p0 = a*v0[Y] - b*v0[Z];			           \
	p1 = a*v1[Y] - b*v1[Z];			       	   \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize[Y] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

/*======================== Y-tests ========================*/
#define AXISTEST_Y02(a, b, fa, fb)			   \
	p0 = -a*v0[X] + b*v0[Z];		      	   \
	p2 = -a*v2[X] + b*v2[Z];	       	       	   \
        if(p0<p2) {min=p0; max=p2;} else {min=p2; max=p0;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

#define AXISTEST_Y1(a, b, fa, fb)			   \
	p0 = -a*v0[X] + b*v0[Z];		      	   \
	p1 = -a*v1[X] + b*v1[Z];	     	       	   \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Z];   \
	if(min>rad || max<-rad) return 0;

/*======================== Z-tests ========================*/

#define AXISTEST_Z12(a, b, fa, fb)			   \
	p1 = a*v1[X] - b*v1[Y];			           \
	p2 = a*v2[X] - b*v2[Y];			       	   \
        if(p2<p1) {min=p2; max=p1;} else {min=p1; max=p2;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
	if(min>rad || max<-rad) return 0;

#define AXISTEST_Z0(a, b, fa, fb)			   \
	p0 = a*v0[X] - b*v0[Y];				   \
	p1 = a*v1[X] - b*v1[Y];			           \
        if(p0<p1) {min=p0; max=p1;} else {min=p1; max=p0;} \
	rad = fa * boxhalfsize[X] + fb * boxhalfsize[Y];   \
	if(min>rad || max<-rad) return 0;

int TriBoxIntersection::triBoxOverlap(float boxcenter[3],float boxhalfsize[3],float triverts[3][3])
{

  /*    use separating axis theorem to test overlap between triangle and box */
  /*    need to test for overlap in these directions: */
  /*    1) the {x,y,z}-directions (actually, since we use the AABB of the triangle */
  /*       we do not even need to test these) */
  /*    2) normal of the triangle */
  /*    3) crossproduct(edge from tri, {x,y,z}-directin) */
  /*       this gives 3x3=9 more tests */
   float v0[3],v1[3],v2[3];
   //float axis[3];
   float min,max,d,p0,p1,p2,rad,fex,fey,fez;  
   float normal[3],e0[3],e1[3],e2[3];

   /* This is the fastest branch on Sun */
   /* move everything so that the boxcenter is in (0,0,0) */
   SUB(v0,triverts[0],boxcenter);
   SUB(v1,triverts[1],boxcenter);
   SUB(v2,triverts[2],boxcenter);

   /* compute triangle edges */
   SUB(e0,v1,v0);      /* tri edge 0 */
   SUB(e1,v2,v1);      /* tri edge 1 */
   SUB(e2,v0,v2);      /* tri edge 2 */

   /* Bullet 3:  */
   /*  test the 9 tests first (this was faster) */
   fex = fabs(e0[X]);
   fey = fabs(e0[Y]);
   fez = fabs(e0[Z]);
   AXISTEST_X01(e0[Z], e0[Y], fez, fey);
   AXISTEST_Y02(e0[Z], e0[X], fez, fex);
   AXISTEST_Z12(e0[Y], e0[X], fey, fex);

   fex = fabs(e1[X]);
   fey = fabs(e1[Y]);
   fez = fabs(e1[Z]);
   AXISTEST_X01(e1[Z], e1[Y], fez, fey);
   AXISTEST_Y02(e1[Z], e1[X], fez, fex);
   AXISTEST_Z0(e1[Y], e1[X], fey, fex);

   fex = fabs(e2[X]);
   fey = fabs(e2[Y]);
   fez = fabs(e2[Z]);
   AXISTEST_X2(e2[Z], e2[Y], fez, fey);
   AXISTEST_Y1(e2[Z], e2[X], fez, fex);
   AXISTEST_Z12(e2[Y], e2[X], fey, fex);

   /* Bullet 1: */
   /*  first test overlap in the {x,y,z}-directions */
   /*  find min, max of the triangle each direction, and test for overlap in */
   /*  that direction -- this is equivalent to testing a minimal AABB around */
   /*  the triangle against the AABB */

   /* test in X-direction */
   FINDMINMAX(v0[X],v1[X],v2[X],min,max);
   if(min>boxhalfsize[X] || max<-boxhalfsize[X]) return 0;

   /* test in Y-direction */
   FINDMINMAX(v0[Y],v1[Y],v2[Y],min,max);
   if(min>boxhalfsize[Y] || max<-boxhalfsize[Y]) return 0;

   /* test in Z-direction */
   FINDMINMAX(v0[Z],v1[Z],v2[Z],min,max);
   if(min>boxhalfsize[Z] || max<-boxhalfsize[Z]) return 0;

   /* Bullet 2: */
   /*  test if the box intersects the plane of the triangle */
   /*  compute plane equation of triangle: normal*x+d=0 */
   CROSS(normal,e0,e1);
   d=-DOT(normal,v0);  /* plane eq: normal.x+d=0 */
   if(!planeBoxOverlap(normal,d,boxhalfsize)) return 0;

   return 1;   /* box and triangle overlaps */
}

#endif



