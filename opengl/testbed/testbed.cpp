/*=====================================================================
testbed.cpp
-----------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/

// For testing the OpenGL Engine


#include <opengl/ui/GLUITextView.h>
#include <opengl/ui/GLUI.h>
#include <maths/GeometrySampling.h>
#include <graphics/FormatDecoderGLTF.h>
#include <graphics/MeshSimplification.h>
#include <opengl/OpenGLEngine.h>
#include <opengl/GLMeshBuilding.h>
#include <indigo/TextureServer.h>
#include <opengl/MeshPrimitiveBuilding.h>
#include <utils/Exception.h>
#include <utils/StandardPrintOutput.h>
#include <utils/IncludeWindows.h>
#include <utils/PlatformUtils.h>
#include <utils/UTF8Utils.h>
#include <utils/FileUtils.h>
#include <utils/ConPrint.h>
#include <utils/StringUtils.h>
#include <GL/gl3w.h>
#include <SDL_opengl.h>
#include <SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>
#include <string>
#if EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <ui/GLUILineEdit.h>


#if !defined(EMSCRIPTEN)
#define EM_BOOL bool
#endif

//static EM_BOOL doOneMainLoopIter(double time, void *userData);
static void doOneMainLoopIter();


static void setGLAttribute(SDL_GLattr attr, int value)
{
	const int result = SDL_GL_SetAttribute(attr, value);
	if(result != 0)
	{
		const char* err = SDL_GetError();
		throw glare::Exception("Failed to set OpenGL attribute: " + (err ? std::string(err) : "[Unknown]"));
	}
}


Reference<OpenGLEngine> opengl_engine;
GLObjectRef main_test_ob;
GLUIRef gl_ui;

float cam_phi = 0.0;
float cam_theta = 1.8f;
Vec4f cam_pos(0,-5,2,1);

Timer* timer;
Timer* time_since_last_frame;
Timer* stats_timer;
Timer* diagnostics_timer;
int num_frames = 0;
std::string last_diagnostics;
bool reset = false;
double fps = 0;
bool wireframes = false;

SDL_Window* win;
SDL_GLContext gl_context;

float sun_phi = 1.f;
float sun_theta = Maths::pi<float>() / 4;

StandardPrintOutput print_output;
bool quit = false;


int main(int, char**)
{
	try
	{
		Clock::init();


	//	TextRenderer::test();
	//	return 0;//TEMP


		timer = new Timer();
		time_since_last_frame = new Timer();
		stats_timer = new Timer();
		diagnostics_timer = new Timer();


		//=========================== Init SDL and OpenGL ================================
		if(SDL_Init(SDL_INIT_VIDEO) != 0)
			throw glare::Exception("SDL_Init Error: " + std::string(SDL_GetError()));


		// Set GL attributes, needs to be done before window creation.


#if EMSCRIPTEN
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		setGLAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
		setGLAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); // We need to request a specific version for a core profile.
		setGLAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
		setGLAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		setGLAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
#endif

		int primary_W = 1600;
		int primary_H = 1000;
#if EMSCRIPTEN
		//int width, height;
		//emscripten_get_canvas_element_size("canvas", &width, &height);
		//primary_W = (int)width;
		//primary_H = (int)height;
#endif
		

		win = SDL_CreateWindow("Testbed", 100, 100, primary_W, primary_H, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
		if(win == nullptr)
			throw glare::Exception("SDL_CreateWindow Error: " + std::string(SDL_GetError()));


		gl_context = SDL_GL_CreateContext(win);
		if(!gl_context)
			throw glare::Exception("OpenGL context could not be created! SDL Error: " + std::string(SDL_GetError()));


#if !defined(EMSCRIPTEN)
		gl3wInit();
#endif


		// Initialise ImGUI
		ImGui::CreateContext();

		ImGui_ImplSDL2_InitForOpenGL(win, gl_context);
		ImGui_ImplOpenGL3_Init();


		// Create OpenGL engine
		OpenGLEngineSettings settings;
		settings.compress_textures = true;
		settings.shadow_mapping = true;
		settings.depth_fog = true;
		settings.render_water_caustics = false;
		settings.allow_multi_draw_indirect = false; // TEMP
		settings.allow_bindless_textures = false; // TEMP
		settings.enable_debug_output = true;
#if defined(EMSCRIPTEN)
		settings.use_general_arena_mem_allocator = false;
#endif
		opengl_engine = new OpenGLEngine(settings);

		const std::string base_src_dir(BASE_SOURCE_DIR);

#if defined(EMSCRIPTEN)
		const std::string data_dir = "/data";
#else
		const std::string data_dir = PlatformUtils::getEnvironmentVariable("GLARE_CORE_TRUNK_DIR") + "/opengl";
#endif
		
		// Create main task manager.
		// This is for doing work like texture compression and EXR loading, that will be created by LoadTextureTasks etc.
		// Alloc these on the heap as Emscripten may have issues with stack-allocated objects before the emscripten_set_main_loop() call.
#if defined(EMSCRIPTEN)
		const size_t main_task_manager_num_threads = myClamp<size_t>(PlatformUtils::getNumLogicalProcessors(), 1, 8);
#else
		const size_t main_task_manager_num_threads = myClamp<size_t>(PlatformUtils::getNumLogicalProcessors(), 1, 512);
#endif
		glare::TaskManager* main_task_manager = new glare::TaskManager("main task manager", main_task_manager_num_threads);
		main_task_manager->setThreadPriorities(MyThread::Priority_Lowest);


		// Create high-priority task manager.
		// For short, processor intensive tasks that the main thread depends on, such as computing animation data for the current frame, or executing Jolt physics tasks.
#if defined(EMSCRIPTEN)
		const size_t high_priority_task_manager_num_threads = myClamp<size_t>(PlatformUtils::getNumLogicalProcessors(), 1, 8);
#else
		const size_t high_priority_task_manager_num_threads = myClamp<size_t>(PlatformUtils::getNumLogicalProcessors(), 1, 512);
#endif
		glare::TaskManager* high_priority_task_manager = new glare::TaskManager("high_priority_task_manager", high_priority_task_manager_num_threads);


		//Reference<glare::Allocator> mem_allocator = new glare::GeneralMemAllocator(/*arena_size_B=*/2 * 1024 * 1024 * 1024ull);
		//Reference<glare::Allocator> mem_allocator = new glare::LimitedAllocator(/*max_size_B=*/1536 * 1024 * 1024ull);
		Reference<glare::Allocator> mem_allocator = new glare::MallocAllocator();


		opengl_engine->initialise(data_dir, NULL, &print_output, main_task_manager, high_priority_task_manager, mem_allocator);
		if(!opengl_engine->initSucceeded())
			throw glare::Exception("OpenGL init failed: " + opengl_engine->getInitialisationErrorMsg());
		opengl_engine->setViewportDims(primary_W, primary_H);
		opengl_engine->setMainViewportDims(primary_W, primary_H);

