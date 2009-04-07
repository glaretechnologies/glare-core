/*=====================================================================
csmodelloader.cpp
-----------------
File created by ClassTemplate on Sat Nov 13 06:32:42 2004Code By Nicholas Chapman.
=====================================================================*/
#include "csmodelloader.h"


#include "../graphics/modelformatdecoder.h"
//#include "../graphics/formatdecoder3ds.h"
#include "../graphics/Lib3dsFormatDecoder.h"
#include "../graphics/formatdecoderobj.h"
//#include "../graphics/formatdecodermd2.h"
#include "../graphics/FormatDecoderPLY.h"
//#include "../cyberspace/model.h"
#include <fstream>
#include "../utils/fileutils.h"
#include "raymesh.h"
//#include "material.h"
//#include "object.h"
//#include "../indigo/simplematerial.h"
#include "../indigo/object.h"
#include "../physics/jscol_triangle.h"
#include "../graphics/image.h"
#include "../graphics/texture.h"
#include "../graphics/imformatdecoder.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"
#include "../indigo/IndigoMeshDecoder.h"
/*#include "../indigo/AnisoPhong.h"
#include "../indigo/Phong.h"
#include "../indigo/MatOverride.h"
#include "../indigo/Specular.h"
#include "../indigo/Diffuse.h"
#include "../indigo/TextureServer.h"
#include "../indigo/IntermediateMesh.h"
*/

CSModelLoader::CSModelLoader()
{
	decoder = std::auto_ptr<ModelFormatDecoder>(new ModelFormatDecoder());

	//FormatDecoder3ds* decoder_3ds = new FormatDecoder3ds();
	//decoder_3ds->setScale(1.0f);

	//------------------------------------------------------------------------
	//register decoders
	//------------------------------------------------------------------------
	Lib3dsFormatDecoder* decoder_3ds = new Lib3dsFormatDecoder();
	decoder->registerModelDecoder(decoder_3ds);

	decoder->registerModelDecoder(new FormatDecoderPLY());

	decoder->registerModelDecoder(new FormatDecoderObj());

	decoder->registerModelDecoder(new IndigoMeshDecoder());
	//decoder->registerModelDecoder(new FormatDecoderMd2());
}


CSModelLoader::~CSModelLoader()
{	
}

void CSModelLoader::streamModel(const std::string& pathname, ModelLoadingStreamHandler& handler, float scale)// throw (ModelFormatDecoderExcep)
{
	//------------------------------------------------------------------------
	//read model file from disk
	//------------------------------------------------------------------------
	/*std::ifstream modelfile(pathname.c_str(), std::ios::binary);

	if(!modelfile)
		throw CSModelLoaderExcep("could not open file '" + pathname + "' for reading.");

	std::vector<unsigned char> modeldata;
	FileUtils::readEntireFile(modelfile, modeldata);
	
	if(modeldata.empty())
		throw CSModelLoaderExcep("failed to read contents of '" + pathname + "'.");*/

	//------------------------------------------------------------------------
	//stream/decode model
	//------------------------------------------------------------------------
	try
	{
		//decoder->buildModel(&(*modeldata.begin()), modeldata.size(), pathname, model);
		decoder->streamModel(/*&(*modeldata.begin()), modeldata.size(), */pathname, handler, scale);
	}
	catch(ModelFormatDecoderExcep& e)
	{
		throw CSModelLoaderExcep("ModelFormatDecoderExcep for model '" + pathname + "': " + e.what());
	}
	catch(ModelLoadingStreamHandlerExcep& e)
	{
		throw CSModelLoaderExcep("ModelLoadingStreamHandlerExcep for model '" + pathname + "': " + e.what());
	}
}




