
// Convolves a probe capture into one octahedral tile - either the irradiance tile or the depth tile, selected by
// convolve_depth.
//
// Irradiance: each output texel is a direction N, integrating cosine-weighted radiance over the whole sphere of
// captured texels.  Result units match the old cosine_env_tex: integral over hemisphere of cosine * incoming
// radiance * 1.0e-9.
//
// Depth: mean and mean squared distance over a narrow cone about N, for the Chebyshev visibility test.  The cone
// is much tighter than the cosine lobe, since visibility needs sharper angular detail than irradiance does.
//
// The whole tile is rendered, border ring included; border texels get the direction of the interior texel they
// wrap around to, so no separate border copy pass is needed.
//
// The per-face basis is passed in from IrradianceProbes::getCaptureFaceBasis() rather than duplicated here, so
// that the mapping used to render the capture and the one used to read it cannot drift apart.

uniform sampler2D capture_tex;
uniform sampler2D capture_depth_tex;
uniform vec2 probe_tile_origin;      // Atlas texel coordinates of the lower left corner of the tile being written.
uniform vec3 capture_face_basis[18]; // 6 faces x (forward, right, up).
uniform int convolve_depth;          // 0 = write the irradiance tile, 1 = write the depth tile.
uniform float capture_near_clip_dist;
uniform float max_probe_distance;    // Distances are clamped to this, so distant geometry can't wreck the variance.

// Cosine power for the depth cone.  Larger keeps the visibility test sharper at tile edges.
const float DEPTH_CONE_SHARPNESS = 50.0;

out vec4 colour_out;


void main()
{
	vec2 tile_texel = gl_FragCoord.xy - probe_tile_origin;

	int interior_res = (convolve_depth != 0) ? PROBE_DEPTH_TILE_INTERIOR_RES : PROBE_TILE_INTERIOR_RES;

	vec2 oct_uv = (tile_texel - vec2(float(PROBE_TILE_BORDER))) * (1.0 / float(interior_res));
	vec3 N = oct_to_float32x3(wrapOctCoord(oct_uv * 2.0 - vec2(1.0)));

	// Face coordinates span [-1, 1], so a texel is this wide on a face at distance 1.
	const float texel_size = 2.0 / float(PROBE_CAPTURE_FACE_RES);

	vec3 irradiance = vec3(0.0);
	float depth_sum = 0.0;
	float depth_sq_sum = 0.0;
	float depth_weight_sum = 0.0;

	for(int face=0; face<6; ++face)
	{
		vec3 face_forward = capture_face_basis[face*3 + 0];
		vec3 face_right   = capture_face_basis[face*3 + 1];
		vec3 face_up      = capture_face_basis[face*3 + 2];

		for(int y=0; y<PROBE_CAPTURE_FACE_RES; ++y)
		for(int x=0; x<PROBE_CAPTURE_FACE_RES; ++x)
		{
			float u = (float(x) + 0.5) * texel_size - 1.0;
			float v = (float(y) + 0.5) * texel_size - 1.0;

			vec3 d = face_forward + face_right * u + face_up * v;
			float d_len2 = dot(d, d); // = 1 + u^2 + v^2

			float cos_theta = dot(N, d) * inversesqrt(d_len2);
			if(cos_theta <= 0.0)
				continue;

			ivec2 capture_texel = ivec2(face * PROBE_CAPTURE_FACE_RES + x, y);

			if(convolve_depth != 0)
			{
				float weight = pow(cos_theta, DEPTH_CONE_SHARPNESS);
				if(weight <= 0.0)
					continue;

				// The depth buffer holds distance along the face's view axis.  d has a unit forward component, so
				// scaling by |d| converts that to distance from the probe.
				float view_axis_dist = getDepthFromDepthTextureValue(capture_near_clip_dist, texelFetch(capture_depth_tex, capture_texel, /*lod=*/0).x);
				float dist = min(view_axis_dist * sqrt(d_len2), max_probe_distance);

				depth_sum        += weight * dist;
				depth_sq_sum     += weight * dist * dist;
				depth_weight_sum += weight;
			}
			else
			{
				// Solid angle of the texel: area * cos(angle to face normal) / r^2, which for a face at distance 1
				// works out as texel_size^2 / (1 + u^2 + v^2)^1.5.  Summed over all 6 faces this gives 4pi.
				float solid_angle = (texel_size * texel_size) / (d_len2 * sqrt(d_len2));

				vec3 radiance = texelFetch(capture_tex, capture_texel, /*lod=*/0).xyz;

				irradiance += radiance * (cos_theta * solid_angle);
			}
		}
	}

	if(convolve_depth != 0)
	{
		float inv_weight = (depth_weight_sum > 0.0) ? (1.0 / depth_weight_sum) : 0.0;
		colour_out = vec4(depth_sum * inv_weight, depth_sq_sum * inv_weight, 0.0, 1.0);
	}
	else
		colour_out = vec4(irradiance, 1.0);
}