#if defined(EMSCRIPTEN)
		const std::string base_dir = "/data";
#else
		const std::string base_dir = ".";
#endif


		// NOTE: this code is also in MainWindow.cpp
#if defined(_WIN32)
		const std::string font_path       = PlatformUtils::getFontsDirPath() + "/Segoeui.ttf"; // SegoeUI is shipped with Windows 7 onwards: https://learn.microsoft.com/en-us/typography/fonts/windows_7_font_list
		const std::string emoji_font_path = PlatformUtils::getFontsDirPath() + "/Seguiemj.ttf";
#elif defined(__APPLE__)
		const std::string font_path       = "/System/Library/Fonts/SFNS.ttf";
		const std::string emoji_font_path = "/System/Library/Fonts/SFNS.ttf";
#else
		// Linux:
		const std::string font_path       = base_dir + "/data/resources/TruenoLight-E2pg.otf";
		const std::string emoji_font_path = base_dir + "/data/resources/TruenoLight-E2pg.otf"; 
#endif


		TextRendererRef text_renderer = new TextRenderer();
		//TextRendererFontFaceRef font       = new TextRendererFontFace(text_renderer, font_path, 30);
		//TextRendererFontFaceRef emoji_font = new TextRendererFontFace(text_renderer, emoji_font_path, 30);

		Timer timer;
		std::vector<TextRendererFontFaceRef> fonts;
		fonts.push_back(new TextRendererFontFace(text_renderer, font_path, 18));
		fonts.push_back(new TextRendererFontFace(text_renderer, font_path, 24));
		fonts.push_back(new TextRendererFontFace(text_renderer, font_path, 36));
		fonts.push_back(new TextRendererFontFace(text_renderer, font_path, 48));
		fonts.push_back(new TextRendererFontFace(text_renderer, font_path, 60));
		fonts.push_back(new TextRendererFontFace(text_renderer, font_path, 72));

		std::vector<TextRendererFontFaceRef> emoji_fonts;
		emoji_fonts.push_back(new TextRendererFontFace(text_renderer, emoji_font_path, 100));
		conPrint("loading fonts took " + timer.elapsedString());

		gl_ui = new GLUI();
		gl_ui->create(opengl_engine, /*device pixel ratio=*/1.f, fonts, emoji_fonts);
		
		

		// Add some text
		std::vector<GLUITextRef> texts;
		{
			GLUIText::GLUITextCreateArgs text_create_args;

			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, "A B C D", /*botleft=*/Vec2f(-1.0f, -0.1f), text_create_args);
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}

		// Create some Emoji text
		{
			//text->create(*gl_ui, opengl_engine, UTF8Utils::encodeCodePoint(0x2639), /*botleft=*/Vec2f(-1.0, -0.6), Colour3f(1,1,1)); // https://unicodelookup.com/#face/1
			GLUIText::GLUITextCreateArgs text_create_args;

			// https://unicode.org/emoji/charts/full-emoji-list.html#1f600, https://unicode.org/emoji/charts/full-emoji-list.html#1f60e, https://unicode.org/emoji/charts/full-emoji-list.html#1f4af
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, 
				UTF8Utils::encodeCodePoint(0x1F600) + 
				UTF8Utils::encodeCodePoint(0x1F60E) + 
				UTF8Utils::encodeCodePoint(0x1f4af), 
				/*botleft=*/Vec2f(0, -0.2f), text_create_args); 
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}

		{
			//text->create(*gl_ui, opengl_engine, UTF8Utils::encodeCodePoint(0x393), /*botleft=*/Vec2f(-1.0, -0.4), Colour3f(1,1,1)); // "U+1F600"
			GLUIText::GLUITextCreateArgs text_create_args;
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, UTF8Utils::encodeCodePoint(0x3B1) + UTF8Utils::encodeCodePoint(0x3B2) + UTF8Utils::encodeCodePoint(0x3B3), /*botleft=*/Vec2f(-1.0f, -0.4f), text_create_args); // alpha  beta gamma
			//text->create(*gl_ui, opengl_engine, UTF8Utils::encodeCodePoint(0x1F600), /*botleft=*/Vec2f(-1.0, -0.4), Colour3f(1,1,1)); // "U+1F600"
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}
		{
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, "ghA", /*botleft=*/Vec2f(-1.0f, -0.2f), GLUIText::GLUITextCreateArgs());
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}
		{
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, "abcdefghijklmnopqrstuvwxyz", /*botleft=*/Vec2f(-1.0f, 0.0f), GLUIText::GLUITextCreateArgs());
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}
		{
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", /*botleft=*/Vec2f(-1.0f, 0.2f), GLUIText::GLUITextCreateArgs());
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}
		{
			GLUIText::GLUITextCreateArgs args;
			args.font_size_px = 48;
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", /*botleft=*/Vec2f(-1.0f, 0.1f), args);
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}

		{
			GLUITextRef text = new GLUIText(*gl_ui, opengl_engine, "0123456789!@#$%^&*()", /*botleft=*/Vec2f(-1.0f, 0.4f), GLUIText::GLUITextCreateArgs());
			
			//gl_ui->addWidget(text);
			texts.push_back(text);
		}

		// Add text-view widget
		{
			GLUITextViewRef text_view = new GLUITextView();
			GLUITextView::GLUITextViewCreateArgs create_args;
			text_view->create(*gl_ui, opengl_engine, "abc ABC 0123456789 !@#$%^&*()", /*botleft=*/Vec2f(0.0f, 0.0f), create_args);
			
			gl_ui->addWidget(text_view);
		}

		// Add text-view widget
		{
			GLUITextViewRef text_view = new GLUITextView();
			GLUITextView::GLUITextViewCreateArgs create_args;
			create_args.font_size_px = 60;
			text_view->create(*gl_ui, opengl_engine, "abc ABC 0123456789 !@#$%^&*()", /*botleft=*/Vec2f(0.0f, 0.1f), create_args);
			
			gl_ui->addWidget(text_view);
		}

		// Add a line edit box
		{
			GLUILineEditRef line_edit = new GLUILineEdit();
			GLUILineEdit::GLUILineEditCreateArgs create_args;
			create_args.font_size_px = 24;
			line_edit->create(*gl_ui, opengl_engine, /*botleft=*/Vec2f(0.0f, -0.1f), create_args);

			gl_ui->addWidget(line_edit);
		}




		//for(int i=0; i<10000; ++i)
		//{
		//	GLUITextRef text = new GLUIText();
		//	text->create(*gl_ui, opengl_engine, "0123456789!@#$%^&*()", /*botleft=*/Vec2f(-1.0f, 0.4f), Colour3f(1,1,1));
		//}

		if(false)
		{
			std::string text = FileUtils::readEntireFileTextMode("d:/files/neuromancer.txt");

			const std::vector<std::string> lines = ::split(text, '\n');

			PCG32 rng(1);
			Vec2f pos(-1,1);
			for(size_t i=0; i<lines.size(); ++i)
			{
				pos.y -= 0.002f;
				if(pos.y < -1.f)
				{
					pos.x += 0.25f;
					pos.y = 1;
				}
				GLUIText::GLUITextCreateArgs create_args;
				create_args.colour = Colour3f(rng.unitRandom(), rng.unitRandom(),rng.unitRandom());
				GLUITextRef linetext = new GLUIText(*gl_ui, opengl_engine, lines[i], /*botleft=*/pos, create_args);
				texts.push_back(linetext);
			}
		}












		sun_phi = 1.f;
		sun_theta = Maths::pi<float>() / 4;
		opengl_engine->setSunDir(normalise(Vec4f(std::cos(sun_phi) * sin(sun_theta), std::sin(sun_phi) * sin(sun_theta), cos(sun_theta), 0)));
		opengl_engine->setEnvMapTransform(Matrix3f::rotationMatrix(Vec3f(0,0,1), sun_phi));

		//---------------------- Set env material -------------------
		{
			OpenGLMaterial env_mat;
			opengl_engine->setEnvMat(env_mat);
		}
		opengl_engine->setCirrusTexture(opengl_engine->getTexture(base_dir + "/resources/cirrus.exr"));

		//---------------------- Add light -------------------
		{
			GLLightRef light = new GLLight();
			light->gpu_data.pos = Vec4f(0, 8, 4, 1);
			light->gpu_data.dir = normalise(Vec4f(0, 1, -1, 0));
			light->gpu_data.col = Colour4f(0, 3.0e10f, 3.0e10f, 0);

			//opengl_engine->addLight(light);
		}

		//----------------------- Make ground plane -----------------------
		{
			GLObjectRef ground_plane = new GLObject();
			ground_plane->mesh_data = MeshPrimitiveBuilding::makeQuadMesh(*opengl_engine->vert_buf_allocator, Vec4f(1,0,0,0), Vec4f(0,1,0,0), 16);
			const float scale = 1000.f;
			ground_plane->ob_to_world_matrix = Matrix4f::uniformScaleMatrix(scale) * Matrix4f::translationMatrix(-0.5f, -0.5f, 0);
			ground_plane->materials.resize(1);
			ground_plane->materials[0].albedo_texture = opengl_engine->getTexture(base_dir + "/resources/obstacle.png");
			ground_plane->materials[0].tex_matrix = Matrix2f(scale, 0, 0, scale);

			opengl_engine->addObject(ground_plane);
		}

		//--------------------------- Load model -------------------------------
