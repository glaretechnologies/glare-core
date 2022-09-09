typedef struct vec2
{
	float2 v;
} vec2;


typedef struct vec4
{
	float4 v;
} vec4;


typedef struct HitInfo
{
	vec2 sub_elem_coords;
	int sub_elem_index;
} HitInfo;


typedef struct FullHitInfo
{
	vec4 geometric_normal;
	vec4 shading_normal;
	vec4 pre_bump_shading_normal;
	vec4 hitpos;
	vec4 dp_du;
	vec4 dp_dv;
	void* hitobject;
	void* hit_material;
	void* external_medium;
	vec2 uv_0;
	HitInfo hitinfo;
	void* world;
	float hitpos_error;
} FullHitInfo;



float evalDisplaceMatParam_bump_3_FullHitInfo_(const  FullHitInfo* const hitinfo)
{
	vec2 uv = hitinfo->uv_0;
		
	printf("uv: (%f, %f)    \n", uv.v.x, uv.v.y);
		
	float let_result_29 = sin(uv.v.y * 20.f) * 0.02f;
	return let_result_29;
}

vec2 vec2_vector_float__2__(float2 v) { vec2 s_; s_.v = v; return s_; }

vec2 vec2_float__float_(const float x, const float y)
{
	return vec2_vector_float__2__((float2)(x, y));
}

vec2 op_add_vec2__vec2_(const  vec2* const a, const  vec2* const b)
{
	return vec2_vector_float__2__((a->v + b->v));
}



FullHitInfo FullHitInfo_vec4__vec4__vec4__vec4__vec4__vec4__opaque__opaque__opaque__vec2__HitInfo__opaque__float__bool__bool__bool__constant_opaque__global_opaque__global_opaque__float_(
	const vec4* const  geometric_normal, const vec4* const  shading_normal, const vec4* const  pre_bump_shading_normal, const vec4* const  hitpos, const vec4* const  dp_du, const vec4* const  dp_dv, 
	void* hitobject, void* hit_material, void* external_medium, const vec2* const  uv_0,
	const HitInfo* const  hitinfo,
	void* world, float hitpos_error
	) 
{ FullHitInfo s_; s_.geometric_normal = *geometric_normal; s_.shading_normal = *shading_normal; s_.pre_bump_shading_normal = *pre_bump_shading_normal; s_.hitpos = *hitpos; s_.dp_du = *dp_du; s_.dp_dv = *dp_dv; s_.hitobject = hitobject; s_.hit_material = hit_material; s_.external_medium = external_medium; s_.uv_0 = *uv_0;
s_.hitinfo = *hitinfo;
s_.world = world; s_.hitpos_error = hitpos_error; 
return s_; }


