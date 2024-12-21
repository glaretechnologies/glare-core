/*=====================================================================
SSAODebugging.cpp
-----------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "SSAODebugging.h"


#include "IncludeOpenGL.h"
#include "OpenGLEngine.h"


typedef Vec2f vec2;
typedef Vec3f vec3;
typedef uint32 uint;
static const float PI = Maths::pi<float>();

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
    return ((v + (v >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
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


float SSAODebugging::computeReferenceAO(OpenGLEngine& gl_engine, DepthQuerier& depth_querier)
{
	Vec2f texture_coords(0.5, 0.5); // middle of screen
	//vec3 src_normal_encoded = textureLod(normal_tex, texture_coords, 0.0).xyz; // Encoded as a RGB8 texture (converted to floating point)
	//vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture
	vec3 src_normal_ws = depth_querier.normalForPosSS(texture_coords);


	//vec3 n_p = normalize((frag_view_matrix * vec4(src_normal_ws, 0)).xyz); // View space 'fragment' normal
	vec3 n_p = Vec3f(normalise(gl_engine.debug_last_main_view_matrix * src_normal_ws.toVec4fVector()));

	vec3 p = viewSpaceFromScreenSpacePos(texture_coords, gl_engine, depth_querier); // View space 'fragment' position
	vec3 unit_p = normalize(p);
	vec3 V = -unit_p; // View vector: vector from 'fragment' position to camera in view/camera space
	
	vec2 origin_ss = texture_coords; // Origin of stepping in screen space

	//uint pixel_id = uint(gl_FragCoord.x + gl_FragCoord.y * iResolution.x);

	float thickness = 0.2;
	float r = 0.05; // Total stepping radius in screen space
	int N_s = 16; // Number of steps per direction
	float step_size = r / (float(N_s) + 1.0f);

	int N_d = 1; // num directions

	float ao = 0; // Accumulated ambient occlusion. 1 = not occluded at all, 0 = totally occluded.
	vec3 indirect = vec3(0.0);

	float debug_val = 0.0;
	for(int i=0; i<N_d; ++i) // For each direction:
	{
		// Direction d_i is given by the view space direction (1,0,0) rotated around (0, 0, 1) by theta = 2pi * i / N_d
		float theta = -PI / 2.0f; // TEMP 2.0 * PI * float(i) / float(N_d);
		vec3 d_i_vs = vec3(cos(theta), sin(theta), 0); // direction i (d_i) in view/camera space
		vec2 d_i_ss = vec2(d_i_vs.x, d_i_vs.y); // d_i_vs.xy; // direction i (d_i) in normalised screen space

		d_i_vs = d_i_vs - V * dot(V, d_i_vs); // Make d_i_vs orthogonal to view vector

		vec3 d_i_vs_cross_v = normalize(cross(d_i_vs, V)); // NOTE: need normalize?  Vector normal to sampling plane and orthogonal to view vector.
		vec3 projected_n_p = normalize(n_p - d_i_vs_cross_v * dot(n_p, d_i_vs_cross_v)); // fragment surface normal projected into sampling plane

		// Get angle between projected normal and view vector
		float N = acos(dot(projected_n_p, V)) * sign(dot(d_i_vs_cross_v, cross(projected_n_p, V)));
		

		uint b_i = 0; // bitmask (= globalOccludedBitfield)
		for(int j=1; j<=N_s; ++j) // For each step
		{
			vec2 pos_j_ss = origin_ss + d_i_ss * step_size * float(j); // step_j position in screen space
			vec3 pos_j_vs = viewSpaceFromScreenSpacePos(pos_j_ss, gl_engine, depth_querier); // step_j position in camera/view space

			vec3 back_pos_j_vs = pos_j_vs - V * thickness; // position of guessed 'backside' of step position in camera/view space

			vec3 p_to_pos_j_vs = pos_j_vs - p; // Vector from fragment position to step position
			vec3 p_to_back_pos_j_vs = back_pos_j_vs - p;
			float front_view_angle = acos(dot(normalize(     p_to_pos_j_vs), V)); // Angle between view vector (frag to cam) and frag-to-step_j position.
			float back_view_angle  = acos(dot(normalize(p_to_back_pos_j_vs), V)); // Angle between view vector (frag to cam) and frag-to-back_step_j position.

			

			float theta_min = min(front_view_angle, back_view_angle);
			float theta_max = max(front_view_angle, back_view_angle);

			debug_val = clamp(front_view_angle, 0.0, 1.0);

			// Adjust to get min and max angles around sampling-plane normal
			float min_angle_from_proj_N = theta_min - N;
			float min_angle_from_proj_N_01 = clamp((min_angle_from_proj_N + (0.5f * PI)) / PI, 0.0f, 1.0f); // Map from [-pi/2, pi/2] to [0, 1]

			float max_angle_from_proj_N = theta_max - N;
			float max_angle_from_proj_N_01 = clamp((max_angle_from_proj_N + (0.5f * PI)) / PI, 0.0f, 1.0f); // Map from [-pi/2, pi/2] to [0, 1]

			uint occlusion_mask = occlusionBitMask(min_angle_from_proj_N_01, max_angle_from_proj_N_01);
			uint new_b_i = b_i | occlusion_mask;
			uint bits_changed = new_b_i & ~b_i; //new_b_i - b_i;
			assert(bits_changed == new_b_i - b_i);
			b_i  = new_b_i;

			indirect += vec3(1.0) * float(countSetBits(bits_changed)) * (1.0 / float(SECTOR_COUNT)); // texture(diffuse_tex, pos_j_ss).xyz;
		}

		ao += 1.0f - float(countSetBits(b_i)) * (1.0f / 32.0f);
	}

	ao *= (1.0f / float(N_d));
	indirect *= (1.0f / float(N_d));
	return ao;
}


void SSAODebugging::addDebugObjects(OpenGLEngine& gl_engine)
{

}