# opengl.cmake
# ------------
#
# Copyright Glare Technologies Limited 2023 -
#
# Produces the following file list variable that should be listed for a target:
# opengl
#
# Requires: GLARE_CORE_TRUNK_DIR_ENV variable should be set (Path to Glare-Core repo on disk)


set(opengl 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/gl3w.c 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/DrawIndirectBuffer.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/DrawIndirectBuffer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/FrameBuffer.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/FrameBuffer.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/GLMeshBuilding.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/GLMeshBuilding.h
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
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngine.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngine.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngineTests.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/OpenGLEngineTests.h
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
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoading.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoading.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoadingTests.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/TextureLoadingTests.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/UniformBufOb.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/UniformBufOb.h
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/WGL.cpp 
${GLARE_CORE_TRUNK_DIR_ENV}/opengl/WGL.h
)