vec2 computeShadingNormal_3_FullHitInfo__vec4_(const  FullHitInfo* const hitinfo)
{
	vec2 let_result_31;
	float f_uv = evalDisplaceMatParam_bump_3_FullHitInfo_(hitinfo);

	
	//float f_u_du_v = f_uv;
	// args for op_add(vec2, vec2):
	vec2 a_arg_39 = hitinfo->uv_0; // (0.f, 0.f)
	vec2 b_arg_40 = vec2_float__float_(0.0005f, 0.f); // (0.0005f, 0.f)
	// args for FullHitInfo(vec4, vec4, vec4, vec4, vec4, vec4, opaque, opaque, opaque, vec2, HitInfo, opaque, float, bool, bool, bool, constant opaque, global opaque, global opaque, float):
	vec4 geometric_normal_arg_32 = hitinfo->geometric_normal;
	vec4 shading_normal_arg_33 = hitinfo->shading_normal;
	vec4 pre_bump_shading_normal_arg_34 = hitinfo->pre_bump_shading_normal;
	vec4 hitpos_arg_35 = hitinfo->hitpos;
	vec4 dp_du_arg_36 = hitinfo->dp_du;
	vec4 dp_dv_arg_37 = hitinfo->dp_dv;
	vec2 uv_0_arg_38 = op_add_vec2__vec2_(&a_arg_39, &b_arg_40); // (0.0005f, 0.f)
	HitInfo hitinfo_arg_41 = hitinfo->hitinfo;
	FullHitInfo temphitinfo1 = FullHitInfo_vec4__vec4__vec4__vec4__vec4__vec4__opaque__opaque__opaque__vec2__HitInfo__opaque__float__bool__bool__bool__constant_opaque__global_opaque__global_opaque__float_(
		&geometric_normal_arg_32, &shading_normal_arg_33, &pre_bump_shading_normal_arg_34, &hitpos_arg_35, &dp_du_arg_36, &dp_dv_arg_37, hitinfo->hitobject, hitinfo->hit_material, 
		hitinfo->external_medium, &uv_0_arg_38,
		&hitinfo_arg_41,
		hitinfo->world, hitinfo->hitpos_error
		);
	float f_u_du_v = evalDisplaceMatParam_bump_3_FullHitInfo_(&temphitinfo1);
	
	// args for op_add(vec2, vec2):
	vec2 a_arg_49 = hitinfo->uv_0; // (0.f, 0.f)
	vec2 b_arg_50 = vec2_float__float_(0.f, 0.0005f);// (0.f, 0.0005f)
	// args for FullHitInfo(vec4, vec4, vec4, vec4, vec4, vec4, opaque, opaque, opaque, vec2, HitInfo, opaque, float, bool, bool, bool, constant opaque, global opaque, global opaque, float):
	vec4 geometric_normal_arg_42 = hitinfo->geometric_normal;
	vec4 shading_normal_arg_43 = hitinfo->shading_normal;
	vec4 pre_bump_shading_normal_arg_44 = hitinfo->pre_bump_shading_normal;
	vec4 hitpos_arg_45 = hitinfo->hitpos;
	vec4 dp_du_arg_46 = hitinfo->dp_du;
	vec4 dp_dv_arg_47 = hitinfo->dp_dv;
	vec2 uv_0_arg_48 = op_add_vec2__vec2_(&a_arg_49, &b_arg_50); // (0.f, 0.0005f)
	HitInfo hitinfo_arg_51 = hitinfo->hitinfo;
	FullHitInfo temphitinfo2 = FullHitInfo_vec4__vec4__vec4__vec4__vec4__vec4__opaque__opaque__opaque__vec2__HitInfo__opaque__float__bool__bool__bool__constant_opaque__global_opaque__global_opaque__float_(
		&geometric_normal_arg_42, &shading_normal_arg_43, &pre_bump_shading_normal_arg_44, &hitpos_arg_45, &dp_du_arg_46, &dp_dv_arg_47, hitinfo->hitobject, hitinfo->hit_material, hitinfo->external_medium, 
		&uv_0_arg_48,
		&hitinfo_arg_51,
		hitinfo->world, hitinfo->hitpos_error
		);
	float f_u_v_dv = evalDisplaceMatParam_bump_3_FullHitInfo_(&temphitinfo2);
			
			
	let_result_31 = vec2_float__float_(f_u_du_v, f_u_v_dv);

	return let_result_31;
}



__kernel void testKernel(
	__global const float * const restrict				input,
	__global float * const restrict						results_out
	)
{
	FullHitInfo hitinfo;
	hitinfo.geometric_normal.v = (float4)(0,0,1,0);
	hitinfo.shading_normal.v = (float4)(0,0,1,0);
	hitinfo.pre_bump_shading_normal.v = (float4)(0,0,1,0);
	hitinfo.hitpos.v = (float4)(0,0,0,1);
	hitinfo.dp_du.v = (float4)(1,0,0,0);
	hitinfo.dp_dv.v = (float4)(0,1,0,0);
	hitinfo.hitobject = 0;
	hitinfo.hit_material = 0;
	hitinfo.external_medium = 0;
	hitinfo.uv_0.v = (float2)(0.f, 0.f);
	hitinfo.hitinfo.sub_elem_coords.v = (float2)(0,0);
	hitinfo.hitinfo.sub_elem_index = 0;
	hitinfo.world = 0;
	hitinfo.hitpos_error = 0.00001f;

	vec2 res = computeShadingNormal_3_FullHitInfo__vec4_(&hitinfo);

	results_out[0] = res.v.s0;
	results_out[1] = res.v.s1;
	results_out[2] = 0;
	results_out[3] = 0;
}
 