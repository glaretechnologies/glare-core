

uniform sampler2D terrain_height_map;
uniform sampler2D terrain_mask_tex;
uniform sampler2D fbm_tex;
uniform sampler2D detail_mask_tex;

#define DETAIL_MASK_MAP_WIDTH_M		256.0


// Needs to be same as PrecomputedPoint in TerrainScattering.h
struct PrecomputedPoint
{
	float u, v;
	float scale_factor;
	float rot;
};


#define SIZEOF_VERTEXSTRUCT			(7 * 4)
struct VertexStruct
{
	float pos_x, pos_y, pos_z;
	float imposter_width;
	float u, v;
	float imposter_rot;
};


// Needs to match ShaderChunkInfo in TerrainSystem.cpp
struct ChunkInfo
{
	int chunk_x_index;
	int chunk_y_index;
	float chunk_w_m;
	float base_scale;
	float imposter_width_over_height;
	float terrain_scale_factor;
	uint vert_data_offset_B;
};



layout(std430) buffer VertexData
{
	VertexStruct vertices[];
};

layout(std430) buffer PrecomputedPoints
{
	PrecomputedPoint precomputed_points[];
};

layout(std430) buffer ChunkInfoData
{
	ChunkInfo chunk_info;
};


float fbm(vec2 p)
{
	return (texture(fbm_tex, p).x - 0.5) * 2.f;
}

vec2 rot(vec2 p)
{
	float theta = 1.618034 * 3.141592653589 * 2;
	return vec2(cos(theta) * p.x - sin(theta) * p.y, sin(theta) * p.x + cos(theta) * p.y);
}

float fbmMix(vec2 p)
{
	return 
		fbm(p) +
		fbm(rot(p * 2)) * 0.5 +
		0;
}


float square(float x) { return x*x; }

// See MitchellNetravali.h
float mitchellNetravaliEval(float x)
{
	float B = 0.5f;
	float C = 0.25f;

	float region_0_a = (float(12)  - B*9  - C*6) * (1.f/6);
	float region_0_b = (float(-18) + B*12 + C*6) * (1.f/6);
	float region_0_d = (float(6)   - B*2       ) * (1.f/6);

	float region_1_a = (-B - C*6)                * (1.f/6);
	float region_1_b = (B*6 + C*30)              * (1.f/6);
	float region_1_c = (B*-12 - C*48)            * (1.f/6);
	float region_1_d = (B*8 + C*24)              * (1.f/6);

	float region_0_f = region_0_a * (x*x*x) + region_0_b * (x*x) + region_0_d;
	float region_1_f = region_1_a * (x*x*x) + region_1_b * (x*x) + region_1_c * x + region_1_d;
	if(x < 1.0)
		return region_0_f;
	else if(x < 2.0)
		return region_1_f;
	else
		return 0.0;
}