#if defined(EMSCRIPTEN)
		const std::string model_dir = "/data";
#else
		const std::string model_dir = "C:\\Users\\nick\\Downloads";
#endif

		GLTFLoadedData gltf_data;
		Timer timer2;
		//BatchedMeshRef batched_mesh = FormatDecoderGLTF::loadGLTFFile("C:\\Users\\nick\\Downloads\\SciFiHelmet.gltf", gltf_data);
		BatchedMeshRef batched_mesh;// = FormatDecoderGLTF::loadGLTFFile(model_dir + "/DamagedHelmet.gltf", gltf_data);
		//BatchedMeshRef batched_mesh = BatchedMesh::readFromFile("D:\\files\\Saeule_6row_obj_4065354431801582820.bmesh");
		//BatchedMeshRef batched_mesh = BatchedMesh::readFromFile("D:\\models\\room2_WindowsFLAT_glb_13600392068904710101.bmesh");
		//BatchedMeshRef batched_mesh = BatchedMesh::readFromFile("D:\\models\\Station_Cleaned__OMPTIMIZED__CENTRAL_FLIPPED__noBin_glb_13427799269604943069.bmesh");
		conPrint("loadGLTFFile time: " + timer2.elapsedString());

		// Simplify mesh
		//batched_mesh = MeshSimplification::buildSimplifiedMesh(*batched_mesh, /*target reduction ratio=*/10.0f, /*target error=*/1.0f, /*sloppy=*/false);


		OpenGLMeshRenderDataRef test_mesh_data;// = GLMeshBuilding::buildBatchedMesh(opengl_engine->vert_buf_allocator.ptr(), batched_mesh, /*skip opengl calls=*/false, NULL);
		//OpenGLMeshRenderDataRef test_mesh_data = MeshPrimitiveBuilding::makeCubeMesh(*opengl_engine->vert_buf_allocator);
		
		PCG32 rng(1);

		if(false)
		{
			GLObjectRef ob = new GLObject();
			main_test_ob = ob;
			ob->mesh_data = test_mesh_data;
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(1, 0, 3) * Matrix4f::rotationAroundZAxis(Maths::pi_2<float>()) * Matrix4f::rotationAroundXAxis(Maths::pi_2<float>()) *
				Matrix4f::uniformScaleMatrix(1);
			ob->materials.resize(test_mesh_data->num_materials_referenced);

			{
				TextureParams params;
				params.use_sRGB = false;
				params.allow_compression = false;
				//ob->materials[0].normal_map = opengl_engine->getTexture("C:\\Users\\nick\\Downloads\\SciFiHelmet_Normal.png", params);
				ob->materials[0].normal_map = opengl_engine->getTexture(model_dir + "/Default_normal.jpg", params);
			}
			ob->materials[0].albedo_texture = opengl_engine->getTexture(model_dir + "/Default_albedo.jpg");
			
			{
				TextureParams params;
				params.use_sRGB = false;
				ob->materials[0].metallic_roughness_texture = opengl_engine->getTexture(model_dir + "/Default_metalRoughness.jpg", params);
				ob->materials[0].metallic_frac = 1;
				ob->materials[0].roughness = 1;
			}
			
			ob->materials[0].emission_texture = opengl_engine->getTexture(model_dir + "/Default_emissive.jpg");
			ob->materials[0].emission_scale = 3.0e9f;
			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}
