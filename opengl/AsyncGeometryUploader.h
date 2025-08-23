/*=====================================================================
AsyncGeometryUploader.h
-----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Vector.h>
#include <queue>
class OpenGLEngine;
class OpenGLMeshRenderData;
class VBO;
struct __GLsync;
typedef struct __GLsync *GLsync;


struct AsyncUploadedGeometryInfo
{
	Reference<VBO> vbo;
	Reference<OpenGLMeshRenderData> meshdata;
};


/*=====================================================================
AsyncGeometryUploader
---------------------

=====================================================================*/
class AsyncGeometryUploader
{
public:
	AsyncGeometryUploader();
	~AsyncGeometryUploader();


	// Assumes source_vbo has been mapped, written to, and then unmapped.
	void startUploadingGeometry(Reference<OpenGLMeshRenderData> meshdata, Reference<VBO> source_vbo, Reference<VBO> dummy_vbo, size_t vert_data_src_offset_B, size_t index_data_src_offset_B, size_t vert_data_size_B, size_t index_data_size_B, size_t total_geom_size_B);

	void checkForUploadedGeometry(OpenGLEngine* opengl_engine, js::Vector<AsyncUploadedGeometryInfo, 16>& loaded_geom_out);

private:
	struct UploadingGeometry
	{
		Reference<OpenGLMeshRenderData> meshdata;

		Reference<VBO> source_vbo;
		//Reference<VBO> dummy_vbo;

		size_t vert_data_src_offset_B; // in source VBO
		size_t index_data_src_offset_B; // in source VBO
		size_t vert_data_size_B; // in source VBO
		size_t index_data_size_B; // in source VBO

		GLsync sync_ob;
	};

	std::queue<UploadingGeometry> uploading_geometry;

	std::queue<UploadingGeometry> copying_geometry;
};
