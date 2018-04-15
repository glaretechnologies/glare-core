/*=====================================================================
EnvMapProcessing.cpp
--------------------
Copyright Glare Technologies Limited 2017 -
=====================================================================*/
#include "IncludeOpenGL.h"
#include "EnvMapProcessing.h"


#include "OpenGLEngine.h"
#include "../indigo/TestUtils.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"
#include "../graphics/imformatdecoder.h"
#include "../graphics/Map2D.h"
#include "../graphics/ImageMap.h"
#include "../graphics/EXRDecoder.h"
#include "../maths/GeometrySampling.h"
#include "../indigo/SobolSequence.h"
#include "../indigo/SobolData.h"
#include "../raytracing/MatUtils.h"


namespace EnvMapProcessing
{


#if BUILD_TESTS


static float alpha2ForRoughness(float r)
{
	return Maths::pow6(r);
}

//static float trowbridgeReitzPDF(float cos_theta, float alpha2)
//{
//	return cos_theta * alpha2 / (3.1415926535897932384626433832795 * Maths::square(Maths::square(cos_theta) * (alpha2 - 1.0f) + 1.0f));
//}


// Sample Trowbridge-Reitz distribution, return microfacet normal (half-vector) and pdf.
static Vec4f sampleHalfVectorForAlpha2(const SamplePair& samples, float alpha2, float& pd_out)
{
	//------------------------------------------------------------------------
	//generate microfacet halfvector
	//------------------------------------------------------------------------
	const float phi = samples.x * Maths::get2Pi<float>();
	const float cos_theta = std::sqrt(-(samples.y - 1)/((alpha2 - 1)*samples.y + 1));

	pd_out = MatUtils::trowbridgeReitzPDF(cos_theta, alpha2);

	//------------------------------------------------------------------------
	//form half-vector
	//------------------------------------------------------------------------
	const float sintheta = std::sqrt(1 - cos_theta*cos_theta);

	return Vec4f(std::cos(phi) * sintheta, std::sin(phi) * sintheta, cos_theta, 0.f);
}


void run(const std::string& indigo_base_dir)
{
#if 1
	conPrint("EnvMapProcessing");

	try
	{


		Map2DRef skymap_highres = ImFormatDecoder::decodeImage(".", "D:\\programming\\indigo_output\\vs2015\\indigo_x64\\RelWithDebInfo\\sky_no_sun.exr");
		//Map2DRef skymap_highres = ImFormatDecoder::decodeImage(".", "N:\\new_cyberspace\\trunk\\assets\\sky_no_sun.exr");
		//Map2DRef skymap_highres = ImFormatDecoder::decodeImage(".", "N:\\indigo\\trunk\\testscenes\\uffizi-large.exr");

		


		//ImageMapFloatRef uniform = new ImageMapFloat(128, 64, 3); // For debugging
		//uniform->set(1.0f);
		//skymap = uniform;

		//=============================== Build cosine refl (irradiance) map =====================================
		if(true)
		{
			// Bake a cube map.  

			bool is_linear;
			Map2DRef skymap_lowres = skymap_highres->resizeToImageMapFloat(256, is_linear);

			if(!skymap_lowres.isType<ImageMapFloat>())
			{
				conPrint("Must be ImageMapFloat");
				return;
			}
			ImageMapFloatRef skymapf = skymap_lowres.downcast<ImageMapFloat>();

			// For each face of the cubemap:
			for(int face=0; face<6; ++face)
			{
				Vec4f origin, uvec, vvec;
				if(face == 0) // GL_TEXTURE_CUBE_MAP_POSITIVE_X (right)
				{
					origin = Vec4f(1,  1, 1, 1); // origin = Vec4f(1, 1, -1, 1);
					uvec   = Vec4f(0, 0,  -1, 0);
					vvec   = Vec4f(0,  -1,  0, 0);
				}
				else if(face == 1) // GL_TEXTURE_CUBE_MAP_NEGATIVE_X (left)
				{
					origin = Vec4f(-1, 1, -1, 1); // origin = Vec4f(-1, 1, 1, 1);
					uvec   = Vec4f(0,   0,  1, 0); // seems to co
					vvec   = Vec4f(0,   -1, 0 , 0); // vvec = Vec4f(0, -1, 0, 0);
				}
				else if(face == 2) // GL_TEXTURE_CUBE_MAP_POSITIVE_Y (top)
				{
					origin = Vec4f(-1, 1, -1, 1);
					uvec   = Vec4f( 1, 0,  0, 0);
					vvec   = Vec4f( 0, 0,  1, 0);
				}
				else if(face == 3) // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y (bottom)
				{
					origin = Vec4f(-1, -1, 1, 1);
					uvec   = Vec4f(1,  0,  0, 0);
					vvec   = Vec4f( 0,  0,  -1, 0);
				}
				else if(face == 4) // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
				{
					origin = Vec4f(-1, 1, 1, 1); // origin = Vec4f(1, 1, 1, 1);
					uvec   = Vec4f(1, 0, 0, 0);
					vvec   = Vec4f( 0, -1, 0, 0);
				}
				else if(face == 5) // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
				{
					origin = Vec4f(1, 1, -1, 1); // origin = Vec4f(-1, 1, -1, 1);
					uvec   = Vec4f(-1, 0, 0, 0);
					vvec   = Vec4f(0, -1, 0, 0);
				}

				// Convert origin, uvec, vvec from OpenGL space with y up to Indigo Space with z up
				//origin = Vec4f(origin[0], -origin[2], origin[1], origin[3]);
				//uvec = Vec4f(uvec[0], -uvec[2], uvec[1], uvec[3]);
				//vvec = Vec4f(vvec[0], -vvec[2], vvec[1], vvec[3]);


				const int diff_H = 32;
				const int diff_W = 32;
				ImageMapFloatRef diffuse_map = new ImageMapFloat(diff_W, diff_H, 3);

				for(int y=0; y<diff_H; ++y)
				{
					printVar(y);
					for(int x=0; x<diff_W; ++x)
					{
						const float fx = (float)x / (float)(diff_W);
						const float fy = (float)y / (float)(diff_H);

						const Vec4f dir = normalise(origin - Vec4f(0, 0, 0, 1) + uvec * 2 * fx + vvec * 2 * fy);

						Vec2f phi_theta = GeometrySampling::sphericalCoordsForDir(dir);

						phi_theta.x -= 1.f; // Rotate in the same way as env map is in MainWindow.
						const float dir_u = Maths::fract(-phi_theta.x / Maths::get2Pi<float>());
						const float dir_v = Maths::fract(phi_theta.y / Maths::pi<float>());
						
						//TEMP:
						//int sx = dir_u * skymap_lowresf->getWidth();
						//int sy = dir_v * skymap_lowresf->getHeight();
						//Colour3f sum = Colour3f(
						//	skymap_lowresf->getPixel(sx, sy)[0], 
						//	skymap_lowresf->getPixel(sx, sy)[1], 
						//	skymap_lowresf->getPixel(sx, sy)[2]);

						
						// Compute integral of cosine-weighted pixel values from the sky map.
						Colour3f sum(0.f);
						for(int sy=0; sy<(int)skymapf->getHeight(); ++sy)
						{
							const float fsy = (float)sy / (float)(skymapf->getHeight());

							const float theta = Maths::pi<float>() * fsy;
							const float sin_theta = sin(theta);

							for(int sx=0; sx<(int)skymapf->getWidth(); ++sx)
							{
								const float fsx = (float)sx / (float)(skymapf->getWidth());

								const Vec4f src_dir = GeometrySampling::dirForSphericalCoords(fsx * Maths::get2Pi<float>(), fsy * Maths::pi<float>());

								const float jacobian = 1 / (2*Maths::pi<float>()*Maths::pi<float>() * myMax(0.f, std::sin(theta)));

								const float weight = myMax(dot(dir, src_dir), 0.f) * (1 / jacobian);

								sum += Colour3f(skymapf->getPixel(sx, sy)[0], skymapf->getPixel(sx, sy)[1], skymapf->getPixel(sx, sy)[2]) * weight;
							}
						}

						// Normalise
						sum /= (skymapf->getHeight() * skymapf->getWidth());
						

						//if(face == 0)
						//	sum = Colour3f(0, 1, 0);


						diffuse_map->getPixel(x, y)[0] = sum.r;
						diffuse_map->getPixel(x, y)[1] = sum.g;
						diffuse_map->getPixel(x, y)[2] = sum.b;
					}
				}

				// Save out diffuse map
				EXRDecoder::saveImageToEXR(*diffuse_map, "diffuse_sky_no_sun_" + toString(face) + ".exr", EXRDecoder::SaveOptions());
			}
		}
		//=============================== End build cosine refl map =====================================

		//=============================== Build specular refl map =====================================
		if(true)
		{
			::createGlobalSobolData(indigo_base_dir);
			SobolSequence seq(16);

			const int NUM_SAMPLES = 1 << 12;
			js::Vector<float, 16> qmc_samples(NUM_SAMPLES * 16);
			
			for(int i=0; i<NUM_SAMPLES; ++i)
				seq.evalPoints(&qmc_samples[i*16]);


			ImageMapFloatRef skymapf = skymap_highres.downcast<ImageMapFloat>();

			const int diff_H = 64;
			const int diff_W = diff_H * 2;
			const int NUM_ROUGHNESSES = 8;

			ImageMapFloatRef combined_map = new ImageMapFloat(diff_W, diff_H * NUM_ROUGHNESSES, 3);

			// For roughness zero, just copy over env map without any blurring
			/*for(int y=0; y<diff_H; ++y)
				for(int x=0; x<diff_W; ++x)
				{
					combined_map->getPixel(x, y)[0] = skymapf->getPixel(x, y)[0];
					combined_map->getPixel(x, y)[1] = skymapf->getPixel(x, y)[1];
					combined_map->getPixel(x, y)[2] = skymapf->getPixel(x, y)[2];
				}*/


			for(int z=0; z<NUM_ROUGHNESSES; ++z)
			{
				const float roughness = myMax(0.07f, (float)z / (NUM_ROUGHNESSES - 1));
				const float alpha2 = alpha2ForRoughness(roughness);

				conPrint("roughness: " + toString(roughness));

				//ImageMapFloatRef diffuse_map = new ImageMapFloat(diff_W, diff_H, 3);

				/*if(!skymap.isType<ImageMapFloat>())
				{
					conPrint("Must be ImageMapFloat");
					return;
				}*/

				for(int y=0; y<diff_H; ++y)
				{
					printVar(y);
					for(int x=0; x<diff_W; ++x)
					{
						const float fx = (float)x / (float)(diff_W);
						const float fy = (float)y / (float)(diff_H);

						const Vec4f dir = GeometrySampling::dirForSphericalCoords(fx * Maths::get2Pi<float>(), fy * Maths::pi<float>());

						Matrix4f basis;
						basis.constructFromVector(dir);

						Vec3d sum(0.0);

						for(int i=0; i<NUM_SAMPLES; ++i)
						{
							float pd;
							const Vec4f sampled_dir_ = sampleHalfVectorForAlpha2(Vec2f(qmc_samples[i*16 + 0], qmc_samples[i*16 + 1]), alpha2, pd);
							const Vec4f sampled_dir = basis * sampled_dir_;

							const Vec2f phi_theta = GeometrySampling::sphericalCoordsForDir(sampled_dir);

							const Colour3f sky_col = skymapf->vec3SampleTiled(phi_theta.x / Maths::get2Pi<float>(), -phi_theta.y / Maths::pi<float>());

							sum += toVec3d(sky_col.toVec3());
						}

						sum /= NUM_SAMPLES;

						combined_map->getPixel(x, y + z * diff_H)[0] = sum.x;
						combined_map->getPixel(x, y + z * diff_H)[1] = sum.y;
						combined_map->getPixel(x, y + z * diff_H)[2] = sum.z;

						/*Colour3d sum(0.0);

						// Form integral of cosine-weighted pixel values from the sky map.
						for(int sy=0; sy<skymapf->getHeight(); ++sy)
						{
							const float fsy = (float)sy / (float)(skymapf->getHeight());

							const float theta = Maths::pi<float>() * fsy;
							const float sin_theta = sin(theta);

							for(int sx=0; sx<skymapf->getWidth(); ++sx)
							{
								const float fsx = (float)sx / (float)(skymapf->getWidth());

								const Vec4f src_dir = GeometrySampling::dirForSphericalCoords(fsx * Maths::get2Pi<float>(), fsy * Maths::pi<float>());

								if(dot(dir, src_dir) > 0)
								{
									const Vec4f h = normalise(src_dir + dir);
									const float pdf = trowbridgeReitzPDF(dot(src_dir, h), alpha2);

									const float jacobian = 1 / (2*Maths::pi<float>()*Maths::pi<float>() * myMax(0.f, sin_theta));

									const float weight = dot(dir, src_dir) * pdf * (1 / jacobian);

									sum += Colour3d(skymapf->getPixel(sx, sy)[0], skymapf->getPixel(sx, sy)[1], skymapf->getPixel(sx, sy)[2]) * weight;
								}
							}
						}

						// Normalise
						sum /= (skymapf->getHeight() * skymapf->getWidth());

						combined_map->getPixel(x, y + z * diff_H)[0] = sum.r;
						combined_map->getPixel(x, y + z * diff_H)[1] = sum.g;
						combined_map->getPixel(x, y + z * diff_H)[2] = sum.b;*/
					}
				}

				// Save out diffuse map
				EXRDecoder::saveImageToEXR(*combined_map, "specular_refl_sky_no_sun_combined.exr", EXRDecoder::SaveOptions());
			}
		}
		//=============================== End build specular refl map =====================================
	}
	catch(ImFormatExcep& e)
	{
		conPrint("ERROR: " + e.what());
		return;
	}
	
	conPrint("EnvMapProcessing done.");
#endif
}


#endif // BUILD_TESTS


} // end namespace EnvMapProcessing