/*
sampleHeightmapHighQual
-----------------------
See See ImageMap::sampleSingleChannelHighQual


               ^ y
               |
   floor(py)+2 |  *            *            *              *
               |
               |
               |
               |
   floor(py)+1 |  *            *            *              *
               |
               |                  +
               |                (px, py)
               |
   floor(py)   |  *            *            *              *
               |
               |
               |
               |
   floor(py)-1 |  *            *            *              *    
               |
               |------------------------------------------------>x 
                  |            |            |              |
                floor(px)-1    floor(px)    floor(px)+1     floor(px)+2
*/
float sampleHeightmapHighQual(float u, float v)
{
	// Get fractional normalised image coordinates
	vec2 normed_coords = vec2(u, /*-*/v); // Normalised coordinates with v flipped, to go from +v up to +v down.
	vec2 normed_frac_part = normed_coords - floor(normed_coords); // Fractional part of normed coords, in [0, 1].

	int width  = 1024;
	int height = 1024;

	//Vec4f f_pixels = mul(normed_frac_part, toVec4f(dims)); // unnormalised floating point pixel coordinates (pixel_x, pixel_y), in [0, width] x [0, height]  [float]
	float f_pixels_x = normed_frac_part.x * width;
	float f_pixels_y = normed_frac_part.y * height;

	int i_pixels_clamped_x = max(0, int(f_pixels_x)); // truncate pixel coords to integers and clamp to >= 0.
	int i_pixels_clamped_y = max(0, int(f_pixels_y));

	int i_pixels_x = min(i_pixels_clamped_x, width  - 1); // clamp to <= (width-1, height-1).
	int i_pixels_y = min(i_pixels_clamped_y, height - 1);

	int ut = i_pixels_x;
	int vt = i_pixels_y;
	int ut_1 = min(i_pixels_clamped_x + 1, width  - 1);
	int vt_1 = min(i_pixels_clamped_y + 1, height - 1);
	int ut_2 = min(i_pixels_clamped_x + 2, width  - 1);
	int vt_2 = min(i_pixels_clamped_y + 2, height - 1);
	int ut_minus_1 = max(i_pixels_clamped_x - 1, 0);
	int vt_minus_1 = max(i_pixels_clamped_y - 1, 0);


	const float  v0 = texelFetch(terrain_height_map, ivec2(ut_minus_1, vt_minus_1), /*lod=*/0).x;
	const float  v1 = texelFetch(terrain_height_map, ivec2(ut,         vt_minus_1), /*lod=*/0).x;
	const float  v2 = texelFetch(terrain_height_map, ivec2(ut_1,       vt_minus_1), /*lod=*/0).x;
	const float  v3 = texelFetch(terrain_height_map, ivec2(ut_2,       vt_minus_1), /*lod=*/0).x;
	const float  v4 = texelFetch(terrain_height_map, ivec2(ut_minus_1, vt        ), /*lod=*/0).x;
	const float  v5 = texelFetch(terrain_height_map, ivec2(ut,         vt        ), /*lod=*/0).x;
	const float  v6 = texelFetch(terrain_height_map, ivec2(ut_1,       vt        ), /*lod=*/0).x;
	const float  v7 = texelFetch(terrain_height_map, ivec2(ut_2,       vt        ), /*lod=*/0).x;
	const float  v8 = texelFetch(terrain_height_map, ivec2(ut_minus_1, vt_1      ), /*lod=*/0).x;
	const float  v9 = texelFetch(terrain_height_map, ivec2(ut,         vt_1      ), /*lod=*/0).x;
	const float v10 = texelFetch(terrain_height_map, ivec2(ut_1,       vt_1      ), /*lod=*/0).x;
	const float v11 = texelFetch(terrain_height_map, ivec2(ut_2,       vt_1      ), /*lod=*/0).x;
	const float v12 = texelFetch(terrain_height_map, ivec2(ut_minus_1, vt_2      ), /*lod=*/0).x;
	const float v13 = texelFetch(terrain_height_map, ivec2(ut,         vt_2      ), /*lod=*/0).x;
	const float v14 = texelFetch(terrain_height_map, ivec2(ut_1,       vt_2      ), /*lod=*/0).x;
	const float v15 = texelFetch(terrain_height_map, ivec2(ut_2,       vt_2      ), /*lod=*/0).x;


	const float w0  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 1)) + square(f_pixels_y - (floor(f_pixels_y) - 1))));
	const float w1  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 0)) + square(f_pixels_y - (floor(f_pixels_y) - 1))));
	const float w2  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 1)) + square(f_pixels_y - (floor(f_pixels_y) - 1))));
	const float w3  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 2)) + square(f_pixels_y - (floor(f_pixels_y) - 1))));
	const float w4  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 1)) + square(f_pixels_y - (floor(f_pixels_y) + 0))));
	const float w5  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 0)) + square(f_pixels_y - (floor(f_pixels_y) + 0))));
	const float w6  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 1)) + square(f_pixels_y - (floor(f_pixels_y) + 0))));
	const float w7  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 2)) + square(f_pixels_y - (floor(f_pixels_y) + 0))));
	const float w8  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 1)) + square(f_pixels_y - (floor(f_pixels_y) + 1))));
	const float w9  = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 0)) + square(f_pixels_y - (floor(f_pixels_y) + 1))));
	const float w10 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 1)) + square(f_pixels_y - (floor(f_pixels_y) + 1))));
	const float w11 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 2)) + square(f_pixels_y - (floor(f_pixels_y) + 1))));
	const float w12 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 1)) + square(f_pixels_y - (floor(f_pixels_y) + 2))));
	const float w13 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) - 0)) + square(f_pixels_y - (floor(f_pixels_y) + 2))));
	const float w14 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 1)) + square(f_pixels_y - (floor(f_pixels_y) + 2))));
	const float w15 = mitchellNetravaliEval(sqrt(square(f_pixels_x - (floor(f_pixels_x) + 2)) + square(f_pixels_y - (floor(f_pixels_y) + 2))));

	float filter_sum = 
		w0 	+
		w1 	+
		w2 	+
		w3 	+
			
		w4 	+
		w5 	+
		w6 	+
		w7 	+
			
		w8 	+
		w9 	+
		w10	+
		w11	+
			
		w12	+
		w13	+
		w14	+
		w15;

	const float sum = 
		(((v0  * w0 +
		   v1  * w1) +
		  (v2  * w2 +
		   v3  * w3)) +

		 ((v4  * w4 +
		   v5  * w5) +
		  (v6  * w6 +
		   v7  * w7))) +

		(((v8  * w8  +
		   v9  * w9 ) +
		  (v10 * w10 +
		   v11 * w11)) +

		 ((v12 * w12 +
		   v13 * w13) +
		  (v14 * w14 +
		   v15 * w15)));

	return sum / filter_sum;
}



layout(local_size_x = /*WORK_GROUP_SIZE*/1, local_size_y = 1, local_size_z = 1) in;


