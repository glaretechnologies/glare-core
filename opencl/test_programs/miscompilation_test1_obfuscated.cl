typedef struct vec2
{
	float2 v;
} vec2;


typedef struct vec4
{
	float4 v;
} vec4;


typedef struct ident_DC31FF
{
	vec2 ident_279FA7;
	int ident_8AEA5A;
} ident_DC31FF;


typedef struct AAA
{
	vec4 ident_4214F3;
	vec4 ident_B12FA9;
	vec4 ident_8EDC4F;
	vec4 ident_FE3DFC;
	vec4 ident_EFF581;
	vec4 ident_4BD72D;
	void* ident_535978;
	void* ident_E37539;
	void* ident_E6C932;
	vec2 ident_F3DF3A;
	ident_DC31FF ident_87004F;
	void* ident_3F3E82;
	float ident_25B9B7;
} AAA;



float ident_354A1B(const  AAA* const ident_87004F)
{
	vec2 ident_EE161B = ident_87004F->ident_F3DF3A;
		
	printf("Values: (%f, %f)    \n", ident_EE161B.v.x, ident_EE161B.v.y);
		
	float ident_766DAE = sin(ident_EE161B.v.y * 20.f) * 0.02f;
	return ident_766DAE;
}

vec2 ident_3D7260(float2 v) { vec2 ident_A6C23E; ident_A6C23E.v = v; return ident_A6C23E; }

vec2 ident_674A13(const float x, const float y)
{
	return ident_3D7260((float2)(x, y));
}

vec2 ident_6B5A0C(const  vec2* const ident_50FF6B, const  vec2* const ident_548D6F)
{
	return ident_3D7260((ident_50FF6B->v + ident_548D6F->v));
}



AAA ident_D5B4EB(
	const vec4* const  ident_4214F3, const vec4* const  ident_B12FA9, const vec4* const  ident_8EDC4F, const vec4* const  ident_FE3DFC, const vec4* const  ident_EFF581, const vec4* const  ident_4BD72D, 
	void* ident_535978, void* ident_E37539, void* ident_E6C932, const vec2* const  ident_F3DF3A,
	const ident_DC31FF* const  ident_87004F,
	void* ident_3F3E82, float ident_25B9B7
	) 
{ AAA ident_A6C23E; ident_A6C23E.ident_4214F3 = *ident_4214F3; ident_A6C23E.ident_B12FA9 = *ident_B12FA9; ident_A6C23E.ident_8EDC4F = *ident_8EDC4F; ident_A6C23E.ident_FE3DFC = *ident_FE3DFC; ident_A6C23E.ident_EFF581 = *ident_EFF581; ident_A6C23E.ident_4BD72D = *ident_4BD72D; ident_A6C23E.ident_535978 = ident_535978; ident_A6C23E.ident_E37539 = ident_E37539; ident_A6C23E.ident_E6C932 = ident_E6C932; ident_A6C23E.ident_F3DF3A = *ident_F3DF3A;
ident_A6C23E.ident_87004F = *ident_87004F;
ident_A6C23E.ident_3F3E82 = ident_3F3E82; ident_A6C23E.ident_25B9B7 = ident_25B9B7; 
return ident_A6C23E; }


vec2 ident_D47058(const  AAA* const ident_87004F)
{
	vec2 ident_C283CF;
	float ident_B8CE0D = ident_354A1B(ident_87004F);

	
	vec2 ident_F5A740 = ident_87004F->ident_F3DF3A; 	
	vec2 ident_22B84E = ident_674A13(0.0005f, 0.f); 		
	vec4 ident_FF96C2 = ident_87004F->ident_4214F3;
	vec4 ident_AB91AB = ident_87004F->ident_B12FA9;
	vec4 ident_729F03 = ident_87004F->ident_8EDC4F;
	vec4 ident_2E3174 = ident_87004F->ident_FE3DFC;
	vec4 ident_3EC65C = ident_87004F->ident_EFF581;
	vec4 ident_B8A9EF = ident_87004F->ident_4BD72D;
	vec2 ident_480C74 = ident_6B5A0C(&ident_F5A740, &ident_22B84E); 	
	ident_DC31FF ident_D978F8 = ident_87004F->ident_87004F;
	AAA ident_EEA60A = ident_D5B4EB(
		&ident_FF96C2, &ident_AB91AB, &ident_729F03, &ident_2E3174, &ident_3EC65C, &ident_B8A9EF, ident_87004F->ident_535978, ident_87004F->ident_E37539, 
		ident_87004F->ident_E6C932, &ident_480C74,
		&ident_D978F8,
		ident_87004F->ident_3F3E82, ident_87004F->ident_25B9B7
		);
	float ident_793515 = ident_354A1B(&ident_EEA60A);
	
	vec2 ident_D574C0 = ident_87004F->ident_F3DF3A; 	
	vec2 ident_3606E0 = ident_674A13(0.f, 0.0005f);		
	vec4 ident_7FEA93 = ident_87004F->ident_4214F3;
	vec4 ident_3CDC1D = ident_87004F->ident_B12FA9;
	vec4 ident_DF99F6 = ident_87004F->ident_8EDC4F;
	vec4 ident_3EE3DD = ident_87004F->ident_FE3DFC;
	vec4 ident_AB8974 = ident_87004F->ident_EFF581;
	vec4 ident_AA9665 = ident_87004F->ident_4BD72D;
	vec2 ident_C5CEFB = ident_6B5A0C(&ident_D574C0, &ident_3606E0); 	
	ident_DC31FF ident_25F3A1 = ident_87004F->ident_87004F;
	AAA ident_399C99 = ident_D5B4EB(
		&ident_7FEA93, &ident_3CDC1D, &ident_DF99F6, &ident_3EE3DD, &ident_AB8974, &ident_AA9665, ident_87004F->ident_535978, ident_87004F->ident_E37539, ident_87004F->ident_E6C932, 
		&ident_C5CEFB,
		&ident_25F3A1,
		ident_87004F->ident_3F3E82, ident_87004F->ident_25B9B7
		);
	float ident_65AAEE = ident_354A1B(&ident_399C99);
			
			
	ident_C283CF = ident_674A13(ident_793515, ident_65AAEE);

	return ident_C283CF;
}



__kernel void testKernel(
	__global float * const restrict						ident_5906C3
	)
{
	AAA ident_87004F;
	ident_87004F.ident_4214F3.v = (float4)(0,0,1,0);
	ident_87004F.ident_B12FA9.v = (float4)(0,0,1,0);
	ident_87004F.ident_8EDC4F.v = (float4)(0,0,1,0);
	ident_87004F.ident_FE3DFC.v = (float4)(0,0,0,1);
	ident_87004F.ident_EFF581.v = (float4)(1,0,0,0);
	ident_87004F.ident_4BD72D.v = (float4)(0,1,0,0);
	ident_87004F.ident_535978 = 0;
	ident_87004F.ident_E37539 = 0;
	ident_87004F.ident_E6C932 = 0;
	ident_87004F.ident_F3DF3A.v = (float2)(0.f, 0.f);
	ident_87004F.ident_87004F.ident_279FA7.v = (float2)(0,0);
	ident_87004F.ident_87004F.ident_8AEA5A = 0;
	ident_87004F.ident_3F3E82 = 0;
	ident_87004F.ident_25B9B7 = 0.00001f;

	vec2 ident_BD4512 = ident_D47058(&ident_87004F);

	ident_5906C3[0] = ident_BD4512.v.s0;
	ident_5906C3[1] = ident_BD4512.v.s1;
	ident_5906C3[2] = 0;
	ident_5906C3[3] = 0;
}