#if 0
void CSModelLoader::loadModel(const std::string& pathname, 
							  const std::string& workpath/*texturepath*/, 
							  const Matrix3& rotation, const Vec3& offset, float scalefactor, 
							  std::map<std::string, MatOverride>& mat_overrrides,
							  std::vector<Object*>& obs_out,
							  bool enable_normal_smoothing)
{
	//------------------------------------------------------------------------
	//read model file from disk
	//------------------------------------------------------------------------
	std::ifstream modelfile(pathname.c_str(), std::ios::binary);

	if(!modelfile)
		throw CSModelLoaderExcep("could not open file '" + pathname + "' for reading.");

	std::vector<unsigned char> modeldata;
	FileUtils::readEntireFile(modelfile, modeldata);
	
	if(modeldata.empty())
		throw CSModelLoaderExcep("failed to read contents of '" + pathname + "'.");

	//------------------------------------------------------------------------
	//decode model
	//------------------------------------------------------------------------
	CS::Model model;
	try
	{
		decoder->buildModel(&(*modeldata.begin()), modeldata.size(), pathname, model);
	}
	catch(ModelFormatDecoderExcep& e)
	{
		throw CSModelLoaderExcep("ModelFormatDecoderExcep for model '" + pathname + "': " + e.what());
	}

	Object* object = new(SSE::alignedSSEMalloc(sizeof(Object))) Object();
	RayMesh* raymesh = new RayMesh(enable_normal_smoothing);
	object->setGeometry(raymesh);

	//object->materials = std::vector<Material*>(model.model_parts.size(), NULL);

	//TODO: fix this
	if(model.model_parts.empty())
		throw CSModelLoaderExcep("Model '" + pathname + "' had 0 parts.");

	//------------------------------------------------------------------------
	//build raytracer model
	//------------------------------------------------------------------------
	for(unsigned int i=0; i<model.model_parts.size(); ++i)
	{
		if(model.model_parts[i].getTris().empty())
			continue;

		CS::Material& srcmat = model.model_parts[i].material;

		//------------------------------------------------------------------------
		//add vertices to the simple3d mesh
		//------------------------------------------------------------------------
		const int vertoffset = raymesh->mesh.getNumVerts();

		for(unsigned int v=0; v<model.model_parts[i].getVerts().size(); ++v)
		{
			const Vec3 vertpos = (rotation * model.model_parts[i].getVerts()[v].pos) * scalefactor + offset;
			const Vec3 normal = rotation * model.model_parts[i].getVerts()[v].normal;

			raymesh->mesh.createVert(Vertex(vertpos, 
										normal,
										model.model_parts[i].getVerts()[v].texcoords));
		}

		//------------------------------------------------------------------------
		//Add triangles
		//------------------------------------------------------------------------
		for(unsigned int t=0; t<model.model_parts[i].getTris().size(); ++t)
		{
			CS::Triangle& tri = model.model_parts[i].getTris()[t];

			//const Vec3 pos0 = model.model_parts[i].getVerts()[tri.v[0]].pos;
			//const Vec3 trans_pos0 = rotation * pos0;

			const Vec3 v0pos = (rotation * model.model_parts[i].getVerts()[tri.v[0]].pos) * scalefactor + offset;
			const Vec3 v1pos = (rotation * model.model_parts[i].getVerts()[tri.v[1]].pos) * scalefactor + offset;
			const Vec3 v2pos = (rotation * model.model_parts[i].getVerts()[tri.v[2]].pos) * scalefactor + offset;

			js::Triangle jstri(v0pos, v1pos, v2pos);

			//------------------------------------------------------------------------
			//filter out degerenate tris
			//------------------------------------------------------------------------
			const float area2 = ::crossProduct(jstri.v1() - jstri.v0(), jstri.v2() - jstri.v0()).length2();

			const float MIN_AREA = 0.0f;//TEMP0.000000001f;
			if(area2 > MIN_AREA)
			{
				//------------------------------------------------------------------------
				//add tri to ray tracing mesh
				//------------------------------------------------------------------------
				raymesh->insertTri(jstri);

				object->tri_materials.push_back(i);//set this tri to use the ith material.
		
				//------------------------------------------------------------------------
				//add tri to simple3d mesh
				//------------------------------------------------------------------------
				raymesh->mesh.createTri(tri.v[0] + vertoffset, 
									tri.v[1] + vertoffset,
									tri.v[2] + vertoffset);

				//raymesh->mesh.getTriList().back().smoothing_group = tri.smoothing_group;

//TEMP NOT SETTING VERT NORMALS
//				raymesh->mesh.getTriList().back().vert_normals[0] = normalise(rotation * tri.vertnormals[0]);
//				raymesh->mesh.getTriList().back().vert_normals[1] = normalise(rotation * tri.vertnormals[1]);
//				raymesh->mesh.getTriList().back().vert_normals[2] = normalise(rotation * tri.vertnormals[2]);
			}
		}

		//------------------------------------------------------------------------
		//build the material
		//------------------------------------------------------------------------
		const float MAX_DIFFUSE = 0.8f;
		Colour3 diffuse = srcmat.diffuse_colour * MAX_DIFFUSE;//needs to be less than 1 for energy conservation
		if(srcmat.textured)
		{		
			diffuse = Colour3(1.0f, 1.0f, 1.0f) * MAX_DIFFUSE;
		}

		const Colour3 emissive = srcmat.emissive_colour;//.rgb.x, srcmat.emissive_colour.rgb.y, srcmat.emissive_colour.rgb.z);

		float transmit_frac = 0.0f;
		if(srcmat.opacity < 1.0f)
			transmit_frac = 1.0f;

		const float max_spec_frac = 0.3333f * (srcmat.specular_colour.r + srcmat.specular_colour.g + srcmat.specular_colour.b);


		Material* newmat = NULL;

		if(mat_overrrides.find(srcmat.getStdStringName()) != mat_overrrides.end())
		{
			conPrint("---mat: '" + srcmat.getStdStringName() + "'---");
			conPrint("Using overridden material.");
			newmat = mat_overrrides[srcmat.getStdStringName()].mat;
			assert(newmat);

			/// Load specular texture if defined
			if(srcmat.textured && mat_overrrides[srcmat.getStdStringName()].use_diffuse_tex_as_spec)
			{				
				//const std::string modeldir = FileUtils::getDirectory(pathname);
				Texture* tex = loadTexture(std::string(srcmat.getCStrTexFileName()), workpath);//modeldir, texturepath);
				if(tex)
				{
					TextureUnit texunit;
					texunit.texture = tex;
					texunit.uvset = 2;
					if(dynamic_cast<Phong*>(newmat))
						dynamic_cast<Phong*>(newmat)->setSpecularTexture(texunit);
				}
			}
		}
		else
		{
			if(transmit_frac > 0.0f)
			{
				newmat = new Specular(/*0.04f, */true, 1.5f, 0.0f, Colour3(0.0f), 5);
			}
			else
			{
				newmat = new Diffuse(diffuse);
			}

			conPrint("---mat: '" + srcmat.getStdStringName() + "'---");
			conPrint("diffuse: " + ::toString(diffuse.r) + ", " + ::toString(diffuse.g) + ", " + ::toString(diffuse.b));
			conPrint("transmit_frac: " + ::toString(transmit_frac));
			if(srcmat.textured)
			{
				conPrint("textured.");
				conPrint("texname: '" + std::string(srcmat.getTexFileName().c_str()) + "'");
			}
			else
				conPrint("untextured.");
		}

		/// Load diffuse texture if defined
		if(srcmat.textured)
		{				
			//const std::string modeldir = FileUtils::getDirectory(pathname);
			Texture* tex = loadTexture(std::string(srcmat.getCStrTexFileName()), workpath);//modeldir, texturepath);
			if(tex)
			{
				TextureUnit texunit;
				texunit.texture = tex;
				texunit.uvset = 0;
				if(dynamic_cast<Diffuse*>(newmat))
					dynamic_cast<Diffuse*>(newmat)->setDiffuseTexture(texunit);
				else if(dynamic_cast<Phong*>(newmat))
					dynamic_cast<Phong*>(newmat)->setDiffuseTexture(texunit);
			}
		}

		object->setMaterial(i, newmat);

	}//end for each model part

	//------------------------------------------------------------------------
	//build the ray tracing mesh (build kd-tree)
	//------------------------------------------------------------------------
	raymesh->build();

	//------------------------------------------------------------------------
	//build simple3d mesh
	//------------------------------------------------------------------------
	raymesh->mesh.buildTriNormals();

	//mesh->mesh.buildAdjacencyInfo();
	//mesh->mesh.calcNormals();
	//if(!model.model_parts.empty() && model.model_parts[0].autogen_normals)
	//TEMP NO BUILD	raymesh->mesh.build();

	obs_out.push_back(object);
}





