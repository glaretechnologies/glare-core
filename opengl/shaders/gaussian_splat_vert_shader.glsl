
// gaussian_splat_vert_shader.glsl
// Copyright Glare Technologies Limited 2026 -
//
// Projects each Gaussian splat to a screen-space ellipse (EWA splatting), drawn as one instanced quad.
// See opengl/GaussianSplatRenderer.h for the overall design.

precision highp float; // Override the engine's default "precision mediump float;" for Emscripten - the covariance maths is precision-sensitive.

in vec3 position_in; // Local quad corner, in [-1, 1] x [-1, 1], z = 0.  Scaled and oriented per splat below.
in uint splat_index_in; // Per-instance index into albedo_texture, reordered each frame by the depth sort.

uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 proj_matrix;

uniform sampler2D albedo_texture; // Packed splat data, 4 RGBA32F texels per splat - see packSplatTexels() in GaussianSplatRenderer.cpp.
uniform vec2 viewport_dims_px;
uniform vec2 focal_len_px;
uniform int splat_tex_width;

out vec2 frag_screen_offset_px; // Pixel-space offset of this vertex from the splat's projected centre.
out vec3 frag_conic; // Inverse 2D covariance (A, B, C) of [[A, B], [B, C]], for the per-pixel Gaussian evaluation.
out vec4 frag_colour; // (r, g, b, opacity)


ivec2 splatTexelCoord(int texel_index)
{
	return ivec2(texel_index % splat_tex_width, texel_index / splat_tex_width);
}


