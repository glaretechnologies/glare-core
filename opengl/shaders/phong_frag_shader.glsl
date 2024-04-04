
in vec3 normal_ws;
in vec3 pos_cs;
#if GENERATE_PLANAR_UVS
in vec3 pos_os;
#endif
in vec3 pos_ws;
in vec2 texture_coords;
#if NUM_DEPTH_TEXTURES > 0
in vec3 shadow_tex_coords[NUM_DEPTH_TEXTURES];
#endif
in vec3 cam_to_pos_ws;
#if VERT_COLOURS
in vec3 vert_colour;
#endif
#if LIGHTMAPPING
in vec2 lightmap_coords;
#endif

#if VERT_TANGENTS
in vec3 tangent_ws;
in vec3 bitangent_ws;
#endif

flat in ivec4 light_indices_0;
flat in ivec4 light_indices_1;

#if DECAL
in mat4 world_to_ob;

uniform sampler2D main_colour_texture; // source texture
uniform sampler2D main_normal_texture;
uniform sampler2D main_depth_texture;

#endif

#if !USE_BINDLESS_TEXTURES
uniform sampler2D diffuse_tex;
uniform sampler2D metallic_roughness_tex;
uniform sampler2D emission_tex;
uniform sampler2D normal_map;

#if DOUBLE_SIDED
uniform sampler2D backface_albedo_tex;
uniform sampler2D transmission_tex;
#endif

#endif



uniform mediump sampler2DShadow dynamic_depth_tex;
uniform mediump sampler2DShadow static_depth_tex;
uniform samplerCube cosine_env_tex;
uniform sampler2D specular_env_tex;
uniform sampler2D blue_noise_tex;
uniform sampler2D fbm_tex;
uniform sampler2D detail_tex_0; // rock
uniform sampler2D detail_tex_1; // sediment
uniform sampler2D detail_tex_2; // vegetation
uniform sampler2D detail_tex_3;

uniform sampler2D detail_heightmap_0; // rock

uniform sampler2D caustic_tex_a;
uniform sampler2D caustic_tex_b;

// uniform sampler2D snow_ice_normal_map;

#if LIGHTMAPPING
#if !USE_BINDLESS_TEXTURES
uniform sampler2D lightmap_tex;
#endif
#endif


//----------------------------------------------------------------------------------------------------------------------------
#if USE_MULTIDRAW_ELEMENTS_INDIRECT

flat in int material_index;


layout(std430) buffer PhongUniforms
{
	MaterialData material_data[];
};

//#define MAT_UNIFORM mat_data
#define MAT_UNIFORM					material_data[material_index]

#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define METALLIC_ROUGHNESS_TEX		MAT_UNIFORM.metallic_roughness_tex
#define LIGHTMAP_TEX				MAT_UNIFORM.lightmap_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex
#define BACKFACE_ALBEDO_TEX			MAT_UNIFORM.backface_albedo_tex
#define TRANSMISSION_TEX			MAT_UNIFORM.transmission_tex
#define NORMAL_MAP					MAT_UNIFORM.normal_map

//----------------------------------------------------------------------------------------------------------------------------
#else // else if !USE_MULTIDRAW_ELEMENTS_INDIRECT:

layout (std140) uniform PhongUniforms
{
	MaterialData matdata;

} mat_data;

#define MAT_UNIFORM mat_data.matdata

#if USE_BINDLESS_TEXTURES
#define DIFFUSE_TEX					MAT_UNIFORM.diffuse_tex
#define METALLIC_ROUGHNESS_TEX		MAT_UNIFORM.metallic_roughness_tex
#define LIGHTMAP_TEX				MAT_UNIFORM.lightmap_tex
#define EMISSION_TEX				MAT_UNIFORM.emission_tex
#define BACKFACE_ALBEDO_TEX			MAT_UNIFORM.backface_albedo_tex
#define TRANSMISSION_TEX			MAT_UNIFORM.transmission_tex
#define NORMAL_MAP					MAT_UNIFORM.normal_map

#else
#define DIFFUSE_TEX					diffuse_tex
#define METALLIC_ROUGHNESS_TEX		metallic_roughness_tex
#define LIGHTMAP_TEX				lightmap_tex
#define EMISSION_TEX				emission_tex
#define BACKFACE_ALBEDO_TEX			backface_albedo_tex
#define TRANSMISSION_TEX			transmission_tex
#define NORMAL_MAP					normal_map
#endif

#endif // end if !USE_MULTIDRAW_ELEMENTS_INDIRECT
//----------------------------------------------------------------------------------------------------------------------------

#if BLOB_SHADOWS
uniform int num_blob_positions;
uniform vec4 blob_positions[8];
#endif


#if USE_SSBOS
layout (std430) buffer LightDataStorage
{
	LightData light_data[];
};
#else
layout (std140) uniform LightDataStorage
{
	LightData light_data[256];
};
#endif


layout(location = 0) out vec4 colour_out;
layout(location = 1) out vec3 normal_out;


float square(float x) { return x*x; }
float pow4(float x) { return (x*x)*(x*x); }
float pow5(float x) { return x*x*x*x*x; }
float pow6(float x) { return x*x*x*x*x*x; }


float length2(vec2 v) { return dot(v, v); }

float alpha2ForRoughness(float r)
{
	//return pow6(r);
	return pow4(r);
}

float trowbridgeReitzPDF(float cos_theta, float alpha2)
{
	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * square(square(cos_theta) * (alpha2 - 1.0) + 1.0));
}


float fresnelApprox(float cos_theta_i, float n2)
{
	//float r_0 = square((1.0 - n2) / (1.0 + n2));
	//return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta_i); // https://en.wikipedia.org/wiki/Schlick%27s_approximation

	float sintheta_i = sqrt(1.0 - cos_theta_i*cos_theta_i); // Get sin(theta_i)
	float sintheta_t = sintheta_i / n2; // Use Snell's law to get sin(theta_t)

	float costheta_t = sqrt(1.0 - sintheta_t*sintheta_t); // Get cos(theta_t)

	float a2 = square(cos_theta_i - n2*costheta_t);
	float b2 = square(cos_theta_i + n2*costheta_t);

	float c2 = square(n2*cos_theta_i - costheta_t);
	float d2 = square(costheta_t + n2*cos_theta_i);

	return 0.5 * (a2*d2 + b2*c2) / (b2*d2);
}


float metallicFresnelApprox(float cos_theta, float r_0)
{
	return r_0 + (1.0 - r_0)*pow5(1.0 - cos_theta);
}