#if 0
		// Add imposter
		{
			GLObjectRef ob = new GLObject();
			main_test_ob = ob;
			ob->mesh_data = test_mesh_data;
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(1, 0, 3) * Matrix4f::rotationAroundZAxis(Maths::pi_2<float>()) * Matrix4f::rotationAroundXAxis(Maths::pi_2<float>()) *
				Matrix4f::uniformScaleMatrix(1);
			ob->materials.resize(1);

			ob->materials[0].albedo_texture = opengl_engine->getTexture(model_dir + "/Default_albedo.jpg");
			ob->materials[0].imposter = true;
			ob->materials[0].begin_fade_out_distance = 1000;
			ob->materials[0].end_fade_out_distance = 2000;
			
			
			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}

		// Add water object
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = test_mesh_data;
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(30, 0, 3) * Matrix4f::rotationAroundZAxis(Maths::pi_2<float>()) * Matrix4f::rotationAroundXAxis(Maths::pi_2<float>()) *
				Matrix4f::uniformScaleMatrix(10);
			ob->materials.resize(1);

			ob->materials[0].albedo_texture = opengl_engine->getTexture(model_dir + "/Default_albedo.jpg");
			//ob->materials[0].imposter = true;
			ob->materials[0].begin_fade_out_distance = 1000;
			ob->materials[0].end_fade_out_distance = 2000;

			ob->materials[0].water = true;
			
			
			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}

		// Add overlay object
		{
			OverlayObjectRef overlay_ob = new OverlayObject();
			overlay_ob->mesh_data = opengl_engine->getUnitQuadMeshData();
			//overlay_ob->material.albedo_linear_rgb = button_colour;
			TextureParams tex_params;
			tex_params.allow_compression = false;
			overlay_ob->material.albedo_texture = opengl_engine->getTexture(model_dir + "/Default_albedo.jpg", tex_params);
			overlay_ob->material.tex_matrix = Matrix2f(1,0,0,-1);


			const float y_scale = 1.f;// opengl_engine->getViewPortAspectRatio();

			Vec2f botleft(0.65f, 0.65f);
			Vec2f dims(0.3f);
			const float z = -0.999f;
			overlay_ob->ob_to_world_matrix = Matrix4f::translationMatrix(botleft.x, botleft.y * y_scale, z) * Matrix4f::scaleMatrix(dims.x, dims.y * y_scale, 1);

			opengl_engine->addOverlayObject(overlay_ob);
		}

		// Add terrain object
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = opengl_engine->getCubeMeshData();
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(-30, 0, 3) * Matrix4f::uniformScaleMatrix(10);
			ob->materials.resize(1);

			ob->materials[0].terrain = true;
			
			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}

		// Add transparent object
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = opengl_engine->getCubeMeshData();
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(-30, 0, 15) * Matrix4f::uniformScaleMatrix(10);
			ob->materials.resize(1);

			ob->materials[0].transparent = true;
			ob->materials[0].alpha = 0.5;
			
			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}

		// Add decal object
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = opengl_engine->getCubeMeshData();
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(-40, 0, 0.5) * Matrix4f::uniformScaleMatrix(5);
			ob->materials.resize(1);

			ob->materials[0].decal = true;
			ob->materials[0].albedo_texture = opengl_engine->getTexture(model_dir + "/Default_albedo.jpg");
			
			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}

		// Add participating media object
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = opengl_engine->getSpriteQuadMeshData();
			ob->ob_to_world_matrix = Matrix4f::translationMatrix(-50, 0, 5) * Matrix4f::uniformScaleMatrix(5);
			ob->materials.resize(1);

			ob->materials[0].participating_media = true;
			ob->materials[0].albedo_texture = opengl_engine->getTexture(model_dir + "/Default_albedo.jpg");

			opengl_engine->addObjectAndLoadTexturesImmediately(ob);
		}