void main()
{
	int base_texel = int(splat_index_in) * 4;

	vec4 t0 = texelFetch(albedo_texture, splatTexelCoord(base_texel + 0), 0);
	vec4 t1 = texelFetch(albedo_texture, splatTexelCoord(base_texel + 1), 0);
	vec4 t2 = texelFetch(albedo_texture, splatTexelCoord(base_texel + 2), 0);
	vec4 t3 = texelFetch(albedo_texture, splatTexelCoord(base_texel + 3), 0);

	vec3 pos_os = t0.xyz;
	vec3 scale  = vec3(t0.w, t1.x, t1.y);
	vec4 rot    = vec4(t1.z, t1.w, t2.x, t2.y); // (x, y, z, w)
	frag_colour = vec4(t2.z, t2.w, t3.x, t3.y); // (r, g, b, opacity)

	vec4 pos_vs = view_matrix * (model_matrix * vec4(pos_os, 1.0));

	// Note that the view_matrix the engine sends is in the standard OpenGL camera-space convention (x = right, y = up,
	// -z = forwards), not the engine's own raw (y = forwards, z = up) convention: OpenGLEngine builds it as
	// indigo_to_opengl_cam_matrix * world_to_camera_space_matrix before it reaches any shader.  So depth is -pos_vs.z,
	// and the Jacobian below is the textbook z-forward perspective derivative, with no engine-specific adaptation.
	float depth = -pos_vs.z;
	const float near_epsilon = 0.1; // Splats closer than this blow the 1/d and 1/d^2 terms in the Jacobian up to numerically extreme values, so cull them rather than relying on the radius clamp alone.
	if(depth < near_epsilon)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // Push outside the clip volume.
		frag_conic = vec3(0.0);
		frag_screen_offset_px = vec2(0.0);
		return;
	}

	// Build the rotation matrix from the quaternion.  Columns are built explicitly, rather than with a 9-scalar
	// mat3(...) literal, to avoid GLSL's column-major constructor order silently transposing this.
	float qx = rot.x, qy = rot.y, qz = rot.z, qw = rot.w;
	vec3 r_col0 = vec3(1.0 - 2.0*(qy*qy + qz*qz),       2.0*(qx*qy + qz*qw),       2.0*(qx*qz - qy*qw));
	vec3 r_col1 = vec3(      2.0*(qx*qy - qz*qw), 1.0 - 2.0*(qx*qx + qz*qz),       2.0*(qy*qz + qx*qw));
	vec3 r_col2 = vec3(      2.0*(qx*qz + qy*qw),       2.0*(qy*qz - qx*qw), 1.0 - 2.0*(qx*qx + qy*qy));
	mat3 R = mat3(r_col0, r_col1, r_col2);

	// 3D covariance in object space: Sigma = R * diag(scale^2) * R^T.
	mat3 RS = mat3(r_col0 * (scale.x*scale.x), r_col1 * (scale.y*scale.y), r_col2 * (scale.z*scale.z));
	mat3 cov_os = RS * transpose(R);

	// Transform to camera space.  This assumes model_matrix has no non-uniform scale or shear; non-uniform scaling would
	// need the inverse transpose here instead.
	mat3 W = mat3(view_matrix * model_matrix);
	mat3 cov_vs = W * cov_os * transpose(W);

	// Project the 3D covariance to a 2D screen-space covariance via the projection's Jacobian, evaluated at this splat's
	// view-space position.  Standard z-forward perspective projection: screen = focal * (x, y) / depth.
	float vx = pos_vs.x, vy = pos_vs.y;
	vec3 j_row0 = vec3(focal_len_px.x / depth, 0.0, focal_len_px.x * vx / (depth*depth));
	vec3 j_row1 = vec3(0.0, focal_len_px.y / depth, focal_len_px.y * vy / (depth*depth));

	vec3 cov_vs_j0 = cov_vs * j_row0;
	vec3 cov_vs_j1 = cov_vs * j_row1;

	float cov2d_a = dot(j_row0, cov_vs_j0) + 0.3; // The +0.3 is a low-pass filter on the diagonal, which avoids degenerate sub-pixel splats aliasing.  Standard 3DGS technique.
	float cov2d_b = dot(j_row0, cov_vs_j1);
	float cov2d_c = dot(j_row1, cov_vs_j1) + 0.3;

	float det = cov2d_a * cov2d_c - cov2d_b * cov2d_b;
	if(det <= 0.0)
	{
		gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // Degenerate covariance, which shouldn't normally happen after the dilation above.
		frag_conic = vec3(0.0);
		frag_screen_offset_px = vec2(0.0);
		return;
	}

	// Eigen-decomposition of the symmetric 2x2 [[a, b], [b, c]], giving the ellipse's screen-space axes and radii.
	float mid = 0.5 * (cov2d_a + cov2d_c);
	float half_span = sqrt(max(mid*mid - det, 0.0));
	float lambda1 = mid + half_span;
	float lambda2 = max(mid - half_span, 0.0);

	vec2 axis1 = (cov2d_b != 0.0) ? normalize(vec2(cov2d_b, lambda1 - cov2d_a)) : ((cov2d_a >= cov2d_c) ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
	vec2 axis2 = vec2(-axis1.y, axis1.x);

	// Clamp the screen-space radius.  Splats near the camera plane can have a legitimately huge but numerically extreme
	// projected size, which without a cap turns a single nearby splat into a screen-covering quad.  Twice the viewport's
	// larger dimension is generous enough never to visibly clip a real splat while still bounding the worst case.
	float max_radius_px = 2.0 * max(viewport_dims_px.x, viewport_dims_px.y);
	float radius1 = min(3.0 * sqrt(lambda1), max_radius_px); // 3 sigma, i.e. a 99.7% cutoff.
	float radius2 = min(3.0 * sqrt(lambda2), max_radius_px);

	vec2 screen_offset_px = position_in.x * radius1 * axis1 + position_in.y * radius2 * axis2;

	vec4 clip_pos = proj_matrix * pos_vs;
	clip_pos.xy += (screen_offset_px / viewport_dims_px) * 2.0 * clip_pos.w; // Offset in clip space, premultiplied by w so it survives the perspective divide unchanged.
	gl_Position = clip_pos;

	// Build the conic from the possibly-clamped radii above, rather than by inverting the raw 2D covariance.  Without
	// this, an oversized splat (e.g. a huge near-flat background splat, common where a region has no parallax to
	// constrain its scale) has its quad clamped above but its alpha falloff still computed from the true, enormous
	// variance, which barely decays by the clamped quad edge and so produces a hard straight-edged cutoff instead of a
	// soft fade.  Using the clamped radius as the effective sigma makes alpha fade to ~0 at the quad boundary, and is a
	// no-op whenever the radius wasn't clamped.
	float eff_lambda1 = (radius1 * radius1) * (1.0 / 9.0); // radius = 3*sqrt(lambda), so lambda = (radius/3)^2.
	float eff_lambda2 = (radius2 * radius2) * (1.0 / 9.0);
	float inv_l1 = 1.0 / eff_lambda1;
	float inv_l2 = 1.0 / eff_lambda2;
	frag_conic = vec3(
		axis1.x*axis1.x*inv_l1 + axis2.x*axis2.x*inv_l2,
		axis1.x*axis1.y*inv_l1 + axis2.x*axis2.y*inv_l2,
		axis1.y*axis1.y*inv_l1 + axis2.y*axis2.y*inv_l2); // (A, B, C) of the conic Ax^2 + 2Bxy + Cy^2.
	frag_screen_offset_px = screen_offset_px;
}