vec2 samples[16] = vec2[](
	vec2(-0.00033789, -0.00072656),
	vec2(0.00056445, -0.00064844),
	vec2(0.000013672, -0.00054883),
	vec2(-0.00058594, -0.00011914),
	vec2(-0.00017773, -0.00021094),
	vec2(0.00019336, -0.00019336),
	vec2(-0.000019531, -0.000058594),
	vec2(-0.00022070, 0.00014453),
	vec2(0.000089844, 0.000068359),
	vec2(0.00036328, 0.000058594),
	vec2(0.00061328, 0.000087891),
	vec2(-0.0000078125, 0.00028906),
	vec2(-0.00089453, 0.00043750),
	vec2(-0.00022852, 0.00058984),
	vec2(0.00019336, 0.00042188),
	vec2(0.00071289, -0.00025977)
);


vec3 toNonLinear(vec3 x)
{
	// Approximation to pow(x, 0.4545).  Max error of ~0.004 over [0, 1].
	return 0.124445006f*x*x + -0.35056138f*x + 1.2311935*sqrt(x);
}


float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2.0;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2.0)) * 0.5;
}


float sampleDynamicDepthMap(mat2 R, vec3 shadow_coords, float bias)
{
	// float actual_depth_0 = min(shadow_cds_0.z, 0.999f); // Cap so that if shadow depth map is max out at value 1, fragment will be considered to be unshadowed.

	// This technique blurs the shadows a bit, but works well to reduce swimming aliasing artifacts on shadow edges etc..
	/*
	float offset_scale = 1.f / 2048; // 2048 = tex res
	float sum = 0;
	for(int x=-1; x<=2; ++x)
		for(int y=-1; y<=2; ++y)
		{
			vec2 st = shadow_coords.xy + R * (vec2(x - 0.5f, y - 0.5f) * offset_scale);
			sum += texture(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z));
		}
	return sum * (1.f / 16);*/

	// This technique is a bit sharper:
	float sum = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(dynamic_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias));
	}
	return sum * (1.f / 16.f);
}


float sampleStaticDepthMap(mat2 R, vec3 shadow_coords, float bias)
{
	// This technique gives sharper shadows, so will use for static depth maps to avoid shadows on smaller objects being blurred away.
	float sum = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		vec2 st = shadow_coords.xy + R * samples[i];
		sum += texture(static_depth_tex, vec3(st.x, st.y, shadow_coords.z - bias));
	}
	return sum * (1.f / 16.f);
}


// Convert a non-linear sRGB colour to linear sRGB.
// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
vec3 toLinearSRGB(vec3 c)
{
	vec3 c2 = c * c;
	return c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
}


#if MATERIALISE_EFFECT

// https://www.shadertoy.com/view/MdcfDj
#define M1 1597334677U     //1719413*929
#define M2 3812015801U     //140473*2467*11
float hash( uvec2 q )
{
	q *= uvec2(M1, M2); 

	uint n = (q.x ^ q.y) * M1;

	return float(n) * (1.0/float(0xffffffffU));
}

// https://iquilezles.org/articles/functions/
float cubicPulse( float c, float w, float x )
{
	x = abs(x - c);
	if( x>w ) return 0.0;
	x /= w;
	return 1.0 - x*x*(3.0-2.0*x);
}

// https://www.shadertoy.com/view/cscSW8
#define SQRT_3 1.7320508

vec2 closestHexCentre(vec2 p)
{
	vec2 grid_p = vec2(p.x, p.y * (1.0 / SQRT_3));

	// Alternating rows of hexagon centres form their own rectanglular lattices.
	// Find closest hexagon centre on each lattice.
	vec2 p_1 = (floor((grid_p + vec2(1,1)) * 0.5) * 2.0            ) * vec2(1, SQRT_3);
	vec2 p_2 = (floor( grid_p              * 0.5) * 2.0 + vec2(1,1)) * vec2(1, SQRT_3);

	// Now return the closest centre from the two lattices.
	float d_1 = length2(p - p_1);
	float d_2 = length2(p - p_2);
	return d_1 < d_2 ? p_1 : p_2;
}


// Fraction along line from hexagon centre at (0,0) through p to hexagon edge.
// Also 1 - distance from boundary (I think)
float hexFracToEdge(in vec2 p)
{
	p = abs(p);
	float right_frac = p.x;
	float upper_right_frac = dot(p, vec2(0.5, 0.5*SQRT_3));
	return max(right_frac, upper_right_frac);
}
#endif // MATERIALISE_EFFECT



// Converts a unit vector to a point in octahedral representation ('oct').
// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 1.

// Returns +- 1
vec2 signNotZero(vec2 v) {
	return vec2(((v.x >= 0.0) ? 1.0 : -1.0), ((v.y >= 0.0) ? 1.0 : -1.0));
}
// Assume normalized input. Output is on [-1, 1] for each component.
vec2 float32x3_to_oct(in vec3 v) {
	// Project the sphere onto the octahedron, and then onto the xy plane
	vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
	// Reflect the folds of the lower hemisphere over the diagonals
	return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}



// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
vec3 snorm12x2_to_unorm8x3(vec2 f) {
	vec2 u = vec2(round(clamp(f, -1.0, 1.0) * 2047.0 + 2047.0));
	float t = floor(u.y / 256.0);
	// If storing to GL_RGB8UI, omit the final division
	return floor(vec3(u.x / 16.0,
		fract(u.x / 16.0) * 256.0 + t,
		u.y - t * 256.0)) / 255.0;
}


#if DECAL

vec3 oct_to_float32x3(vec2 e) {
	vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
	if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
	return normalize(v);
}

// 'A Survey of Efficient Representations for Independent Unit Vectors', listing 5.
vec2 unorm8x3_to_snorm12x2(vec3 u) {
	u *= 255.0;
	u.y *= (1.0 / 16.0);
	vec2 s = vec2(u.x * 16.0 + floor(u.y),
		fract(u.y) * (16.0 * 256.0) + u.z);
	return clamp(s * (1.0 / 2047.0) - 1.0, vec2(-1.0), vec2(1.0));
}

// See 'Calculations for recovering depth values from depth buffer' in OpenGLEngine.cpp
float getDepthFromDepthTexture(float px, float py)
{
#if USE_REVERSE_Z
	return near_clip_dist / texture(main_depth_texture, vec2(px, py)).x;
#else
	return -near_clip_dist / (texture(main_depth_texture, vec2(px, py)).x - 1.0);
#endif
}
#endif

vec3 removeComponentInDir(vec3 v, vec3 unit_dir)
{
	return v - unit_dir * dot(v, unit_dir);
}