#endif


#if 0
		std::vector<GLObjectRef> obs;
		const float cube_w = 0.2f;
		for(int i=0; i<10000; ++i) // 1000 hangs
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = test_mesh_data;
			ob->ob_to_world_matrix = Matrix4f::translationMatrix((float)(i / 64), (float)(i % 64), cube_w/2) * /*Matrix4f::rotationAroundXAxis(Maths::pi_2<float>()) * */
				Matrix4f::uniformScaleMatrix(cube_w) * Matrix4f::translationMatrix(Vec4f(-0.5f));
			ob->materials.resize(test_mesh_data->num_materials_referenced);
			//ob->materials[0].albedo_texture = opengl_engine->getTexture(base_dir + "/resources/obstacle.png");
			//ob->materials[0].tex_matrix = Matrix2f(10.f, 0, 0, 10.f);

			//uint8 r, g, b;
			uint8 col[3] = { (uint8)rng.nextUInt(256), (uint8)rng.nextUInt(256), (uint8)rng.nextUInt(256) };
			ImageMapUInt8Ref map = new ImageMapUInt8(16, 16, 3);
			for(size_t z=0; z<map->numPixels(); ++z)
			{
				map->getData()[z * 3 + 0] = col[0];
				map->getData()[z * 3 + 1] = col[1];
				map->getData()[z * 3 + 2] = (uint8)rng.nextUInt(256);
			}

			ob->materials[0].albedo_texture = opengl_engine->getOrLoadOpenGLTextureForMap2D(OpenGLTextureKey("tex " + toString(i)), *map);

			opengl_engine->addObject(ob);

			obs.push_back(ob);
		}
