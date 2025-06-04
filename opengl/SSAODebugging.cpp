/*=====================================================================
SSAODebugging.cpp
-----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "SSAODebugging.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"
#include "MeshPrimitiveBuilding.h"


typedef Vec2f vec2;
typedef Vec3f vec3;
typedef uint32 uint;
static const float PI = Maths::pi<float>();
static const float PI_2 = Maths::pi<float>() / 2;

static Vec3f cross(const Vec3f& a, const Vec3f& b)
{
	return crossProduct(a, b);
}
static float dot(const Vec3f& a, const Vec3f& b)
{
	return dotProduct(a, b);
}
static Vec3f normalize(const Vec3f& v) { return normalise(v); }
static inline float min(float x, float y) { return myMin(x, y); }
static inline float max(float x, float y) { return myMax(x, y); }
static inline float clamp(float x, float a, float b) { return myClamp(x, a, b); }

static inline float sign(float x) { return Maths::sign(x); }


#define SECTOR_COUNT 32

// From https://cdrinmatane.github.io/posts/ssaovb-code/
uint occlusionBitMask(float minHorizon, float maxHorizon)
{
    uint startHorizonInt = uint(minHorizon * SECTOR_COUNT);
    uint angleHorizonInt = uint(ceil((maxHorizon-minHorizon) * SECTOR_COUNT));
    uint angleHorizonBitfield = angleHorizonInt > 0 ? (0xFFFFFFFF >> (SECTOR_COUNT-angleHorizonInt)) : 0;
    return angleHorizonBitfield << startHorizonInt;
}

// https://graphics.stanford.edu/%7Eseander/bithacks.html#CountBitsSetParallel | license: public domain
uint countSetBits(uint v)
{
    v = v - ((v >> 1u) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2u) & 0x33333333u);
    return (((v + (v >> 4u)) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}


vec3 viewSpaceFromScreenSpacePos(vec2 normed_pos_ss, OpenGLEngine& gl_engine, SSAODebugging::DepthQuerier& depth_querier)
{
	float depth = depth_querier.depthForPosSS(normed_pos_ss);// getDepthFromDepthTexture(normed_pos_ss);
 
	float l_over_w = gl_engine.getCurrentScene()->lens_sensor_dist / gl_engine.getCurrentScene()->use_sensor_width;
	float l_over_h = gl_engine.getCurrentScene()->lens_sensor_dist / gl_engine.getCurrentScene()->use_sensor_height;

	return vec3(
		(normed_pos_ss.x - 0.5f) * depth / l_over_w,
		(normed_pos_ss.y - 0.5f) * depth / l_over_h,
		-depth
	);
}

static vec3 rayPlaneIntersectPoint(vec3 p, vec3 dir, float plane_d, vec3 plane_n)
{
	const float start_to_plane_dist = dot(p, plane_n) - plane_d;

	return p + dir * (start_to_plane_dist / -dot(dir, plane_n));
}


float SSAODebugging::computeReferenceAO(OpenGLEngine& gl_engine, DepthQuerier& depth_querier)
{
	Vec2f texture_coords(0.5, 0.5); // middle of screen
	//vec3 src_normal_encoded = textureLod(normal_tex, texture_coords, 0.0).xyz; // Encoded as a RGB8 texture (converted to floating point)
	//vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture
	vec3 src_normal_ws = depth_querier.normalForPosSS(texture_coords);


	//vec3 n_p = normalize((frag_view_matrix * vec4(src_normal_ws, 0)).xyz); // View space 'fragment' normal
	vec3 n = Vec3f(normalise(gl_engine.getCurrentScene()->last_view_matrix * src_normal_ws.toVec4fVector()));

	vec3 p = viewSpaceFromScreenSpacePos(texture_coords, gl_engine, depth_querier); // View space 'fragment' position
	vec3 unit_p = normalize(p);
	vec3 V = -unit_p; // View vector: vector from 'fragment' position to camera in view/camera space

	drawPoint(gl_engine, p, Colour4f(1,0,0,1)); // Draw fragment position

	const float plane_d = dot(p, n);
	
	vec2 origin_ss = texture_coords; // Origin of stepping in screen space

	//uint pixel_id = uint(gl_FragCoord.x + gl_FragCoord.y * iResolution.x);

	float thickness = 0.2;
	float r = 0.2; // Total stepping radius in screen space
	int N_s = 16; // Number of steps per direction
	float step_size = r / float(N_s);//(float(N_s) + 1.0f);

	int N_d = 1; // num directions

	//float ao = 0; // Accumulated ambient occlusion. 1 = not occluded at all, 0 = totally occluded.
	//vec3 indirect = vec3(0.0);
	float uniform_irradiance = 0.0;
	vec3 irradiance = vec3(0.0);

	const float pixel_hash = 0.5f;

	float debug_val = 0.0;
	for(int i=0; i<N_d; ++i) // For each direction:
	{
		const float aspect_ratio = gl_engine.getViewPortAspectRatio();
		// Direction d_i is given by the view space direction (1,0,0) rotated around (0, 0, 1) by theta = 2pi * i / N_d
		//float theta = -PI / 2.0f; // TEMP 2.0 * PI * float(i) / float(N_d);
		//vec3 d_i_vs = vec3(cos(theta), sin(theta), 0); // direction i (d_i) in view/camera space
		//vec2 d_i_ss = vec2(d_i_vs.x, d_i_vs.y); // d_i_vs.xy; // direction i (d_i) in normalised screen space
		const float theta = PI * (float(i) + pixel_hash) / float(N_d);
		const vec3 d_i_vs = vec3(cos(theta), sin(theta) * aspect_ratio, 0); // direction i (d_i) in view/camera space
		const vec2 d_i_ss = vec2(d_i_vs.x, d_i_vs.y); // direction i (d_i) in normalised screen space

		//d_i_vs = d_i_vs - V * dot(V, d_i_vs); // Make d_i_vs orthogonal to view vector

		//vec3 d_i_vs_cross_v = normalize(cross(d_i_vs, V)); // NOTE: need normalize?  Vector normal to sampling plane and orthogonal to view vector.
		//vec3 projected_n_p = normalize(n - d_i_vs_cross_v * dot(n, d_i_vs_cross_v)); // fragment surface normal projected into sampling plane

		const vec3 sampling_plane_n = normalize(cross(d_i_vs, V));
		const vec3 projected_n = normalize(removeComponentInDir(n, sampling_plane_n));

		const float view_proj_n_angle = acos(dot(projected_n, V));
		const float view_alpha = PI_2 + sign(dot(cross(V, projected_n), sampling_plane_n)) * view_proj_n_angle;

		drawSamplingPlane(gl_engine, p, sampling_plane_n, projected_n);

		float step_incr = r / N_s; // Distance in screen space to step, increases slightly each step.
		float last_step_incr = step_incr;
		float dist_ss = step_incr;// * pixel_hash; // Total distance stepped in screen space, before randomisation

		uint b_i = 0; // bitmask (= globalOccludedBitfield)
		for(int q=0; q<N_s * 2; ++q)
		{
			if(q == N_s)
			{
				// Reset, start walking in other direction
				step_incr = -r / N_s;
				last_step_incr = step_incr;
				dist_ss = step_incr;
			}

			float cur_dist_ss = dist_ss - pixel_hash * last_step_incr;

			//vec2 pos_j_ss = origin_ss + d_i_ss * step_size * float(j); // step_j position in screen space
			const vec2 pos_j_ss = origin_ss + d_i_ss * cur_dist_ss; // step_j position in screen space
			if(!(pos_j_ss.x >= 0.0 && pos_j_ss.y <= 1.0 && pos_j_ss.y >= 0.0 && pos_j_ss.y <= 1.0))
				continue;

			vec3 pos_j = viewSpaceFromScreenSpacePos(pos_j_ss, gl_engine, depth_querier); // step_j position in camera/view space

			drawPoint(gl_engine, pos_j, Colour4f(1,1,0,1)); // Draw pos_j

			vec3 back_pos_j = pos_j - V * thickness; // position of guessed 'backside' of step position in camera/view space

			//pos_j = rayPlaneIntersectPoint(pos_j, /*dir=*/V, plane_d, /*plane_normal=*/n);
			//back_pos_j = rayPlaneIntersectPoint(back_pos_j, /*dir=*/V, plane_d, /*plane_normal=*/n);

			const vec3 unit_p_to_pos_j      = normalize(pos_j      - p);
			const vec3 unit_p_to_back_pos_j = normalize(back_pos_j - p);
			

			// Convert to angles in [0, pi], the angle between the surface and frag-to-step_j position
			const float V_p_p_j_angle =      acos(dot(V, unit_p_to_pos_j)); // Angle between view vector and p to p_j.
			const float V_p_p_j_back_angle = acos(dot(V, unit_p_to_back_pos_j)); // Angle between view vector and p to p_back_j.
			const float angle_add_sign = sign(dot(cross(unit_p_to_pos_j, V), sampling_plane_n));
			float front_alpha = view_alpha + angle_add_sign * V_p_p_j_angle;
			float back_alpha  = view_alpha + angle_add_sign * V_p_p_j_back_angle;

			// Map from [0, pi] to [0, 1]
			front_alpha = clamp(front_alpha / PI, 0.0, 1.0);
			back_alpha  = clamp(back_alpha  / PI, 0.0, 1.0);

			const float min_alpha = min(front_alpha, back_alpha);
			const float max_alpha = max(front_alpha, back_alpha);

			//debug_val = clamp(front_view_angle, 0.0, 1.0);

			uint occlusion_mask = occlusionBitMask(min_alpha, max_alpha);
			uint new_b_i = b_i | occlusion_mask;
			uint bits_changed = new_b_i & ~b_i; //new_b_i - b_i;
			assert(bits_changed == new_b_i - b_i);
			b_i  = new_b_i;

			//indirect += vec3(1.0) * float(countSetBits(bits_changed)) * (1.0 / float(SECTOR_COUNT)); // texture(diffuse_tex, pos_j_ss).xyz;
			const float cos_norm_angle = dot(unit_p_to_pos_j, n);
			if((cos_norm_angle > 0.01f) && (bits_changed != 0))
			{
				// Draw sector
				drawSectors(gl_engine, p, bits_changed, sampling_plane_n, projected_n);

				vec3 n_j_vs = Vec3f(normalise(gl_engine.getCurrentScene()->last_view_matrix * depth_querier.normalForPosSS(pos_j_ss).toVec4fVector()));
				float n_j_cos_theta = dot(n_j_vs, -unit_p_to_pos_j); // cosine of angle between surface normal at step position and vector from step position to p.

				const float sin_factor = sqrt(max(0.0f, 1.0f - cos_norm_angle*cos_norm_angle));

				const float scalar_factors = cos_norm_angle * sin_factor * float(countSetBits(bits_changed));
				uniform_irradiance += scalar_factors;

				if(n_j_cos_theta > -0.3) // dot(n_j_ss, p_to_pos_j_vs) < 0)
				{
					vec3 common_factors = scalar_factors * vec3(1.f); // TEMP textureLod(diffuse_tex, pos_j_ss, 0.0).xyz;
					irradiance += /*n_j_cos_theta * */common_factors;
				}
			}

			dist_ss += step_incr;
			last_step_incr = step_incr;
			const float step_incr_factor = exp(log(2.0) / float(N_s));
			step_incr *= step_incr_factor;
		}

		//ao += 1.0f - float(countSetBits(b_i)) * (1.0f / 32.0f);
	}

	uniform_irradiance        *= PI * PI / float(N_d * SECTOR_COUNT);
	//uniform_specular_spec_rad *= PI * PI / float(N_d * SECTOR_COUNT);
	irradiance                *= PI * PI / float(N_d * SECTOR_COUNT);


	const float irradiance_scale = 1.0f - clamp(uniform_irradiance / PI, 0.0f, 1.0f);

	//ao *= (1.0f / float(N_d));
	//indirect *= (1.0f / float(N_d));
	return irradiance_scale;
}