void main()
{
#if USE_MULTIDRAW_ELEMENTS_INDIRECT
	//MaterialData mat_data = material_data[material_index];
#endif

	vec3 use_normal_ws;
	vec2 use_texture_coords = texture_coords;
	if((MAT_UNIFORM.flags & HAVE_SHADING_NORMALS_FLAG) != 0)
	{
		use_normal_ws = normal_ws;
	}
	else
	{
		vec3 dp_dx, dp_dy, N_g;
#if GENERATE_PLANAR_UVS
		// For voxels: Compute texture coords based on object-space geometric normal.
		dp_dx = dFdx(pos_os);
		dp_dy = dFdy(pos_os);
		vec3 N_g_os = cross(dp_dx, dp_dy);

		if(abs(N_g_os.x) > abs(N_g_os.y) && abs(N_g_os.x) > abs(N_g_os.z))
		{
			use_texture_coords.x = pos_os.y;
			use_texture_coords.y = pos_os.z;
			if(N_g_os.x < 0.0)
				use_texture_coords.x = -use_texture_coords.x;
		}
		else if(abs(N_g_os.y) > abs(N_g_os.x) && abs(N_g_os.y) > abs(N_g_os.z))
		{
			use_texture_coords.x = pos_os.x;
			use_texture_coords.y = pos_os.z;
			if(N_g_os.y > 0.0)
				use_texture_coords.x = -use_texture_coords.x;
		}
		else
		{
			use_texture_coords.x = pos_os.x;
			use_texture_coords.y = pos_os.y;
			if(N_g_os.z < 0.0)
				use_texture_coords.x = -use_texture_coords.x;
		}
#endif

		// Compute world-space geometric normal.
		dp_dx = dFdx(pos_ws);
		dp_dy = dFdy(pos_ws);
		N_g = cross(dp_dx, dp_dy);
		use_normal_ws = N_g;
	}

	// Get normal from normal map if we have one
	if((MAT_UNIFORM.flags & HAVE_NORMAL_MAP_FLAG) != 0)
	{
		vec2 st = MAT_UNIFORM.texture_upper_left_matrix_col0 * use_texture_coords.x + MAT_UNIFORM.texture_upper_left_matrix_col1 * use_texture_coords.y + MAT_UNIFORM.texture_matrix_translation;
		vec3 norm_map_v = texture(NORMAL_MAP, st).xyz;
		norm_map_v = norm_map_v * 2.0 - vec3(1.0);
#if VERT_TANGENTS
		use_normal_ws = normalize(
			tangent_ws    * norm_map_v.x + 
			bitangent_ws  * norm_map_v.y + 
			use_normal_ws * norm_map_v.z
		);
#else
		vec3 dp_dx = dFdx(pos_ws);
		vec3 dp_dy = dFdy(pos_ws);

		// ( ds/dx  ds/dy )
		// ( dt/dx  dt/dy )
		mat2 dst_dxy;
		dst_dxy[0] = dFdx(st);
		dst_dxy[1] = dFdy(st);

		// ( dx/ds  dx/dt )
		// ( dy/ds  dy/dt )
		mat2 dxy_dst = inverse(dst_dxy);

		// Compute dp/ds and dp/dt
		// dp/ds = dp/dx dx/ds + dp/dy dy/ds
		// dp/dt = dp/dx dx/dt + dp/dy dy/dt
		vec3 dp_ds = dp_dx * dxy_dst[0].x + dp_dy * dxy_dst[0].y;
		vec3 dp_dt = dp_dx * dxy_dst[1].x + dp_dy * dxy_dst[1].y;

		dp_ds = normalize(removeComponentInDir(dp_ds, use_normal_ws));
		dp_dt = normalize(removeComponentInDir(dp_dt, use_normal_ws));

		use_normal_ws = normalize(
			 dp_ds * norm_map_v.x +
			-dp_dt * norm_map_v.y +
			use_normal_ws * norm_map_v.z
		);
#endif
	}

	// float snow_frac = smoothstep(0.56, 0.6, normalize(use_normal_ws).z);
	// if(pos_ws.z < water_level_z - 3.0)
	// 	snow_frac = 0.0;

#if DECAL
	// snow_frac = 0;
#endif


#if DECAL
	vec4 decal_col;
	{
		// Compute world-space position and normal of existing fragment based on normal and depth buffer.

		// image coordinates of this fragment
		float px = pos_cs.x / -pos_cs.z * l_over_w + 0.5;
		float py = pos_cs.y / -pos_cs.z * l_over_h + 0.5;

		float dir_dot_forwards = -normalize(pos_cs).z;

		vec3 src_normal_encoded = texture(main_normal_texture, vec2(px, py)).xyz; // Encoded as a RGB8 texture (converted to floating point)
		vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture

		// cam_to_pos_ws = pos_ws - campos_ws
		// campos_ws = pos_ws - cam_to_pos_ws
		vec3 campos_ws = pos_ws - cam_to_pos_ws; // camera position to decal fragment position
		float depth = getDepthFromDepthTexture(px, py); // Get depth from depth buffer for existing fragment
		vec3 src_pos_ws = campos_ws + normalize(cam_to_pos_ws) / dir_dot_forwards * depth; // position in world space of existing fragment TODO: take into acount cos(theta)?

		vec3 pos_os = (world_to_ob * vec4(src_pos_ws, 1.0)).xyz; // Transform src position in world space into position in decal object space.

		use_texture_coords.x = pos_os.x;
		use_texture_coords.y = pos_os.y;
		if(pos_os.x < 0.0 || pos_os.x > 1.0 || pos_os.y < 0.0 || pos_os.y > 1.0 || pos_os.z < 0.0 || pos_os.z > 1.0)
			discard;

		vec3 src_normal_decal_space = normalize((world_to_ob * vec4(src_normal_ws, 0.0)).xyz);
		if(src_normal_decal_space.z < 0.1)
			discard;

		use_normal_ws = src_normal_ws;
	}
#endif


	vec3 unit_normal_ws = normalize(use_normal_ws);

	float light_cos_theta = dot(unit_normal_ws, sundir_ws.xyz);

#if DOUBLE_SIDED
	float sun_light_cos_theta_factor = abs(light_cos_theta);
#else
	float sun_light_cos_theta_factor = max(0.f, light_cos_theta);
#endif

	vec3 frag_to_cam_ws = normalize(-cam_to_pos_ws);

	// We will get two diffuse colours - one for the sun contribution, and one for reflected light from the same hemisphere the camera is in.
	// The sun contribution diffuse colour may use the transmission texture.  The reflected light colour may be the front or backface albedo texture.


	vec2 main_tex_coords = MAT_UNIFORM.texture_upper_left_matrix_col0 * use_texture_coords.x + MAT_UNIFORM.texture_upper_left_matrix_col1 * use_texture_coords.y + MAT_UNIFORM.texture_matrix_translation;

	vec4 sun_texture_diffuse_col;
	vec4 refl_texture_diffuse_col;
	if((MAT_UNIFORM.flags & HAVE_TEXTURE_FLAG) != 0)
	{
#if DOUBLE_SIDED
		// Work out if we are seeing the front or back face of the material
		float frag_to_cam_dot_normal = dot(frag_to_cam_ws, unit_normal_ws);
		if(frag_to_cam_dot_normal < 0.f)
		{
			refl_texture_diffuse_col = texture(BACKFACE_ALBEDO_TEX, main_tex_coords); // backface
			//refl_texture_diffuse_col.xyz = vec3(1,0,0);//TEMP
		}
		else
			refl_texture_diffuse_col = texture(DIFFUSE_TEX,          main_tex_coords); // frontface


		if(frag_to_cam_dot_normal * light_cos_theta <= 0.f) // If frag_to_cam_ws and sundir_ws are in different geometric hemispheres:
		{
			sun_texture_diffuse_col = texture(TRANSMISSION_TEX, main_tex_coords);
			//sun_texture_diffuse_col.xyz = vec3(1,0,0);//TEMP
		}
		else
			sun_texture_diffuse_col = refl_texture_diffuse_col; // Else sun is illuminating the face facing the camera.
#else
		sun_texture_diffuse_col = texture(DIFFUSE_TEX, main_tex_coords);
		refl_texture_diffuse_col = sun_texture_diffuse_col;
#endif

#if CONVERT_ALBEDO_FROM_SRGB
		// Texture value is in non-linear sRGB, convert to linear sRGB.
		// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
		vec4 c = sun_texture_diffuse_col;
		vec4 c2 = c * c;
		sun_texture_diffuse_col = c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;

		c = refl_texture_diffuse_col;
		c2 = c * c;
		refl_texture_diffuse_col = c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
#endif
	}
	else
	{
		sun_texture_diffuse_col = vec4(1.f);
		refl_texture_diffuse_col = vec4(1.f);
	}

	// Final diffuse colour = texture diffuse colour * constant diffuse colour
	vec4 sun_diffuse_col  = sun_texture_diffuse_col  * MAT_UNIFORM.diffuse_colour; // diffuse_colour is linear sRGB already.
	vec4 refl_diffuse_col = refl_texture_diffuse_col * MAT_UNIFORM.diffuse_colour;


#if TERRAIN
	// NOTE: Simplified a bit to stop crashing Chrome's GPU process.
	// Removed rock (wasn't visible anyway) and vegetation colour variation.

	vec2 mask_coords = main_tex_coords + vec2(0.5 + 0.5 / 1024.0, 0.5 + 0.5 / 1024.0);
	vec4 mask = texture(DIFFUSE_TEX, mask_coords);
	
	//vec2 detail_map_0_uvs = main_tex_coords * (8.0 * 1024.0 / 8.0);
	vec2 detail_map_1_uvs = main_tex_coords * (8.0 * 1024.0 / 4.0);
	vec2 detail_map_2_uvs = main_tex_coords * (8.0 * 1024.0 / 4.0);

	vec4 detail_0_texval = vec4(0.f);
	vec4 detail_1_texval = vec4(0.f);
	vec4 detail_2_texval = vec4(0.f);
	//if(mask.x > 0.0)
		// detail_0_texval =  texture(detail_tex_0, detail_map_0_uvs); // Rock
	//if(mask.y > 0.0)
		detail_1_texval =  texture(detail_tex_1, detail_map_1_uvs); // Sediment
	//if(mask.z > 0.0)
		detail_2_texval =  texture(detail_tex_2, detail_map_2_uvs);  // CRASH!!!!  detail_tex_2 crashes!   // vegetation

	//float rock_heightmap_val = texture(detail_heightmap_0, detail_map_0_uvs).x;

	float non_beach_factor = smoothstep(water_level_z + 2.0, water_level_z + 3.0, pos_ws.z);
	float beach_factor = 1.0 - non_beach_factor;

	float rock_weight = 0.0; // Disable any rock for now.
	//float rock_weight_env = smoothstep(0.2, 0.6, mask.x + fbmMix(detail_map_2_uvs * 0.2) * 0.2);
	//float rock_height = rock_heightmap_val * rock_weight_env;
	//float rock_weight = (rock_height > 0.1/*|| normal_ws.z <  0.5*/) ? 1.f : 0.f;

	//float veg_frac = mask.z > texture(fbm_tex, detail_map_2_uvs).x ? 1.0 : 0.0;
	// Vegetation as a fraction of (vegetation + sediment)
	float veg_frac = ((mask.z > fbmMix(detail_map_2_uvs * 0.2) * 0.3 + 0.5 + beach_factor) ? 1.0 : 0.0);

	float sed_weight = (1.0 - rock_weight) * (1.0 - veg_frac); // (mask.y / (mask.y + mask.z));
	float veg_weight = (1.0 - rock_weight) * veg_frac; // (mask.z / (mask.y + mask.z));

	//float col_variation_amt = 0.1;
	//vec4 colour_variation_factor = vec4(
	//	1.0 + fbmMix(detail_map_2_uvs * 0.026546) * col_variation_amt, 
	//	1.0 + fbmMix(detail_map_2_uvs * 0.016546) * col_variation_amt, 
	//	1.0,
	//	1.0);

	vec4 texcol = vec4(0.f);
	//texcol += detail_0_texval * rock_weight;
	texcol += detail_1_texval * sed_weight;
	texcol += detail_2_texval * veg_weight; // TEMP disabled colour variation.    * colour_variation_factor * veg_weight;
	texcol.w = 1.0;

	sun_diffuse_col  = texcol;
	refl_diffuse_col = texcol;
#endif

	// float snow_tex = 0.5 + texture(snow_ice_normal_map, pos_ws.xy * 0.25).x;
	// vec4 snow_albedo = vec4(snow_tex, snow_tex, snow_tex, 1);
	// sun_diffuse_col  = mix(sun_diffuse_col,  snow_albedo, snow_frac);
	// refl_diffuse_col = mix(refl_diffuse_col, snow_albedo, snow_frac);

	// Apply vertex colour, if enabled.
#if VERT_COLOURS
	vec3 linear_vert_col = toLinearSRGB(vert_colour);
	sun_diffuse_col.xyz *= linear_vert_col;
	refl_diffuse_col.xyz *= linear_vert_col;
#endif

	float pixel_hash = texture(blue_noise_tex, gl_FragCoord.xy * (1.f / 128.f)).x;
	// Fade out (to fade in an imposter), if enabled.
#if IMPOSTERABLE
	float dist_alpha_factor = smoothstep(MAT_UNIFORM.begin_fade_out_distance, MAT_UNIFORM.end_fade_out_distance,  /*dist=*/-pos_cs.z);
	if(dist_alpha_factor > pixel_hash)
		discard;
#endif

#if ALPHA_TEST && !DECAL // For decals we will use alpha blending instead of discarding.
	if(refl_diffuse_col.a < 0.5f)
		discard;
#endif

#if DRAW_PLANAR_UV_GRID
	float du_dx = abs(dFdx(use_texture_coords.x));
	float du_dy = abs(dFdy(use_texture_coords.x));
	
	float dv_dx = abs(dFdx(use_texture_coords.y));
	float dv_dy = abs(dFdy(use_texture_coords.y));
	
	float a = max(du_dx, du_dy);
	float b = max(dv_dx, dv_dy);

	float border_w_u = max(0.01f, a * 0.5f);
	float border_w_v = max(0.01f, b * 0.5f);
	if(	fract(use_texture_coords.x) < border_w_u || fract(use_texture_coords.x) >= (1.0 - border_w_u) ||
		fract(use_texture_coords.y) < border_w_v || fract(use_texture_coords.y) >= (1.0 - border_w_v))
	{
		refl_diffuse_col = vec4(0.2f, 0.8f, 0.54f, 1.f);
		sun_diffuse_col = vec4(0.2f, 0.8f, 0.54f, 1.f);
	}
#endif

	vec3 unit_cam_to_pos_ws = normalize(cam_to_pos_ws);

	// Flip normal into hemisphere camera is in.
	if(dot(unit_normal_ws, cam_to_pos_ws) > 0.0)
		unit_normal_ws = -unit_normal_ws;

	float final_metallic_frac = ((MAT_UNIFORM.flags & HAVE_METALLIC_ROUGHNESS_TEX_FLAG) != 0) ? (MAT_UNIFORM.metallic_frac * texture(METALLIC_ROUGHNESS_TEX, main_tex_coords).b) : MAT_UNIFORM.metallic_frac;

	// final_metallic_frac *= (1.f - snow_frac);

	float unclamped_roughness = ((MAT_UNIFORM.flags & HAVE_METALLIC_ROUGHNESS_TEX_FLAG) != 0) ? (MAT_UNIFORM.roughness     * texture(METALLIC_ROUGHNESS_TEX, main_tex_coords).g) : MAT_UNIFORM.roughness;
	float final_roughness = max(0.04, unclamped_roughness); // Avoid too small roughness values resulting in glare/bloom artifacts

	// final_roughness = mix(final_roughness, 0.6f, snow_frac);

	float final_fresnel_scale = MAT_UNIFORM.fresnel_scale * (1.0 - square(final_roughness)); // Reduce fresnel reflection at higher roughnesses

	//----------------------- Direct lighting from interior lights ----------------------------
	// Load indices into a local array, so we can iterate over the array in a for loop.  TODO: find a better way of doing this.
	int indices[8];
	indices[0] = light_indices_0.x;
	indices[1] = light_indices_0.y;
	indices[2] = light_indices_0.z;
	indices[3] = light_indices_0.w;
	indices[4] = light_indices_1.x;
	indices[5] = light_indices_1.y;
	indices[6] = light_indices_1.z;
	indices[7] = light_indices_1.w;

	vec4 local_light_radiance = vec4(0.f);
	for(int i=0; i<8; ++i)
	{
		int light_index = indices[i];
		if(light_index >= 0)
		{
			vec3 light_emitted_radiance = light_data[light_index].col.xyz;

			vec3 pos_to_light = light_data[light_index].pos.xyz - pos_ws;
			float pos_to_light_len2 = dot(pos_to_light, pos_to_light);
			vec3 unit_pos_to_light = pos_to_light * inversesqrt(pos_to_light_len2);

			float dir_factor;
			if(light_data[light_index].light_type == 0) // Point light:
			{
				dir_factor = 1.0;
			}
			else
			{
				// light_type == 1: spotlight
				float from_light_cos_angle = -dot(light_data[light_index].dir.xyz, unit_pos_to_light);
				dir_factor =
					smoothstep(0.4f, 0.9f, from_light_cos_angle) * 0.03 + // A little light outside of the main cone
					smoothstep(light_data[light_index].cone_cos_angle_start, light_data[light_index].cone_cos_angle_end, from_light_cos_angle);
			}

			float cos_theta_term = max(0.f, dot(unit_normal_ws, unit_pos_to_light));

			vec3 diffuse_bsdf = refl_diffuse_col.xyz * (1.0 / 3.141592653589793);

			// Compute specular bsdf
			vec3 h_ws = normalize(unit_pos_to_light - unit_cam_to_pos_ws);
			float h_cos_theta = abs(dot(h_ws, unit_normal_ws));
			vec4 specular_fresnel;
			{
				vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, 1.5)) * final_fresnel_scale;
				vec4 metal_fresnel = vec4(
					metallicFresnelApprox(h_cos_theta, refl_diffuse_col.r),
					metallicFresnelApprox(h_cos_theta, refl_diffuse_col.g),
					metallicFresnelApprox(h_cos_theta, refl_diffuse_col.b),
					1);

				// Blend between metal_fresnel and dielectric_fresnel based on metallic_frac.
				specular_fresnel = mix(dielectric_fresnel, metal_fresnel, final_metallic_frac);
			}

			vec4 specular = trowbridgeReitzPDF(h_cos_theta, alpha2ForRoughness(final_roughness)) * specular_fresnel;

			vec3 bsdf = diffuse_bsdf + specular.xyz;
			vec3 reflected_radiance = bsdf * cos_theta_term * light_emitted_radiance * dir_factor / pos_to_light_len2;

			local_light_radiance.xyz += reflected_radiance;
		}
	}


	//------------- Compute specular microfacet terms for sun lighting --------------
	vec3 h_ws = normalize(frag_to_cam_ws + sundir_ws.xyz);

	vec4 specular = vec4(0.0);
	if(light_cos_theta > -0.3)
	{
		float shadow_factor = smoothstep(-0.3, 0.0, light_cos_theta);
		//float h_cos_theta = max(0.0, dot(h, unit_normal_cs));
		float h_cos_theta = abs(dot(h_ws, unit_normal_ws));
		vec4 specular_fresnel;
		{
			vec4 dielectric_fresnel = vec4(fresnelApprox(h_cos_theta, 1.5)) * final_fresnel_scale;
			vec4 metal_fresnel = vec4(
				metallicFresnelApprox(h_cos_theta, refl_diffuse_col.r),
				metallicFresnelApprox(h_cos_theta, refl_diffuse_col.g),
				metallicFresnelApprox(h_cos_theta, refl_diffuse_col.b),
				1);

			// Blend between metal_fresnel and dielectric_fresnel based on metallic_frac.
			specular_fresnel = mix(dielectric_fresnel, metal_fresnel, final_metallic_frac);
		}

		specular = trowbridgeReitzPDF(h_cos_theta, alpha2ForRoughness(final_roughness)) * specular_fresnel * shadow_factor;
	}


	// Shadow mapping
	float sun_vis_factor = 0.0;
