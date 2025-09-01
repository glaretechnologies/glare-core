/*=====================================================================
AsyncGeometryUploader.h
-----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include <utils/Reference.h>
#include <utils/Vector.h>
#include <utils/ThreadSafeRefCounted.h>
#include <queue>
class OpenGLEngine;
class OpenGLMeshRenderData;
class VBO;
struct __GLsync;
typedef struct __GLsync *GLsync;


class UploadingGeometryUserInfo : public ThreadSafeRefCounted
{
public:
	virtual ~UploadingGeometryUserInfo() {}
};


struct AsyncUploadedGeometryInfo
{
	Reference<VBO> vbo;
	Reference<OpenGLMeshRenderData> meshdata;
	Reference<UploadingGeometryUserInfo> user_info;
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

	// Geometry has been submitted to the GPU from glBufferSubData calls on source_vbo, or the geometry data has been written to source_vbo while it is memory-mapped.
	void startUploadingGeometry(Reference<OpenGLMeshRenderData> meshdata, Reference<VBO> source_vbo, Reference<VBO> dummy_vbo, size_t vert_data_src_offset_B, size_t index_data_src_offset_B, 
		size_t vert_data_size_B, size_t index_data_size_B, size_t total_geom_size_B, uint64 frame_num, Reference<UploadingGeometryUserInfo> user_info);

	void checkForUploadedGeometry(OpenGLEngine* opengl_engine, uint64 frame_num, js::Vector<AsyncUploadedGeometryInfo, 16>& loaded_geom_out);

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
		uint64 frame_num;

		Reference<UploadingGeometryUserInfo> user_info;
	};

	std::queue<UploadingGeometry> uploading_geometry;

	std::queue<UploadingGeometry> copying_geometry;
};