void SSAODebugging::addDebugObjects(OpenGLEngine& gl_engine)
{

}

void SSAODebugging::drawSamplingPlane(OpenGLEngine& gl_engine, Vec3f p_cs, Vec3f sampling_plane_n_cs, Vec3f projected_n_cs)
{
	Matrix4f cam_to_world;
	bool res = gl_engine.getCurrentScene()->last_view_matrix.getInverseForAffine3Matrix(cam_to_world);
	assert(res);

	Vec4f p_ws           = cam_to_world * p_cs.toVec4fPoint();
	Vec4f projected_n_ws = cam_to_world * projected_n_cs.toVec4fVector();
	Vec4f sampling_plane_n_ws = cam_to_world * sampling_plane_n_cs.toVec4fVector();
	const Vec4f tangent = normalise(crossProduct(sampling_plane_n_ws, projected_n_ws));

	{
		GLObjectRef ob = gl_engine.makeArrowObject(p_ws, p_ws + projected_n_ws, Colour4f(1,0,0,1), 0.5f);
		gl_engine.addObject(ob);
	}

	{
		GLObjectRef ob = new GLObject();
		ob->mesh_data = MeshPrimitiveBuilding::makeCircleSector(*gl_engine.vert_buf_allocator, /*angle=*/Maths::pi<float>());
		ob->ob_to_world_matrix = /*Matrix4f::translationMatrix(p.x, p.y, p.z) * */Matrix4f(tangent, projected_n_ws, sampling_plane_n_ws, 
			Vec4f(p_ws[0], p_ws[1], p_ws[2], 1));
		ob->materials.resize(1);
		ob->materials[0].albedo_linear_rgb = Colour3f(0.1f, 0.2f, 0.5);
		ob->materials[0].alpha = 0.5f;
		//ob->materials[0].alpha_blend = true;
		ob->materials[0].simple_double_sided = true;
		gl_engine.addObject(ob);
	}
}