#if SHADOW_MAPPING

#define VISUALISE_CASCADES 0

	if(sun_light_cos_theta_factor != 0.0) // Avoid doing shadow map lookups for faces facing away from sun.
	{
		float samples_scale = 1.f;

		float pattern_theta = pixel_hash * 6.283185307179586f;
		mat2 R = mat2(cos(pattern_theta), sin(pattern_theta), -sin(pattern_theta), cos(pattern_theta));

		sun_vis_factor = 0.0;

		float depth_map_0_bias = 8.0e-5f;
		float depth_map_1_bias = 4.0e-4f;
		float static_depth_map_bias = 2.2e-3f;

		float dist = -pos_cs.z;
		if(dist < DEPTH_TEXTURE_SCALE_MULT*DEPTH_TEXTURE_SCALE_MULT)
		{
			if(dist < DEPTH_TEXTURE_SCALE_MULT) // if dynamic_depth_tex_index == 0:
			{
				float tex_0_vis = sampleDynamicDepthMap(R, shadow_tex_coords[0], /*bias=*/depth_map_0_bias);

				float edge_dist = 0.8f * DEPTH_TEXTURE_SCALE_MULT;
				if(dist > edge_dist)
				{
					float tex_1_vis = sampleDynamicDepthMap(R, shadow_tex_coords[1], /*bias=*/depth_map_1_bias);

					float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT, dist);
					sun_vis_factor = mix(tex_0_vis, tex_1_vis, blend_factor);
				}
				else
					sun_vis_factor = tex_0_vis;

#if VISUALISE_CASCADES
				refl_diffuse_col.yz *= 0.5f;
#endif
			}
			else
			{
				sun_vis_factor = sampleDynamicDepthMap(R, shadow_tex_coords[1], /*bias=*/depth_map_1_bias);

				float edge_dist = 0.6f * (DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT);

				// Blending with static shadow map 0
				if(dist > edge_dist)
				{
					vec3 static_shadow_cds = shadow_tex_coords[NUM_DYNAMIC_DEPTH_TEXTURES];

					float static_sun_vis_factor = sampleStaticDepthMap(R, static_shadow_cds, static_depth_map_bias); // NOTE: had 0.999f cap and bias of 0.0005: min(static_shadow_cds.z, 0.999f) - bias

					float blend_factor = smoothstep(edge_dist, DEPTH_TEXTURE_SCALE_MULT * DEPTH_TEXTURE_SCALE_MULT, dist);
					sun_vis_factor = mix(sun_vis_factor, static_sun_vis_factor, blend_factor);
				}

#if VISUALISE_CASCADES
				refl_diffuse_col.yz *= 0.75f;
#endif
			}
		}
		else
		{
			float l1dist = dist;
	
			if(l1dist < 1024.0)
			{
				int static_depth_tex_index;
				float cascade_end_dist;
				vec3 shadow_cds, next_shadow_cds;
				if(l1dist < 64.0)
				{
					static_depth_tex_index = 0;
					cascade_end_dist = 64.0;
					shadow_cds      = shadow_tex_coords[0 + NUM_DYNAMIC_DEPTH_TEXTURES];
					next_shadow_cds = shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
				}
				else if(l1dist < 256.0)
				{
					static_depth_tex_index = 1;
					cascade_end_dist = 256.0;
					shadow_cds      = shadow_tex_coords[1 + NUM_DYNAMIC_DEPTH_TEXTURES];
					next_shadow_cds = shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
				}
				else
				{
					static_depth_tex_index = 2;
					cascade_end_dist = 1024.0;
					shadow_cds = shadow_tex_coords[2 + NUM_DYNAMIC_DEPTH_TEXTURES];
					next_shadow_cds = vec3(0.0); // Suppress warning about being possibly uninitialised.
				}

				sun_vis_factor = sampleStaticDepthMap(R, shadow_cds, static_depth_map_bias); // NOTE: had cap and bias

#if DO_STATIC_SHADOW_MAP_CASCADE_BLENDING
				if(static_depth_tex_index < NUM_STATIC_DEPTH_TEXTURES - 1)
				{
					float edge_dist = 0.7f * cascade_end_dist;

					// Blending with static shadow map static_depth_tex_index + 1
					if(l1dist > edge_dist)
					{
						float next_sun_vis_factor = sampleStaticDepthMap(R, next_shadow_cds, static_depth_map_bias); // NOTE: had cap and bias

						float blend_factor = smoothstep(edge_dist, cascade_end_dist, l1dist);
						sun_vis_factor = mix(sun_vis_factor, next_sun_vis_factor, blend_factor);
					}
				}
#endif

#if VISUALISE_CASCADES
				refl_diffuse_col.xz *= float(static_depth_tex_index) / NUM_STATIC_DEPTH_TEXTURES;
#endif
			}
			else
				sun_vis_factor = 1.0;
		}
	} // end if sun_light_cos_theta_factor != 0