IntermediateMesh* CSModelLoader::buildIntermediateMesh(const std::string& pathname, NameMap<Material*>& materials)
{
	IntermediateMesh* newmesh = new IntermediateMesh();

	//------------------------------------------------------------------------
	//read model file from disk
	//------------------------------------------------------------------------
	std::ifstream modelfile(pathname.c_str(), std::ios::binary);

	if(!modelfile)
		throw CSModelLoaderExcep("could not open file '" + pathname + "' for reading.");

	std::vector<unsigned char> modeldata;
	FileUtils::readEntireFile(modelfile, modeldata);
	
	if(modeldata.empty())
		throw CSModelLoaderExcep("failed to read contents of '" + pathname + "'.");

	//------------------------------------------------------------------------
	//decode model
	//------------------------------------------------------------------------
	CS::Model model;
	try
	{
		decoder->buildModel(&(*modeldata.begin()), modeldata.size(), pathname, model);
	}
	catch(ModelFormatDecoderExcep& e)
	{
		throw CSModelLoaderExcep("ModelFormatDecoderExcep for model '" + pathname + "': " + e.what());
	}

	//TODO: fix this
	if(model.model_parts.empty())
		throw CSModelLoaderExcep("Model '" + pathname + "' had 0 parts.");

	//------------------------------------------------------------------------
	//build raytracer model
	//------------------------------------------------------------------------
	for(unsigned int i=0; i<model.model_parts.size(); ++i)
	{
		if(model.model_parts[i].getTris().empty())
			continue;

		//------------------------------------------------------------------------
		//add vertices to the intermediate mesh
		//------------------------------------------------------------------------
		const int vertoffset = (int)newmesh->verts.size();

		for(unsigned int v=0; v<model.model_parts[i].getVerts().size(); ++v)
		{
			IntermediateVert vert;
			vert.normal = model.model_parts[i].getVerts()[v].normal;
			vert.pos = model.model_parts[i].getVerts()[v].pos;
			vert.uv[0] = model.model_parts[i].getVerts()[v].texcoords;

			newmesh->verts.push_back(vert);
		}

		//------------------------------------------------------------------------
		//Add triangles
		//------------------------------------------------------------------------
		for(unsigned int t=0; t<model.model_parts[i].getTris().size(); ++t)
		{
			CS::Triangle& tri = model.model_parts[i].getTris()[t];

			IntermediateTri newtri;
			newtri.v[0] = tri.v[0] + vertoffset;
			newtri.v[1] = tri.v[1] + vertoffset;
			newtri.v[2] = tri.v[2] + vertoffset;
			newtri.material_index = i;//set this tri to use the ith material.

			newmesh->tris.push_back(newtri);
		}

		//------------------------------------------------------------------------
		//add material reference
		//------------------------------------------------------------------------
		CS::Material& srcmat = model.model_parts[i].material;
		
		newmesh->matname_to_index_map[srcmat.getStdStringName()] = i;

		const std::string texworkpath = FileUtils::getDirectory(pathname);

		parseMaterial(srcmat, texworkpath, materials);
	}
	
	//set up a default uv set export: albedo uses channel 0
	newmesh->uvset_name_to_index["albedo"] = 0;

	return newmesh;
}