#endif

		
		conPrint("Starting main loop...");
#if EMSCRIPTEN
		//emscripten_request_animation_frame_loop(doOneMainLoopIter, 0);

		emscripten_set_main_loop(doOneMainLoopIter, /*fps=*/0, /*simulate_infinite_loop=*/true);
#else
		//bool quit = false;
		while(!quit)
		{
			doOneMainLoopIter();
		}

		SDL_Quit();

		test_mesh_data = NULL;
		main_test_ob = NULL;
		opengl_engine = NULL;
#endif

		
		conPrint("main finished...");
		return 0;
	}
	catch(glare::Exception& e)
	{
		stdErrPrint(e.what());
		return 1;
	}
}


static inline Key getKeyForSDLKey(SDL_Keycode sym)
{
	switch(sym)
	{
	case SDLK_ESCAPE: return Key_Escape;
	case SDLK_BACKSPACE: return Key_Backspace;
	case SDLK_DELETE: return Key_Delete;
	case SDLK_SPACE: return Key_Space;
	case SDLK_LEFTBRACKET: return Key_LeftBracket;
	case SDLK_RIGHTBRACKET: return Key_RightBracket;
	case SDLK_PAGEUP: return Key_PageUp;
	case SDLK_PAGEDOWN: return Key_PageDown;
	case SDLK_EQUALS: return Key_Equals;
	case SDLK_PLUS: return Key_Plus;
	case SDLK_MINUS: return Key_Minus;
	case SDLK_LEFT: return Key_Left;
	case SDLK_RIGHT: return Key_Right;
	case SDLK_UP: return Key_Up;
	case SDLK_DOWN: return Key_Down;
	default: break;
	};

	if(sym >= SDLK_a && sym <= SDLK_z)
		return (Key)(Key_A + (sym - SDLK_a));

	if(sym >= SDLK_0 && sym <= SDLK_9)
		return (Key)(Key_0 + (sym - SDLK_0));

	if(sym >= SDLK_F1 && sym <= SDLK_F12)
		return (Key)(Key_F1 + (sym - SDLK_F1));

	return Key_None;
}


static uint32 convertSDLModifiers(SDL_Keymod sdl_keymod)
{
	uint32 modifiers = 0;
	if((sdl_keymod & SDL_Keymod::KMOD_ALT) != 0)
		modifiers |= Modifiers::Alt;
	if((sdl_keymod & SDL_Keymod::KMOD_CTRL) != 0)
		modifiers |= Modifiers::Ctrl;
	if((sdl_keymod & SDL_Keymod::KMOD_SHIFT) != 0)
		modifiers |= Modifiers::Shift;
	return modifiers;
}


static void convertFromSDKKeyEvent(SDL_Event ev, KeyEvent& key_event)
{
	key_event.key = getKeyForSDLKey(ev.key.keysym.sym);
	key_event.native_virtual_key = 0; // TODO
	// TODO: set key_event.text

	if(key_event.key >= Key_A && key_event.key <= Key_Z)
		key_event.text = std::string(1, 'a' + (key_event.key - Key_A));

	key_event.modifiers = convertSDLModifiers((SDL_Keymod)ev.key.keysym.mod);
}


static Vec2f GLCoordsForGLWidgetPos(OpenGLEngine& gl_engine, const Vec2f widget_pos)
{
	const int vp_width  = gl_engine.getViewPortWidth();
	const int vp_height = gl_engine.getViewPortHeight();


	const int device_pixel_ratio = 1; // TEMP main_window->ui->glWidget->devicePixelRatio(); // For retina screens this is 2, meaning the gl viewport width is in physical pixels, of which have twice the density of qt pixel coordinates.
	const int use_vp_width  = vp_width  / device_pixel_ratio;
	const int use_vp_height = vp_height / device_pixel_ratio;


	return Vec2f(
		(widget_pos.x - use_vp_width /2) / (use_vp_width /2),
		-(widget_pos.y - use_vp_height/2) / (use_vp_height/2)
	);
}