#else // else if !SHADOW_MAPPING:
	sun_vis_factor = 1.0;
#endif

	// Apply cloud shadows
#if RENDER_CLOUD_SHADOWS
	if(((mat_common_flags & CLOUD_SHADOWS_FLAG) != 0) && pos_ws.z < 1000.f && sun_vis_factor != 0.0) // If below cloud layer, and sunlight factor is not already zero:
	{
		// Compute position on cumulus cloud layer
		vec3 cum_layer_pos = pos_ws + sundir_ws.xyz * (1000.f - pos_ws.z) / sundir_ws.z;

		vec2 cum_tex_coords = vec2(cum_layer_pos.x, cum_layer_pos.y) * 1.0e-4f;
		cum_tex_coords.x += time * 0.002;

		vec2 cumulus_coords = vec2(cum_tex_coords.x * 2.0 + 2.3453, cum_tex_coords.y * 2.0 + 1.4354);
		float cumulus_val = max(0.f, fbmMix(cumulus_coords) - 0.3f);

		float cumulus_trans = max(0.f, 1.f - cumulus_val * 1.4);
		sun_vis_factor *= cumulus_trans;
	}
#endif


	

	vec4 sky_irradiance;
#if LIGHTMAPPING
	// glTexImage2D expects the start of the texture data to be the lower left of the image, whereas it is actually the upper left.  So flip y coord to compensate.
	sky_irradiance = texture(LIGHTMAP_TEX, vec2(lightmap_coords.x, -lightmap_coords.y));