void CSModelLoader::parseMaterial(const CS::Material& srcmat, const std::string& texworkpath, NameMap<Material*>& materials)
{
	/*if(materials.isInserted(srcmat.getStdStringName()))
	{
		conPrint("Material '" + srcmat.getStdStringName() + "' encountered in 3ds file is already defined, so using override.");
	}
	else
	{*/
		//------------------------------------------------------------------------
		//build the material
		//------------------------------------------------------------------------
		Material* newmat;

		if(srcmat.opacity < 1.0f)
		{
			//transparent
			newmat = new Specular(/*0.04f, */true, 1.5f, 0.0f, Colour3(0.0f), 5);
		}
		else
		{
			//make it a diffuse mat

			Colour3 diffusecol = srcmat.diffuse_colour;
			if(srcmat.textured)
				diffusecol = Colour3(1.0f);

			Diffuse* diffuse = new Diffuse(diffusecol);
			newmat = diffuse;

			/// Load diffuse texture if defined
			if(srcmat.textured)
			{				
				Texture* tex = loadTexture(std::string(srcmat.getCStrTexFileName()), texworkpath);
				if(tex)
				{
					//tex->raiseToPower(1.5f);

					TextureUnit texunit;
					texunit.texture = tex;
					texunit.uvset = 0;
					texunit.texturepath = srcmat.getCStrTexFileName();
					texunit.uvset_name = "albedo";
					diffuse->setDiffuseTexture(texunit);
				}
			}
		}

		conPrint("Adding material '" + srcmat.getStdStringName() + "' to scene materials.");
		materials.insert(srcmat.getStdStringName(), newmat);
	//}
}