void SSAODebugging::drawPoint(OpenGLEngine& gl_engine, Vec3f p_cs, const Colour4f& col)
{
	Matrix4f cam_to_world;
	bool res = gl_engine.getCurrentScene()->last_view_matrix.getInverseForAffine3Matrix(cam_to_world);
	assert(res);

	Vec4f p_ws           = cam_to_world * p_cs.toVec4fPoint();

	// Temp: add marker in opengl scene
	GLObjectRef ob = gl_engine.makeSphereObject(1.f, col);//Colour4f(1,1,0,1));
	ob->ob_to_world_matrix = Matrix4f::translationMatrix(p_ws) *  Matrix4f::uniformScaleMatrix(0.02f);
	gl_engine.addObject(ob);
}


void SSAODebugging::drawSectors(OpenGLEngine& gl_engine, vec3 p_cs, uint32 bits_changed, vec3 sampling_plane_n_cs, vec3 projected_n_cs)
{
	Matrix4f cam_to_world;
	bool res = gl_engine.getCurrentScene()->last_view_matrix.getInverseForAffine3Matrix(cam_to_world);
	assert(res);

	Vec4f p_ws           = cam_to_world * p_cs.toVec4fPoint();
	Vec4f projected_n_ws = cam_to_world * projected_n_cs.toVec4fVector();
	Vec4f sampling_plane_n_ws = cam_to_world * sampling_plane_n_cs.toVec4fVector();
	const Vec4f tangent = normalise(crossProduct(sampling_plane_n_ws, projected_n_ws));
	for(uint i=0; i<32; ++i)
	{
		if(bits_changed & (1 << i))
		{
			// Add a couple with offsets to avoid z-fighting with the the sampling plane sector
			for(int z=0; z<2; ++z)
			{
				Vec4f use_p_ws = p_ws + sampling_plane_n_ws * (-0.5f + z) * 0.001f;
				GLObjectRef ob = new GLObject();
				ob->mesh_data = MeshPrimitiveBuilding::makeCircleSector(*gl_engine.vert_buf_allocator, /*angle=*/0.85f * (Maths::pi<float>() / 32)); // Make slightly narrower to distinguish each individual sector
				ob->ob_to_world_matrix = /*Matrix4f::translationMatrix(p.x, p.y, p.z) * */Matrix4f(tangent, projected_n_ws, sampling_plane_n_ws, 
					Vec4f(use_p_ws[0], use_p_ws[1], use_p_ws[2], 1)) * Matrix4f::rotationAroundZAxis((i + 0.075f) * Maths::pi<float>() / 32);
				ob->materials.resize(1);
				ob->materials[0].albedo_linear_rgb = Colour3f(1, 0.5, 0.5);
				ob->materials[0].simple_double_sided = true;
				gl_engine.addObject(ob);
			}
		}
	}
}


#if BUILD_TESTS


#include <utils/TestUtils.h>


///countSetBits
void SSAODebugging::test()
{
	testAssert(countSetBits(0) == 0);
	testAssert(countSetBits(0x1) == 1);

	for(int i=0; i<32; ++i)
		testAssert(countSetBits(0x1 << i) == 1);

//	for(uint32 i=0; i<std::numeric_limits<uint32>::max(); ++i)
//	{
//		testAssert(countSetBits(i) == _mm_popcnt_u32(i));
//	}
//
//	testAssert(countSetBits(std::numeric_limits<uint32>::max()) == _mm_popcnt_u32(std::numeric_limits<uint32>::max()));

}

#endif