#else
	sky_irradiance = texture(cosine_env_tex, unit_normal_ws.xyz); // integral over hemisphere of cosine * incoming radiance from sky * 1.0e-9
#endif

#if BLOB_SHADOWS
	for(int i=0; i<num_blob_positions; ++i)
	{
		vec3 pos_to_blob_centre = blob_positions[i].xyz - pos_ws;
		float r = 0.4;
		float r2 = r * r;
		float d2 = dot(pos_to_blob_centre, pos_to_blob_centre); // d^2
	
//		float bl = max(0.f, dot(unit_normal_ws, normalize(pos_to_blob_centre))) * min(1.f, r2 / d2);
		float bl = max(0.f, dot(unit_normal_ws, pos_to_blob_centre) * inversesqrt(d2)) * min(1.f, r2 / d2);
	
		float vis = 1.0 - bl * 0.7f;
		sky_irradiance *= vis;
		local_light_radiance *= vis;
	}
#endif


	// Reflect cam-to-fragment vector in ws normal
	vec3 reflected_dir_ws = unit_cam_to_pos_ws - unit_normal_ws * (2.0 * dot(unit_normal_ws, unit_cam_to_pos_ws));

	//========================= Do specular reflection of environment, weighted by fresnel factor ============================
	
	// Look up env map for reflected dir
	int map_lower = int(final_roughness * 6.9999);
	int map_higher = map_lower + 1;
	float map_t = final_roughness * 6.9999 - float(map_lower);

	float refl_theta = acos(reflected_dir_ws.z);
	float refl_phi = atan(reflected_dir_ws.y, reflected_dir_ws.x) - env_phi; // env_phi term is to rotate reflection so it aligns with env rotation.
	vec2 refl_map_coords = vec2(refl_phi * (1.0 / 6.283185307179586), clamp(refl_theta * (1.0 / 3.141592653589793), 1.0 / 64.0, 1.0 - 1.0 / 64.0)); // Clamp to avoid texture coord wrapping artifacts.

	vec4 spec_refl_light_lower  = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_lower)  * (1.0/8.0) + refl_map_coords.y * (1.0/8.0))); //  -refl_map_coords / 8.0 + map_lower  * (1.0 / 8)));
	vec4 spec_refl_light_higher = texture(specular_env_tex, vec2(refl_map_coords.x, float(map_higher) * (1.0/8.0) + refl_map_coords.y * (1.0/8.0)));
	vec4 spec_refl_light = spec_refl_light_lower * (1.0 - map_t) + spec_refl_light_higher * map_t; // spectral radiance * 1.0e-9


	float fresnel_cos_theta = max(0.0, dot(reflected_dir_ws, unit_normal_ws));
	vec4 dielectric_refl_fresnel = vec4(fresnelApprox(fresnel_cos_theta, 1.5) * final_fresnel_scale);
	vec4 metallic_refl_fresnel = vec4(
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.r),
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.g),
		metallicFresnelApprox(fresnel_cos_theta, refl_diffuse_col.b),
		1)/* * fresnel_scale*/;

	vec4 refl_fresnel = metallic_refl_fresnel * final_metallic_frac + dielectric_refl_fresnel * (1.0f - final_metallic_frac);

	//========================= Emission ============================
	vec4 emission_col = MAT_UNIFORM.emission_colour;
	if((MAT_UNIFORM.flags & HAVE_EMISSION_TEX_FLAG) != 0)
	{
		emission_col *= texture(EMISSION_TEX, main_tex_coords);
	}



	vec4 sun_light = sun_spec_rad_times_solid_angle * sun_vis_factor; // sun_spec_rad_times_solid_angle is Sun spectral radiance multiplied by solid angle

	