Texture* CSModelLoader::loadTexture(const std::string& texname,
									const std::string& texworkpath)
									//const std::string& modelpath, const std::string& texturepath)
{

	try
	{
		const std::string fullpath = FileUtils::join(texworkpath, texname);

		return TextureServer::getInstance().getTexForPath(fullpath);
	}
	catch(TextureServerExcep& )
	{
		return NULL;
	}

	/*bool loadedtex = false;

	///Try to load the texture out of the workpath directory first
	std::string texpath = FileUtils::join(workpath, texname);

	std::vector<unsigned char> texdata;
	try
	{
		conPrint("trying to load texture from '" + texpath + "'...");
		FileUtils::readEntireFile(texpath, texdata);
		loadedtex = true;
	}
	catch(FileUtils::FileUtilsExcep&)
	{
		conPrint("couldn't open '" + texpath + "' for reading.");
	}

	///Try to load the texture out of the texture directory, but with lower cased name
	if(!loadedtex)
	{
		texpath = FileUtils::join(workpath, ::toLowerCase(texname));

		try
		{
			FileUtils::readEntireFile(texpath, texdata);
			loadedtex = true;
		}
		catch(FileUtils::FileUtilsExcep&)
		{
			conPrint("couldn't open '" + texpath + "' for reading.");
		}
	}
*/
	/*if(!loadedtex)
	{
		/// If the tex wasn't in the tex dir, try to get it out of the dir the model 
		/// is in.
		texpath = modelpath + texname;

		try
		{
			conPrint("trying to load texture from '" + texpath + "'...");
			FileUtils::readEntireFile(texpath, texdata);
			loadedtex = true;
		}
		catch(FileUtils::FileUtilsExcep&)
		{
			conPrint("couldn't open '" + texpath + "' for reading.");
		}
	}

	if(!loadedtex)
	{
		texpath = modelpath + ::toLowerCase(texname);

		try
		{
			FileUtils::readEntireFile(texpath, texdata);
			loadedtex = true;
		}
		catch(FileUtils::FileUtilsExcep&)
		{	
			conPrint("couldn't open '" + texpath + "' for reading.");
		}
	}*//*

	//if failed to load from any of the paths...
	if(!loadedtex)
	{
		return NULL;
		//throw CSModelLoaderExcep("Error loading texture file '" + texname + "'");
	}

	//------------------------------------------------------------------------
	//decode the texture file
	//------------------------------------------------------------------------
	try
	{
		Texture* tex = new Texture();

		ImFormatDecoder::decodeImage(texdata, texpath, *tex);	

		conPrint("Loaded texture successfully.");

		return tex;
	}
	catch(ImFormatExcep& e)
	{
		//TEMP NO DELETE ME LEAK: delete mat->texture;
		//mat->texture = NULL;
		conPrint("Warning: failed to decode texture '" + texpath + "'.");

		throw CSModelLoaderExcep("ImFormatExcep: " + e.what());

		//return NULL;
	}*/
}

#endif