void main()
{
	const uint i = gl_GlobalInvocationID.x;

	PrecomputedPoint point = precomputed_points[i];

	float pos_x = (float(chunk_info.chunk_x_index) + point.u) * chunk_info.chunk_w_m; // world space coordinates in metres.
	float pos_y = (float(chunk_info.chunk_y_index) + point.v) * chunk_info.chunk_w_m;

	// -------------- eval terrain height ----------------
	float nx = pos_x * chunk_info.terrain_scale_factor + 0.5; // Offset by 0.5 so that the central heightmap is centered at (0,0,0).
	float ny = pos_y * chunk_info.terrain_scale_factor + 0.5;

	float heightmap_terrain_z = sampleHeightmapHighQual(nx, ny);

	vec4 mask_val = texture(terrain_mask_tex, vec2(nx + 0.5/1024, ny + 0.5/1024));

	vec2 main_tex_coords = vec2(pos_x * chunk_info.terrain_scale_factor, pos_y * chunk_info.terrain_scale_factor);

	vec2 mask_coords = main_tex_coords + vec2(0.5 + 0.5 / 1024, 0.5 + 0.5 / 1024);
	vec2 detail_map_2_uvs = main_tex_coords * (8.0 * 1024 / 4.0);
	float beach_factor = 0;
	float veg_frac = ((mask_val.z > fbmMix(detail_map_2_uvs * 0.2).x * 0.3 + 0.5 + beach_factor) ? 1.0 : 0.0);

	veg_frac = max(veg_frac, texture(detail_mask_tex, vec2(pos_x / DETAIL_MASK_MAP_WIDTH_M, pos_y / DETAIL_MASK_MAP_WIDTH_M)).x); // Apply detail mask texture

	float terrain_h = heightmap_terrain_z;

	float veg_noise_xy_scale = 1 / 50.f;
	float veg_noise_mag = 0.4f * mask_val.z;
	float veg_fbm_val = (veg_noise_mag > 0) ?
		fbmMix(vec2(pos_x, pos_y) * veg_noise_xy_scale) * veg_noise_mag : 
		0.f;
	terrain_h += veg_fbm_val;

	
	float scale_factor =  precomputed_points[i].scale_factor;
	float scale_variation = chunk_info.base_scale / 3.0;
	float scale = chunk_info.base_scale + scale_factor * scale_variation;

	float imposter_height = scale;
	float imposter_width = imposter_height * chunk_info.imposter_width_over_height;
	float imposter_rot = precomputed_points[i].rot;

	// Apply veg_frac: Hide imposters not on vegetation by setting them to zero width.
	if(veg_frac < 0.5)
		imposter_width = 0;

	const uint base_vert_offset = chunk_info.vert_data_offset_B / SIZEOF_VERTEXSTRUCT;

	// Vertex 0
	vertices[base_vert_offset + i*4 + 0].pos_x = pos_x;
	vertices[base_vert_offset + i*4 + 0].pos_y = pos_y;
	vertices[base_vert_offset + i*4 + 0].pos_z = terrain_h;
	vertices[base_vert_offset + i*4 + 0].imposter_width = imposter_width;
	vertices[base_vert_offset + i*4 + 0].u = 0.0;
	vertices[base_vert_offset + i*4 + 0].v = 0.0;
	vertices[base_vert_offset + i*4 + 0].imposter_rot = imposter_rot;

	// Vertex 1
	vertices[base_vert_offset + i*4 + 1].pos_x = pos_x;
	vertices[base_vert_offset + i*4 + 1].pos_y = pos_y;
	vertices[base_vert_offset + i*4 + 1].pos_z = terrain_h;
	vertices[base_vert_offset + i*4 + 1].imposter_width = imposter_width;
	vertices[base_vert_offset + i*4 + 1].u = 1.0;
	vertices[base_vert_offset + i*4 + 1].v = 0.0;
	vertices[base_vert_offset + i*4 + 1].imposter_rot = imposter_rot;

	// Vertex 2
	vertices[base_vert_offset + i*4 + 2].pos_x = pos_x;
	vertices[base_vert_offset + i*4 + 2].pos_y = pos_y;
	vertices[base_vert_offset + i*4 + 2].pos_z = terrain_h + imposter_height;
	vertices[base_vert_offset + i*4 + 2].imposter_width = imposter_width;
	vertices[base_vert_offset + i*4 + 2].u = 1.0;
	vertices[base_vert_offset + i*4 + 2].v = 1.0;
	vertices[base_vert_offset + i*4 + 2].imposter_rot = imposter_rot;

	// Vertex 3
	vertices[base_vert_offset + i*4 + 3].pos_x = pos_x;
	vertices[base_vert_offset + i*4 + 3].pos_y = pos_y;
	vertices[base_vert_offset + i*4 + 3].pos_z = terrain_h + imposter_height;
	vertices[base_vert_offset + i*4 + 3].imposter_width = imposter_width;
	vertices[base_vert_offset + i*4 + 3].u = 0.0;
	vertices[base_vert_offset + i*4 + 3].v = 1.0;
	vertices[base_vert_offset + i*4 + 3].imposter_rot = imposter_rot;
}
