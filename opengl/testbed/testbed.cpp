/*=====================================================================
testbed.cpp
-----------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/

// For testing the OpenGL Engine


#include <maths/GeometrySampling.h>
#include <graphics/FormatDecoderGLTF.h>
#include <opengl/OpenGLEngine.h>
#include <opengl/GLMeshBuilding.h>
#include <indigo/TextureServer.h>
#include <opengl/MeshPrimitiveBuilding.h>
#include <utils/Exception.h>
#include <utils/StandardPrintOutput.h>
#include <utils/IncludeWindows.h>
#include <utils/PlatformUtils.h>
#include <utils/FileUtils.h>
#include <utils/ConPrint.h>
#include <utils/StringUtils.h>
#include <GL/gl3w.h>
#include <SDL_opengl.h>
#include <SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl.h>
#include <iostream>
#include <string>



void setGLAttribute(SDL_GLattr attr, int value)
{
	const int result = SDL_GL_SetAttribute(attr, value);
	if(result != 0)
	{
		const char* err = SDL_GetError();
		throw glare::Exception("Failed to set OpenGL attribute: " + (err ? std::string(err) : "[Unknown]"));
	}
}


int main(int, char**)
{
	Clock::init();

	try
	{
		//=========================== Init SDL and OpenGL ================================
		int primary_W = 1280;
		int primary_H = 720;
		uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

		conPrint("Using primary window resolution   " + toString(primary_W) + " x " + toString(primary_H));

		SDL_Window* win = SDL_CreateWindow("TerrainGen", 100, 100, 1920, 1080, window_flags | SDL_WINDOW_RESIZABLE);
		if(win == nullptr)
			throw glare::Exception("SDL_CreateWindow Error: " + std::string(SDL_GetError()));

		//setGLAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		//setGLAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		setGLAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		setGLAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

		SDL_GLContext gl_context = SDL_GL_CreateContext(win);
		if(!gl_context)
			throw glare::Exception("OpenGL context could not be created! SDL Error: " + std::string(SDL_GetError()));

		if(SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1) != 0)
			throw glare::Exception("SDL_GL_SetAttribute Error: " + std::string(SDL_GetError()));


		gl3wInit();




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
		Reference<OpenGLEngine> opengl_engine = new OpenGLEngine(settings);

		TextureServer* texture_server = new TextureServer(/*use_canonical_path_keys=*/false);

		const std::string base_src_dir(BASE_SOURCE_DIR);

		const std::string data_dir = PlatformUtils::getEnvironmentVariable("GLARE_CORE_TRUNK_DIR") + "/opengl";
		StandardPrintOutput print_output;
		opengl_engine->initialise(data_dir, texture_server, &print_output);
		if(!opengl_engine->initSucceeded())
			throw glare::Exception("OpenGL init failed: " + opengl_engine->getInitialisationErrorMsg());
		opengl_engine->setViewport(primary_W, primary_H);
		opengl_engine->setMainViewport(primary_W, primary_H);

		const std::string base_dir = ".";

		std::cout << "Finished compiling and linking program." << std::endl;


		float sun_phi = 1.f;
		float sun_theta = Maths::pi<float>() / 4;
		opengl_engine->setSunDir(normalise(Vec4f(std::cos(sun_phi) * sin(sun_theta), std::sin(sun_phi) * sin(sun_theta), cos(sun_theta), 0)));
		opengl_engine->setEnvMapTransform(Matrix3f::rotationMatrix(Vec3f(0,0,1), sun_phi));

		/*
		Set env material
		*/
		{
			OpenGLMaterial env_mat;
			env_mat.albedo_texture = opengl_engine->getTexture(base_dir + "/resources/sky_no_sun.exr");
			env_mat.albedo_texture->setTWrappingEnabled(false); // Disable wrapping in vertical direction to avoid grey dot straight up.
			
			env_mat.tex_matrix = Matrix2f(-1 / Maths::get2Pi<float>(), 0, 0, 1 / Maths::pi<float>());

			opengl_engine->setEnvMat(env_mat);
		}

		opengl_engine->setCirrusTexture(opengl_engine->getTexture(base_dir + "/resources/cirrus.exr"));


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
		//GLTFLoadedData gltf_data;
		//BatchedMeshRef batched_mesh = FormatDecoderGLTF::loadGLBFile("D:\\models\\peugot_closed.glb", gltf_data);
		//OpenGLMeshRenderDataRef test_mesh_data = GLMeshBuilding::buildBatchedMesh(opengl_engine->vert_buf_allocator.ptr(), batched_mesh, /*skip opengl calls=*/false, NULL);
		OpenGLMeshRenderDataRef test_mesh_data = MeshPrimitiveBuilding::makeCubeMesh(*opengl_engine->vert_buf_allocator);
		
		std::vector<GLObjectRef> obs;

		const float cube_w = 0.2f;
		for(int i=0; i<64*64; ++i)
		{
			GLObjectRef ob = new GLObject();
			ob->mesh_data = test_mesh_data;
			ob->ob_to_world_matrix = Matrix4f::translationMatrix((float)(i / 64), (float)(i % 64), cube_w/2) * /*Matrix4f::rotationAroundXAxis(Maths::pi_2<float>()) * */
				Matrix4f::uniformScaleMatrix(cube_w) * Matrix4f::translationMatrix(Vec4f(-0.5f));
			ob->materials.resize(test_mesh_data->num_materials_referenced);
			//ob->materials[0].albedo_texture = opengl_engine->getTexture(base_dir + "/resources/obstacle.png");
			//ob->materials[0].tex_matrix = Matrix2f(10.f, 0, 0, 10.f);

			opengl_engine->addObject(ob);

			obs.push_back(ob);
		}


		Timer timer;

		float cam_phi = 0.0;
		float cam_theta = 1.f;
		Vec4f cam_pos(0,-10,5,1);

		Timer time_since_last_frame;
		Timer stats_timer;
		int num_frames = 0;
		std::string last_diagnostics;
		bool reset = false;
		double fps = 0;

		PCG32 rng(1);

		bool quit = false;
		while(!quit)
		{
			//const double cur_time = timer.elapsed();
			const float cur_time = (float)timer.elapsed();

			//TEMP:
			if(SDL_GL_MakeCurrent(win, gl_context) != 0)
			{
				std::cout << "SDL_GL_MakeCurrent failed." << std::endl;
			}

			/*for(size_t i=0; i<obs.size(); ++i)
			{
				if(i % 8 == 0)
				{
					obs[i]->ob_to_world_matrix = Matrix4f::translationMatrix((float)(i / 64), (float)(i % 64), cube_w/2) * Matrix4f::rotationAroundZAxis(cur_time) * 
						Matrix4f::uniformScaleMatrix(cube_w) * Matrix4f::translationMatrix(Vec4f(-0.5f));
					opengl_engine->updateObjectTransformData(*obs[i]);
				}
			}*/
			for(int i=0; i<1024; ++i)
			{
				int ob_i = rng.nextUInt((uint32)obs.size());

				obs[ob_i]->ob_to_world_matrix = Matrix4f::translationMatrix((float)(ob_i / 64), (float)(ob_i % 64), cube_w/2) * Matrix4f::rotationAroundZAxis(cur_time) * 
					Matrix4f::uniformScaleMatrix(cube_w) * Matrix4f::translationMatrix(Vec4f(-0.5f));
				opengl_engine->updateObjectTransformData(*obs[ob_i]);
			}

			if(stats_timer.elapsed() > 1.0)
			{
				last_diagnostics = opengl_engine->getDiagnostics();
				// Update statistics
				fps = num_frames / stats_timer.elapsed();
				stats_timer.reset();
				num_frames = 0;

			}


			const Matrix4f z_rot = Matrix4f::rotationAroundZAxis(cam_phi);
			const Matrix4f x_rot = Matrix4f::rotationAroundXAxis(-(cam_theta - Maths::pi_2<float>()));
			const Matrix4f rot = x_rot * z_rot;

			const Matrix4f world_to_camera_space_matrix = rot * Matrix4f::translationMatrix(-cam_pos);

			const float sensor_width = 0.035f;
			const float lens_sensor_dist = 0.025f;//0.03f;
			const float render_aspect_ratio = opengl_engine->getViewPortAspectRatio();


			int gl_w, gl_h;
			SDL_GL_GetDrawableSize(win, &gl_w, &gl_h);

			opengl_engine->setViewport(gl_w, gl_h);
			opengl_engine->setMainViewport(gl_w, gl_h);
			opengl_engine->setMaxDrawDistance(1000000.f);
			opengl_engine->setPerspectiveCameraTransform(world_to_camera_space_matrix, sensor_width, lens_sensor_dist, render_aspect_ratio, /*lens shift up=*/0.f, /*lens shift right=*/0.f);
			opengl_engine->setCurrentTime((float)timer.elapsed());
			opengl_engine->draw();


			ImGuiIO& imgui_io = ImGui::GetIO();

			// Draw ImGUI GUI controls
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame(win);
			ImGui::NewFrame();

			//ImGui::ShowDemoWindow();

			ImGui::SetNextWindowSize(ImVec2(600, 900));
			ImGui::Begin("Testbed");

			//ImGui::TextColored(ImVec4(1,1,0,1), "Simulation parameters");

			bool sundir_changed = false;
			sundir_changed = sundir_changed || ImGui::SliderFloat("sun_theta", &sun_theta, 0.01f, Maths::pi<float>(), "%1.2f", 1.f);
			sundir_changed = sundir_changed || ImGui::SliderFloat("sun_phi", &sun_phi, 0, Maths::get2Pi<float>(), "%1.2f", 1.f);

			if(sundir_changed)
			{
				opengl_engine->setSunDir(normalise(Vec4f(std::cos(sun_phi) * sin(sun_theta), std::sin(sun_phi) * sin(sun_theta), cos(sun_theta), 0)));
				opengl_engine->setEnvMapTransform(Matrix3f::rotationMatrix(Vec3f(0,0,1), sun_phi));
			}

			ImGui::Checkbox("use_scatter_shader", &opengl_engine->use_scatter_shader);


			ImGui::TextColored(ImVec4(1,1,0,1), "Stats");
			ImGui::Text(("FPS: " + doubleToStringNDecimalPlaces(fps, 1)).c_str());

			if(ImGui::CollapsingHeader("Diagnostics"))
			{
				ImGui::Text(last_diagnostics.c_str());
			}
			

			ImGui::End(); 

			
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			// Display
			SDL_GL_SwapWindow(win);

			const float dt = (float)time_since_last_frame.elapsed();
			time_since_last_frame.reset();

			const Vec4f forwards = GeometrySampling::dirForSphericalCoords(-cam_phi + Maths::pi_2<float>(), Maths::pi<float>() - cam_theta);
			const Vec4f right = normalise(crossProduct(forwards, Vec4f(0,0,1,0)));
			const Vec4f up = crossProduct(right, forwards);

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
					quit = true;
				else if(e.type == SDL_WINDOWEVENT) // If user closes the window:
				{
					if(e.window.event == SDL_WINDOWEVENT_CLOSE)
						quit = true;
					else if(e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
					{
						int w, h;
						SDL_GL_GetDrawableSize(win, &w, &h);
						
						opengl_engine->setViewport(w, h);
						opengl_engine->setMainViewport(w, h);
					}
				}
				else if(e.type == SDL_KEYDOWN)
				{
					if(e.key.keysym.sym == SDLK_r)
						reset = true;
				}
				else if(e.type == SDL_MOUSEMOTION)
				{
					if(e.motion.state & SDL_BUTTON_LMASK)
					{
						const float move_scale = 0.005f;
						cam_phi += e.motion.xrel * move_scale;
						cam_theta = myClamp<float>(cam_theta - (float)e.motion.yrel * move_scale, 0.01f, Maths::pi<float>() - 0.01f);
					}
				}
				else if(e.type == SDL_MOUSEWHEEL)
				{
					const float move_speed = 3.f;
					cam_pos += forwards * (float)e.wheel.y * move_speed;
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
		SDL_Quit();
		return 0;
	}
	catch(glare::Exception& e)
	{
		std::cout << e.what() << std::endl;
		return 1;
	}
}