#if MATERIALISE_EFFECT
	// box mapping
	vec2 materialise_coords;
	if(abs(normal_ws.x) > abs(normal_ws.y))
	{
		if(abs(normal_ws.x) > abs(normal_ws.z)) // |x| > |z| && |x| > |y|
			materialise_coords = pos_ws.yz;
		else // |z| >= |x| > |y|:
			materialise_coords = pos_ws.xy;
	}
	else // else |y| >= |x|:
	{
		if(abs(normal_ws.y) > abs(normal_ws.z))
			materialise_coords = pos_ws.xz;
		else // |z| >= |y| >= |x|
			materialise_coords = pos_ws.xy;
	}

	float sweep_speed_factor = 1.0;
	float sweep_frac = (pos_ws.z - MAT_UNIFORM.materialise_lower_z) / (MAT_UNIFORM.materialise_upper_z - MAT_UNIFORM.materialise_lower_z);
	float materialise_stage = fbmMix(materialise_coords * 0.2) * 0.4 + sweep_frac;
	float use_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor - materialise_stage - 0.3;

	float band_1_centre = 0.1;
	float band_2_centre = 0.8;
	float band_1 = cubicPulse(/*centre*/band_1_centre, /*width=*/0.06, use_frac);
	float band_2 = cubicPulse(/*centre*/band_2_centre, /*width=*/0.06, use_frac);

	if(use_frac < band_1_centre)
		discard;
	if(use_frac >= band_1_centre + 0.05 && use_frac < band_2_centre)
	{
		vec2 hex_centre = closestHexCentre(materialise_coords * 40.0);
		float hex_frac = hexFracToEdge(hex_centre - materialise_coords * 40.0);

		float materialise_pixel_hash = hash(uvec2(hex_centre + vec2(100000.0,100000.0)));

		float hex_interior_frac = (time - MAT_UNIFORM.materialise_start_time) * sweep_speed_factor;// - materialise_stage - 0.3;
		if(hex_interior_frac < materialise_pixel_hash + /*fill-in delay=*/0.7 + sweep_frac)
			discard;

		emission_col =  vec4(0.0,0.2,0.5, 0) * //vec4(MAT_UNIFORM.materialise_r, MAT_UNIFORM.materialise_g, MAT_UNIFORM.materialise_b, 0.0) * 
			smoothstep(0.8, 0.9, hex_frac) * 4.0;
	}

	emission_col += (band_1 * vec4(1,1,1.0,0)  + band_2 * vec4(0.0,1,0.5,0)) * 10.0;
