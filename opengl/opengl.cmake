# opengl.cmake
# ------------
#
# Copyright Glare Technologies Limited 2023 -
#
# Produces the following file list variable that should be listed for a target:
# opengl
#
# Requires: GLARE_CORE_TRUNK_DIR_ENV variable should be set (Path to Glare-Core repo on disk)


if(NOT EMSCRIPTEN)
	set(gl3w  ${GLARE_CORE_TRUNK_DIR_ENV}/opengl/gl3w.c)
endif()

set(opengl
${gl3w}
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/AsyncTextureLoader.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/AsyncTextureLoader.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/DrawIndirectBuffer.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/DrawIndirectBuffer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/FrameBuffer.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/FrameBuffer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/RenderBuffer.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/RenderBuffer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/GLMeshBuilding.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/GLMeshBuilding.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/GLMemUsage.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/MeshPrimitiveBuilding.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/MeshPrimitiveBuilding.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VAO.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VAO.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VBO.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VBO.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VertexBufferAllocator.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VertexBufferAllocator.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLTexture.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLTexture.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLTextureKey.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngine.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngine.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngineTests.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngineTests.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLMeshRenderData.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLMeshRenderData.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLProgram.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLProgram.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLShader.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLShader.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLCircularBuffer.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLCircularBuffer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ShaderFileWatcherThread.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ShaderFileWatcherThread.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ShadowMapping.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ShadowMapping.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/SSBO.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/SSBO.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureAllocator.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureAllocator.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoading.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoading.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoadingTests.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoadingTests.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/UniformBufOb.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/UniformBufOb.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/WGL.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/WGL.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/Query.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/Query.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/BufferedTimeElapsedQuery.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/BufferedTimeElapsedQuery.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TimestampQuery.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TimestampQuery.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/SSAODebugging.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/SSAODebugging.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/PBO.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/PBO.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/PBOPool.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/PBOPool.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VBOPool.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/VBOPool.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/PBOAsyncTextureUploader.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/PBOAsyncTextureUploader.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/AsyncGeometryUploader.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/AsyncGeometryUploader.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/RenderStatsWidget.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/RenderStatsWidget.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLUploadThread.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLUploadThread.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLMemoryObject.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLMemoryObject.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLExtensions.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLExtensions.h
)


set(opengl_ui
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIButton.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIButton.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUITextButton.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUITextButton.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUI.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUI.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIImage.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIImage.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUITextView.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUITextView.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIWidget.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIWidget.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUICallbackHandler.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIText.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIText.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/FontCharTexCache.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/FontCharTexCache.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUILineEdit.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUILineEdit.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/TextEditingUtils.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/TextEditingUtils.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUISlider.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUISlider.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIInertWidget.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIInertWidget.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIGridContainer.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUIGridContainer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUICheckBox.cpp
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/ui/GLUICheckBox.h
)


set(opengl_shaders
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/build_imposters_compute_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/scatter_data_compute_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/clear_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/clear_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/common_vert_structures.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/common_frag_structures.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/depth_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/depth_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/downsize_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/downsize_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/edge_extract_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/edge_extract_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/env_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/env_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/final_imaging_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/final_imaging_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/gaussian_blur_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/gaussian_blur_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/imposter_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/imposter_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/outline_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/outline_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/overlay_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/overlay_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/participating_media_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/participating_media_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/phong_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/phong_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/transparent_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/transparent_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/water_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/water_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/compute_ssao_frag_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/compute_ssao_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/vert_utils.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/frag_utils.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/draw_aurora_tex_vert_shader.glsl
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/shaders/draw_aurora_tex_frag_shader.glsl
)


SOURCE_GROUP(opengl FILES ${opengl})
SOURCE_GROUP(opengl/shaders FILES ${opengl_shaders})
SOURCE_GROUP(opengl/ui FILES ${opengl_ui})