static void doOneMainLoopIter(/*double time, void *userData*/)
{
	//conPrint("doOneMainLoopIter");
	//conPrint("doOneMainLoopIter, time: " + ::toString(time));

	//printVar(num_frames);
			//const double cur_time = timer.elapsed();
			//const float cur_time = (float)timer.elapsed();

		if(SDL_GL_MakeCurrent(win, gl_context) != 0)
			conPrint("SDL_GL_MakeCurrent failed.");

		//	for(size_t i=0; i<obs.size(); ++i)
		//	{
		//		if(i % 4 == 0)
		//		{
		//			obs[i]->ob_to_world_matrix = Matrix4f::translationMatrix((float)(i / 64), (float)(i % 64), cube_w/2) * Matrix4f::rotationAroundZAxis(cur_time) * 
		//				Matrix4f::uniformScaleMatrix(cube_w) * Matrix4f::translationMatrix(Vec4f(-0.5f));
		//			opengl_engine->updateObjectTransformData(*obs[i]);
		//		}
		//	}

			// Update some objects in random order
			/*for(int i=0; i<1024; ++i)
			{
				int ob_i = rng.nextUInt((uint32)obs.size());

				obs[ob_i]->ob_to_world_matrix = Matrix4f::translationMatrix((float)(ob_i / 64), (float)(ob_i % 64), cube_w/2) * Matrix4f::rotationAroundZAxis(cur_time) * 
					Matrix4f::uniformScaleMatrix(cube_w) * Matrix4f::translationMatrix(Vec4f(-0.5f));
				opengl_engine->updateObjectTransformData(*obs[ob_i]);
			}*/

			if(stats_timer->elapsed() > 1.0)
			{
				last_diagnostics = opengl_engine->getDiagnostics();
				// Update statistics
				fps = num_frames / stats_timer->elapsed();
				stats_timer->reset();
				num_frames = 0;
			}


			// Rotate ob
			if(main_test_ob.nonNull())
			{
				main_test_ob->ob_to_world_matrix = Matrix4f::translationMatrix(0, 0, 1) * Matrix4f::rotationAroundZAxis(Maths::pi_2<float>() + (float)timer->elapsed() * 0.4f) * Matrix4f::rotationAroundXAxis(Maths::pi_2<float>()) *
					Matrix4f::uniformScaleMatrix(10);
				opengl_engine->updateObjectTransformData(*main_test_ob);
			}


			const Matrix4f z_rot = Matrix4f::rotationAroundZAxis(cam_phi);
			const Matrix4f x_rot = Matrix4f::rotationAroundXAxis(cam_theta - Maths::pi_2<float>());
			const Matrix4f rot = x_rot * z_rot;

			const Matrix4f world_to_camera_space_matrix = rot * Matrix4f::translationMatrix(-cam_pos);

			const float sensor_width = 0.035f;
			const float lens_sensor_dist = 0.025f;//0.03f;
			const float render_aspect_ratio = opengl_engine->getViewPortAspectRatio();


			int gl_w, gl_h;
			SDL_GL_GetDrawableSize(win, &gl_w, &gl_h);
			if(gl_w > 0 && gl_h > 0)
			{
				opengl_engine->setViewportDims(gl_w, gl_h);
				opengl_engine->setMainViewportDims(gl_w, gl_h);
				opengl_engine->setMaxDrawDistance(10000.f);
				opengl_engine->setPerspectiveCameraTransform(world_to_camera_space_matrix, sensor_width, lens_sensor_dist, render_aspect_ratio, /*lens shift up=*/0.f, /*lens shift right=*/0.f);
				opengl_engine->setCurrentTime((float)timer->elapsed());
				opengl_engine->draw();
			}


			ImGuiIO& imgui_io = ImGui::GetIO();
		
#if 1
			{
				// Draw ImGUI GUI controls
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplSDL2_NewFrame();
				ImGui::NewFrame();
		
				//ImGui::ShowDemoWindow();
		
				ImGui::SetNextWindowSize(ImVec2(600, 900));
				ImGui::Begin("Testbed");
		
				bool sundir_changed = false;
				sundir_changed = sundir_changed || ImGui::SliderFloat("sun_theta", &sun_theta, 0.01f, Maths::pi<float>(), "%1.2f", 0);
				sundir_changed = sundir_changed || ImGui::SliderFloat("sun_phi", &sun_phi, 0, Maths::get2Pi<float>(), "%1.2f", 0);
				if(sundir_changed)
				{
					opengl_engine->setSunDir(normalise(Vec4f(std::cos(sun_phi) * sin(sun_theta), std::sin(sun_phi) * sin(sun_theta), cos(sun_theta), 0)));
					opengl_engine->setEnvMapTransform(Matrix3f::rotationMatrix(Vec3f(0,0,1), sun_phi));
				}
		
				ImGui::Checkbox("use_scatter_shader", &opengl_engine->use_scatter_shader);

				if(ImGui::Checkbox("wireframe", &wireframes))
					opengl_engine->setDrawWireFrames(wireframes);
		
		
				ImGui::TextColored(ImVec4(1,1,0,1), "Stats");
				ImGui::TextUnformatted(("FPS: " + doubleToStringNDecimalPlaces(fps, 1)).c_str());
		
				if(ImGui::CollapsingHeader("Diagnostics"))
				{
					if(diagnostics_timer->elapsed() > 1.0)
					{
						last_diagnostics = opengl_engine->getDiagnostics();
						diagnostics_timer->reset();
					}

					ImGui::TextUnformatted(last_diagnostics.c_str());
				}
			
		
				ImGui::End(); 
		
			
				ImGui::Render();
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}
#endif

			// Display
			SDL_GL_SwapWindow(win);

			const float dt = (float)time_since_last_frame->elapsed();
			time_since_last_frame->reset();

			const Vec4f forwards = GeometrySampling::dirForSphericalCoords(-cam_phi + Maths::pi_2<float>(), cam_theta);
			const Vec4f right = normalise(crossProduct(forwards, Vec4f(0,0,1,0)));
			const Vec4f up = crossProduct(right, forwards);

			bool imgui_captures_mouse_ev = false;

			// Handle any events
			SDL_Event e;
			while(SDL_PollEvent(&e))
			{
				if(imgui_io.WantCaptureMouse)
				{
					ImGui_ImplSDL2_ProcessEvent(&e); // Pass event onto ImGUI
					continue;
				}

				if(e.type == SDL_QUIT) // "An SDL_QUIT event is generated when the user clicks on the close button of the last existing window" - https://wiki.libsdl.org/SDL_EventType#Remarks
				{
					quit = true;
				}
				else if(e.type == SDL_WINDOWEVENT) // If user closes the window:
				{
					if(e.window.event == SDL_WINDOWEVENT_CLOSE)
					{
						quit = true;
					}
					else if(e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
					{
						int w, h;
						SDL_GL_GetDrawableSize(win, &w, &h);
						
						opengl_engine->setViewportDims(w, h);
						opengl_engine->setMainViewportDims(w, h);

						gl_ui->viewportResized(w, h);
					}
				}
				else if(e.type == SDL_KEYDOWN)
				{
					if(e.key.keysym.sym == SDLK_r)
						reset = true;

					KeyEvent key_event;
					convertFromSDKKeyEvent(e, key_event);

					gl_ui->handleKeyPressedEvent(key_event);
				}
				else if(e.type == SDL_MOUSEMOTION)
				{
					if(e.motion.state & SDL_BUTTON_LMASK)
					{
						const float move_scale = 0.005f;
						cam_phi += e.motion.xrel * move_scale;
						cam_theta = myClamp<float>(cam_theta + (float)e.motion.yrel * move_scale, 0.01f, Maths::pi<float>() - 0.01f);
					}
				}
				else if(e.type == SDL_MOUSEWHEEL)
				{
					const float move_speed = 3.f;
					cam_pos += forwards * (float)e.wheel.y * move_speed;
				}
				else if(e.type == SDL_MOUSEBUTTONDOWN)
				{
					if(!imgui_captures_mouse_ev)
					{
						// conPrint("SDL_MOUSEBUTTONDOWN, pos: " + Vec2i(e.button.x, e.button.y).toString() + ", clicks: " + toString(e.button.clicks));

						//SDL_GetMouseState(&mouse_move_origin.x, &mouse_move_origin.y);

						MouseEvent mouse_event;
						mouse_event.cursor_pos = Vec2i(e.button.x, e.button.y);
						mouse_event.gl_coords = GLCoordsForGLWidgetPos(*opengl_engine, Vec2f((float)e.button.x, (float)e.button.y));
						//mouse_event.button = convertSDLMouseButton(e.button.button);
						//mouse_event.modifiers = convertSDLModifiers(SDL_GetModState());

						gl_ui->handleMouseClick(mouse_event.gl_coords);

						if(e.button.clicks == 1) // Single click:
						{
							//SDL_SetRelativeMouseMode(SDL_TRUE);
						}
						else if(e.button.clicks == 2) // Double click:
						{
							//gui_client->doObjectSelectionTraceForMouseEvent(mouse_event);
						}
					}
				}
				else if(e.type == SDL_MOUSEBUTTONUP)
				{
					if(!imgui_captures_mouse_ev)
					{
						// conPrint("SDL_MOUSEBUTTONUP, pos: " + Vec2i(e.button.x, e.button.y).toString() + ", clicks: " + toString(e.button.clicks));

						if(e.button.clicks == 1) // Single click:
						{
							SDL_SetRelativeMouseMode(SDL_FALSE);

							MouseEvent mouse_event;
							mouse_event.cursor_pos = Vec2i(e.button.x, e.button.y);
							mouse_event.gl_coords = GLCoordsForGLWidgetPos(*opengl_engine, Vec2f((float)e.button.x, (float)e.button.y));
							//mouse_event.button = convertSDLMouseButton(e.button.button);
							///mouse_event.modifiers = convertSDLModifiers(SDL_GetModState());
							//gui_client->mouseClicked(mouse_event);
						}
					}
				}
			}

			SDL_PumpEvents();
			const uint8* keystate = SDL_GetKeyboardState(NULL);
			const float shift_factor = (keystate[SDL_SCANCODE_LSHIFT] != 0) ? 3.f : 1.f;
			if(keystate[SDL_SCANCODE_LEFT])
				cam_phi -= dt * 0.25f * shift_factor;
			if(keystate[SDL_SCANCODE_RIGHT])
				cam_phi += dt * 0.25f * shift_factor;

			const float move_speed = 14.f * shift_factor;
			if(keystate[SDL_SCANCODE_W])
				cam_pos += forwards * dt * move_speed;
			if(keystate[SDL_SCANCODE_S])
				cam_pos -= forwards * dt * move_speed;
			if(keystate[SDL_SCANCODE_A])
				cam_pos -= right * dt * move_speed;
			if(keystate[SDL_SCANCODE_D])
				cam_pos += right * dt * move_speed;

			num_frames++;
}