#endif // MATERIALISE_EFFECT


	vec4 col =
		sky_irradiance * sun_diffuse_col * (1.0 / 3.141592653589793) * (1.0 - refl_fresnel) * (1.0 - final_metallic_frac) +  // Diffuse substrate part of BRDF * incoming radiance from sky
		refl_fresnel * spec_refl_light + // Specular reflection of sky
		sun_light * (1.0 - refl_fresnel) * (1.0 - final_metallic_frac) * refl_diffuse_col * (1.0 / 3.141592653589793) * sun_light_cos_theta_factor + //  Diffuse substrate part of BRDF * sun light
		sun_light * specular + // sun light * specular microfacet terms
		local_light_radiance + // Reflected light from local light sources.
		emission_col;
	
	//vec4 col = (sun_light + 3000000000.0)  * diffuse_col;

#if DEPTH_FOG
	// Blend with background/fog colour
	float dist_ = max(0.0, -pos_cs.z); // Max with 0 avoids bright artifacts on horizon.
	vec3 transmission = exp(air_scattering_coeffs.xyz * -dist_);

	col.xyz *= transmission;
	col.xyz += sun_and_sky_av_spec_rad.xyz * (1.0 - transmission); // Add in-scattered sky+sunlight
#endif

	//------------------------------- Apply underwater effects ---------------------------
#if UNDERWATER_CAUSTICS
	// campos_ws + cam_to_pos_ws = pos_ws
	// campos_ws = pos_ws - cam_to_pos_ws;

	float campos_z = pos_ws.z - cam_to_pos_ws.z;
	if(/*(campos_z < -3.8) && */pos_ws.z < water_level_z)
	{
		vec3 extinction = vec3(1.0, 0.10, 0.1) * 2.0;
		vec3 scattering = vec3(0.4, 0.4, 0.1);

		vec3 src_col = col.xyz; // texture(main_colour_texture, vec2(refracted_px, refracted_py)).xyz * (1.0 / 0.000000003); // Get colour value at refracted ground position, undo tonemapping.

		//vec3 src_normal_encoded = texture(main_normal_texture, vec2(refracted_px, refracted_py)).xyz; // Encoded as a RGB8 texture (converted to floating point)
		//vec3 src_normal_ws = oct_to_float32x3(unorm8x3_to_snorm12x2(src_normal_encoded)); // Read normal from normal texture

		//--------------- Apply caustic texture ---------------
		// Caustics are projected onto a plane normal to the direction to the sun.
		vec3 sun_right = normalize(cross(sundir_ws.xyz, vec3(0.0,0.0,1.0)));
		vec3 sun_up = cross(sundir_ws.xyz, sun_right);
		vec2 hitpos_sunbasis = vec2(dot(pos_ws, sun_right), dot(pos_ws, sun_up));

		float sun_lambert_factor = max(0.0, dot(normal_ws, sundir_ws.xyz));

		// Distance from water surface to ground, along the sun direction.  Used for computing the caustic effect envelope.
		float water_to_ground_sun_d = max(0.0, (water_level_z - pos_ws.z) / sundir_ws.z); // TEMP HACK Assuming water surface height

		float cam_to_pos_dist = length(cam_to_pos_ws);

		float total_path_dist = water_to_ground_sun_d + cam_to_pos_dist;

		float caustic_depth_factor = 0.03 + 0.9 * (smoothstep(0.1, 2.0, water_to_ground_sun_d) - 0.8 *smoothstep(2.0, 8.0, water_to_ground_sun_d)); // Caustics should not be visible just under the surface.
		float caustic_frac = fract(time * 24.0); // Get fraction through frame, assuming 24 fps.
		float scale_factor = 1.0; // Controls width of caustic pattern in world space.
		// Interpolate between caustic animation frames
		vec3 caustic_val = mix(texture(caustic_tex_a, hitpos_sunbasis * scale_factor),  texture(caustic_tex_b, hitpos_sunbasis * scale_factor), caustic_frac).xyz;

		// Since the caustic is focused light, we should dim the src texture slightly between the focused caustic light areas.
		src_col *= mix(vec3(1.0), vec3(0.3, 0.5, 0.7) + vec3(3.0, 1.0, 0.8) * caustic_val * 7.0, caustic_depth_factor * sun_lambert_factor);

		if(campos_z > water_level_z)
		{
			// If camera is above water:
			// Don't apply absorption on the edge between object and camera, we will do that in water shader.

			col = vec4(src_col, 1.0);
		}
		else // Else if camera is underwater:
		{
			// NOTE: this calculation is also in colourForUnderwaterPoint() in water_frag_shader.glsl and should be kept in sync.
			vec3 inscatter_radiance_sigma_s_over_sigma_t = sun_and_sky_av_spec_rad.xyz * vec3(0.004, 0.015, 0.03) * 3.0;
			vec3 exp_optical_depth = exp(extinction * -cam_to_pos_dist);
			vec3 inscattering = inscatter_radiance_sigma_s_over_sigma_t * (vec3(1.0) - exp_optical_depth);

	
			vec3 attentuated_col = src_col * exp_optical_depth;

			col = vec4(attentuated_col + inscattering, 1.0);
		}
	}
#endif // UNDERWATER_CAUSTICS
	//------------------------------- End apply underwater effects ---------------------------


	col *= 3.0; // tone-map
	
#if DO_POST_PROCESSING
	colour_out = vec4(col.xyz, 1); // toNonLinear will be done after adding blurs etc.
#else
	colour_out = vec4(toNonLinear(col.xyz), 1);
#endif

#if DECAL
	// materialise_start_time = particle spawn time
	// materialise_upper_z = dopacity/dt
	float life_time = time - MAT_UNIFORM.materialise_start_time;
	float overall_alpha_factor = max(0.0, min(1.0, refl_diffuse_col.w + life_time * MAT_UNIFORM.materialise_upper_z));

	colour_out.w = overall_alpha_factor;
#endif

	normal_out = snorm12x2_to_unorm8x3(float32x3_to_oct(unit_normal_ws));
}
