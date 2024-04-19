cmake_minimum_required(VERSION 3.0)

project(testbed)

if(NOT EMSCRIPTEN)
	add_definitions(/MP) # Enable multi-processor compilation.
	add_definitions(/W4)
endif()

add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D__SSE3__ -D__SSSE3__ -D__SSE4_1__)

add_definitions(-DPNG_ALLOW_BENIGN_ERRORS=1)
add_definitions(-DPNG_INTEL_SSE=1)
add_definitions(-DPNG_NO_SETJMP=1)

add_definitions(-DBUILD_TESTS=1)

add_definitions(-DCMS_NO_REGISTER_KEYWORD=1)

if(EMSCRIPTEN)

	add_definitions(-fwasm-exceptions)
	add_definitions(-pthread)

	add_definitions(-DEMSCRIPTEN=1)
	
	add_definitions(-msimd128)
	add_definitions(-msse4.1)
	
	add_definitions(-O2)

	include_directories("C:/programming/emsdk/upstream/emscripten/cache/sysroot/include")
endif()



if(WIN32)
	if(MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1930)
		set(TESTBED_VS_VER 2019)
	elseif(MSVC_VERSION GREATER_EQUAL 1930 AND MSVC_VERSION LESS 1940)
		set(TESTBED_VS_VER 2022)
	else()
		message(FATAL_ERROR "Unhandled MSVC version")
	endif()
endif()



SET(GLARE_CORE_TRUNK         $ENV{GLARE_CORE_TRUNK_DIR})
SET(GLARE_CORE_TRUNK_DIR_ENV $ENV{GLARE_CORE_TRUNK_DIR})

SET(GLARE_CORE_LIBS $ENV{GLARE_CORE_LIBS})


set(SDL_DIR "${GLARE_CORE_LIBS}/SDL/SDL-release-2.26.2" CACHE FILEPATH "SDL source directory")
set(SDL_BUILD_DIR "${GLARE_CORE_LIBS}/SDL/sdl_build_vs2022" CACHE FILEPATH "SDL build directory")

MESSAGE("SDL_DIR: ${SDL_DIR}")
MESSAGE("SDL_BUILD_DIR: ${SDL_BUILD_DIR}")

add_definitions(-DBASE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")






# These libraries are checked into the glare-core repo to make setting up builds easier.
set(zlibdir		"${GLARE_CORE_TRUNK}/zlib")
set(pugixmldir	"${GLARE_CORE_TRUNK}/pugixml/src")
set(pngdir		"${GLARE_CORE_TRUNK}/libpng")
set(zstddir		"${GLARE_CORE_TRUNK}/zstd")
set(imguidir 	"${GLARE_CORE_TRUNK}/imgui")


if(WIN32)
	set(jpegdir		"${GLARE_CORE_LIBS}/libjpeg-turbo/libjpeg-turbo-3.0.0-vs${TESTBED_VS_VER}-install")
elseif(EMSCRIPTEN)
	set(jpegdir		"${GLARE_CORE_LIBS}/libjpeg-turbo/libjpeg-turbo-3.0.0-vs2022-install")
else()
	set(jpegdir 	"${GLARE_CORE_LIBS}/libjpeg-turbo/libjpeg-turbo-${SUBSTRATA_JPEGTURBO_VERSION}-install")
endif()

MESSAGE("jpegdir: ${jpegdir}")

set(libjpegturbodir "${GLARE_CORE_LIBS}/libjpeg-turbo-builds")
if(WIN32)
	set(libjpegturbodir "${libjpegturbodir}/vs_${TESTBED_VS_VER}_64") # Append dir suffix for VS version and bitness.
elseif(EMSCRIPTEN)
	set(libjpegturbodir "${libjpegturbodir}/vs_2022_64") # Append dir suffix for VS version and bitness.
endif()

include_directories("./")
include_directories(${SDL_DIR}/include)
include_directories(${GLARE_CORE_TRUNK})
include_directories(${GLARE_CORE_TRUNK}/utils)
include_directories(${GLARE_CORE_TRUNK}/opengl)
include_directories(${zstddir}/lib)
include_directories(${zstddir}/lib/common) # for xxhash
include_directories(${GLARE_CORE_TRUNK}/little_cms/include)
include_directories(${GLARE_CORE_TRUNK}/giflib/lib)

include_directories(${jpegdir}/include)  # libjpeg-turbo-master (jpegdir) has most of the source,
include_directories(${libjpegturbodir})  # libjpegturbodir has jconfig.h
include_directories(${pngdir})
include_directories(${zlibdir})
include_directories(${imguidir})



#============== Tracy profiler ==============
include_directories("${GLARE_CORE_TRUNK}/tracy/public")
set(tracy_files "${GLARE_CORE_TRUNK}/tracy/public/TracyClient.cpp")


#============== OpenEXR ==============
include(${GLARE_CORE_TRUNK}/OpenEXR/openexr.cmake)


#============== ImGui ==============
set (imgui
${imguidir}/imgui.cpp
${imguidir}/imgui.h
${imguidir}/imgui_demo.cpp
${imguidir}/imgui_draw.cpp
${imguidir}/imgui_internal.h
${imguidir}/imgui_widgets.cpp
${imguidir}/imgui_tables.cpp
${imguidir}/imstb_rectpack.h
${imguidir}/imstb_textedit.h
${imguidir}/imstb_truetype.h
${imguidir}/backends/imgui_impl_sdl2.cpp
${imguidir}/backends/imgui_impl_sdl2.h
${imguidir}/backends/imgui_impl_opengl3.cpp
${imguidir}/backends/imgui_impl_opengl3.h
)


#============== libpng ==============
set(libpng
${pngdir}/png.c ${pngdir}/pngerror.c ${pngdir}/pngget.c ${pngdir}/pngmem.c ${pngdir}/pngpread.c ${pngdir}/pngread.c ${pngdir}/pngrio.c ${pngdir}/pngrtran.c
${pngdir}/pngrutil.c ${pngdir}/pngset.c ${pngdir}/pngtrans.c ${pngdir}/pngwio.c ${pngdir}/pngwrite.c ${pngdir}/pngwtran.c ${pngdir}/pngwutil.c
${pngdir}/png.h
${pngdir}/pngconf.h
${pngdir}/pngdebug.h
${pngdir}/pnginfo.h
${pngdir}/pnglibconf.h
${pngdir}/pngpriv.h
${pngdir}/pngstruct.h

${pngdir}/intel/intel_init.c
${pngdir}/intel/filter_sse2_intrinsics.c
)


#============== zlib ==============
set(zlib
${zlibdir}/adler32.c
${zlibdir}/compress.c
${zlibdir}/crc32.c
${zlibdir}/deflate.c
${zlibdir}/inffast.c
${zlibdir}/inflate.c
${zlibdir}/inftrees.c
${zlibdir}/trees.c
${zlibdir}/uncompr.c
${zlibdir}/zutil.c
${zlibdir}/crc32.h
${zlibdir}/deflate.h
${zlibdir}/inffast.h
${zlibdir}/inffixed.h
${zlibdir}/inflate.h
${zlibdir}/inftrees.h
${zlibdir}/trees.h
${zlibdir}/zconf.h
${zlibdir}/zlib.h
${zlibdir}/zutil.h
)


#============== pugixml ==============
set(pugixml
${pugixmldir}/pugixml.cpp
${pugixmldir}/pugixml.hpp
${pugixmldir}/pugiconfig.hpp
)

# MESSAGE(${pugixml})

FILE(GLOB giflib "${GLARE_CORE_TRUNK}/giflib/lib/*.c" "${GLARE_CORE_TRUNK}/giflib/lib/*.cpp" "${GLARE_CORE_TRUNK}/giflib/lib/*.h")
FILE(GLOB little_cms "${GLARE_CORE_TRUNK}/little_cms/src/*.c"  "${GLARE_CORE_TRUNK}/little_cms/include/*.h")
FILE(GLOB zstandard 
"${zstddir}/lib/*.h"
"${zstddir}/lib/common/*.c"
"${zstddir}/lib/common/*.h"
"${zstddir}/lib/compress/*.c"
"${zstddir}/lib/compress/*.h"
"${zstddir}/lib/decompress/*.c"
"${zstddir}/lib/decompress/*.h"
)

SOURCE_GROUP(libpng FILES ${libpng})
SOURCE_GROUP(zlib FILES ${zlib})
SOURCE_GROUP(pugixml FILES ${pugixml})
SOURCE_GROUP(giflib FILES ${giflib})
SOURCE_GROUP(little_cms FILES ${little_cms})
SOURCE_GROUP(zstandard FILES ${zstandard})


#============== glare-core/indigo ==============
set(indigo
${GLARE_CORE_TRUNK}/indigo/TextureServer.cpp
${GLARE_CORE_TRUNK}/indigo/TextureServer.h

${GLARE_CORE_TRUNK}/dll/IndigoMesh.cpp
${GLARE_CORE_TRUNK}/dll/include/IndigoMesh.h
${GLARE_CORE_TRUNK}/dll/IndigoAllocation.cpp
${GLARE_CORE_TRUNK}/dll/include/IndigoAllocation.h
)

#============== graphics ==============
set(graphics
${GLARE_CORE_TRUNK}/graphics/AnimationData.cpp
${GLARE_CORE_TRUNK}/graphics/AnimationData.h
${GLARE_CORE_TRUNK}/graphics/BatchedMesh.cpp
${GLARE_CORE_TRUNK}/graphics/BatchedMesh.h
${GLARE_CORE_TRUNK}/graphics/bitmap.cpp
${GLARE_CORE_TRUNK}/graphics/bitmap.h
${GLARE_CORE_TRUNK}/graphics/Image.cpp
${GLARE_CORE_TRUNK}/graphics/Image.h
${GLARE_CORE_TRUNK}/graphics/Map2D.cpp
${GLARE_CORE_TRUNK}/graphics/Map2D.h
${GLARE_CORE_TRUNK}/graphics/ImageMap.cpp
${GLARE_CORE_TRUNK}/graphics/ImageMap.h
${GLARE_CORE_TRUNK}/graphics/imformatdecoder.cpp
${GLARE_CORE_TRUNK}/graphics/imformatdecoder.h
${GLARE_CORE_TRUNK}/graphics/jpegdecoder.cpp
${GLARE_CORE_TRUNK}/graphics/jpegdecoder.h
${GLARE_CORE_TRUNK}/graphics/PNGDecoder.cpp
${GLARE_CORE_TRUNK}/graphics/PNGDecoder.h
${GLARE_CORE_TRUNK}/graphics/tgadecoder.cpp
${GLARE_CORE_TRUNK}/graphics/tgadecoder.h
${GLARE_CORE_TRUNK}/graphics/bmpdecoder.cpp
${GLARE_CORE_TRUNK}/graphics/bmpdecoder.h
${GLARE_CORE_TRUNK}/graphics/EXRDecoder.cpp
${GLARE_CORE_TRUNK}/graphics/EXRDecoder.h
${GLARE_CORE_TRUNK}/graphics/FloatDecoder.cpp
${GLARE_CORE_TRUNK}/graphics/FloatDecoder.h
${GLARE_CORE_TRUNK}/graphics/GifDecoder.cpp
${GLARE_CORE_TRUNK}/graphics/GifDecoder.h
${GLARE_CORE_TRUNK}/graphics/DXTCompression.cpp
${GLARE_CORE_TRUNK}/graphics/DXTCompression.h
${GLARE_CORE_TRUNK}/graphics/KTXDecoder.cpp
${GLARE_CORE_TRUNK}/graphics/KTXDecoder.h
${GLARE_CORE_TRUNK}/graphics/CompressedImage.cpp
${GLARE_CORE_TRUNK}/graphics/CompressedImage.h
${GLARE_CORE_TRUNK}/graphics/PerlinNoise.cpp
${GLARE_CORE_TRUNK}/graphics/PerlinNoise.h
${GLARE_CORE_TRUNK}/graphics/Voronoi.cpp
${GLARE_CORE_TRUNK}/graphics/Voronoi.h
${GLARE_CORE_TRUNK}/graphics/GridNoise.cpp
${GLARE_CORE_TRUNK}/graphics/GridNoise.h
${GLARE_CORE_TRUNK}/graphics/TextureProcessing.cpp
${GLARE_CORE_TRUNK}/graphics/TextureProcessing.h
${GLARE_CORE_TRUNK}/graphics/SRGBUtils.cpp
${GLARE_CORE_TRUNK}/graphics/SRGBUtils.h
${GLARE_CORE_TRUNK}/graphics/Colour4f.cpp
${GLARE_CORE_TRUNK}/graphics/Colour4f.h
${GLARE_CORE_TRUNK}/graphics/Image4f.cpp
${GLARE_CORE_TRUNK}/graphics/Image4f.h
${GLARE_CORE_TRUNK}/graphics/BoxFilterFunction.cpp
${GLARE_CORE_TRUNK}/graphics/BoxFilterFunction.h
${GLARE_CORE_TRUNK}/graphics/FilterFunction.cpp
${GLARE_CORE_TRUNK}/graphics/FilterFunction.h
${GLARE_CORE_TRUNK}/graphics/FormatDecoderGLTF.cpp
${GLARE_CORE_TRUNK}/graphics/FormatDecoderGLTF.h
${GLARE_CORE_TRUNK}/graphics/MeshSimplification.cpp
${GLARE_CORE_TRUNK}/graphics/MeshSimplification.h
${GLARE_CORE_TRUNK}/graphics/TextRenderer.cpp
${GLARE_CORE_TRUNK}/graphics/TextRenderer.h
)

include(${GLARE_CORE_TRUNK}/opengl/opengl.cmake)

set(glui
${GLARE_CORE_TRUNK}/opengl/ui/FontCharTexCache.cpp
${GLARE_CORE_TRUNK}/opengl/ui/FontCharTexCache.h
${GLARE_CORE_TRUNK}/opengl/ui/GLUI.cpp
${GLARE_CORE_TRUNK}/opengl/ui/GLUI.h
${GLARE_CORE_TRUNK}/opengl/ui/GLUIButton.cpp
${GLARE_CORE_TRUNK}/opengl/ui/GLUIButton.h
${GLARE_CORE_TRUNK}/opengl/ui/GLUIImage.cpp
${GLARE_CORE_TRUNK}/opengl/ui/GLUIImage.h
${GLARE_CORE_TRUNK}/opengl/ui/GLUIText.cpp
${GLARE_CORE_TRUNK}/opengl/ui/GLUIText.h
${GLARE_CORE_TRUNK}/opengl/ui/GLUITextView.cpp
${GLARE_CORE_TRUNK}/opengl/ui/GLUITextView.h
${GLARE_CORE_TRUNK}/opengl/ui/GLUIWidget.cpp
${GLARE_CORE_TRUNK}/opengl/ui/GLUIWidget.h
)


set(utils
${GLARE_CORE_TRUNK}/utils/BestFitAllocator.cpp
${GLARE_CORE_TRUNK}/utils/BestFitAllocator.h
${GLARE_CORE_TRUNK}/utils/BufferViewInStream.cpp
${GLARE_CORE_TRUNK}/utils/BufferViewInStream.h
${GLARE_CORE_TRUNK}/utils/Clock.cpp
${GLARE_CORE_TRUNK}/utils/Clock.h
${GLARE_CORE_TRUNK}/utils/CycleTimer.cpp
${GLARE_CORE_TRUNK}/utils/CycleTimer.h
${GLARE_CORE_TRUNK}/utils/ConPrint.cpp
${GLARE_CORE_TRUNK}/utils/ConPrint.h
${GLARE_CORE_TRUNK}/utils/StringUtils.cpp
${GLARE_CORE_TRUNK}/utils/StringUtils.h
${GLARE_CORE_TRUNK}/utils/Mutex.cpp
${GLARE_CORE_TRUNK}/utils/Mutex.h
${GLARE_CORE_TRUNK}/utils/Lock.cpp
${GLARE_CORE_TRUNK}/utils/Lock.h
#${GLARE_CORE_TRUNK}/utils/DynamicLib.cpp
#${GLARE_CORE_TRUNK}/utils/DynamicLib.h
${GLARE_CORE_TRUNK}/utils/StandardPrintOutput.cpp
${GLARE_CORE_TRUNK}/utils/StandardPrintOutput.h
${GLARE_CORE_TRUNK}/utils/FileUtils.cpp
${GLARE_CORE_TRUNK}/utils/FileUtils.h
${GLARE_CORE_TRUNK}/utils/MemMappedFile.cpp
${GLARE_CORE_TRUNK}/utils/MemMappedFile.h
${GLARE_CORE_TRUNK}/utils/PlatformUtils.cpp
${GLARE_CORE_TRUNK}/utils/PlatformUtils.h
${GLARE_CORE_TRUNK}/utils/TaskManager.cpp
${GLARE_CORE_TRUNK}/utils/TaskManager.h
${GLARE_CORE_TRUNK}/utils/Task.cpp
${GLARE_CORE_TRUNK}/utils/Task.h
${GLARE_CORE_TRUNK}/utils/Condition.cpp
${GLARE_CORE_TRUNK}/utils/Condition.h
${GLARE_CORE_TRUNK}/utils/MyThread.cpp
${GLARE_CORE_TRUNK}/utils/MyThread.h
${GLARE_CORE_TRUNK}/utils/TaskRunnerThread.cpp
${GLARE_CORE_TRUNK}/utils/TaskRunnerThread.h
${GLARE_CORE_TRUNK}/utils/ThreadManager.cpp
${GLARE_CORE_TRUNK}/utils/ThreadManager.h
${GLARE_CORE_TRUNK}/utils/Parser.cpp
${GLARE_CORE_TRUNK}/utils/Parser.h
${GLARE_CORE_TRUNK}/utils/FileHandle.cpp
${GLARE_CORE_TRUNK}/utils/FileHandle.h
${GLARE_CORE_TRUNK}/utils/Timer.cpp
${GLARE_CORE_TRUNK}/utils/Timer.h
${GLARE_CORE_TRUNK}/utils/MemAlloc.cpp
${GLARE_CORE_TRUNK}/utils/MemAlloc.h
${GLARE_CORE_TRUNK}/utils/FileInStream.cpp
${GLARE_CORE_TRUNK}/utils/FileInStream.h
${GLARE_CORE_TRUNK}/utils/FileOutStream.cpp
${GLARE_CORE_TRUNK}/utils/FileOutStream.h
${GLARE_CORE_TRUNK}/utils/OutStream.cpp
${GLARE_CORE_TRUNK}/utils/OutStream.h
${GLARE_CORE_TRUNK}/utils/InStream.cpp
${GLARE_CORE_TRUNK}/utils/InStream.h
${GLARE_CORE_TRUNK}/utils/BufferInStream.cpp
${GLARE_CORE_TRUNK}/utils/BufferInStream.h
${GLARE_CORE_TRUNK}/utils/BufferOutStream.cpp
${GLARE_CORE_TRUNK}/utils/BufferOutStream.h
${GLARE_CORE_TRUNK}/utils/TestUtils.cpp
${GLARE_CORE_TRUNK}/utils/TestUtils.h
${GLARE_CORE_TRUNK}/utils/RuntimeCheck.cpp
${GLARE_CORE_TRUNK}/utils/RuntimeCheck.h
${GLARE_CORE_TRUNK}/utils/MessageableThread.cpp
${GLARE_CORE_TRUNK}/utils/MessageableThread.h
${GLARE_CORE_TRUNK}/utils/GeneralMemAllocator.cpp
${GLARE_CORE_TRUNK}/utils/GeneralMemAllocator.h
${GLARE_CORE_TRUNK}/utils/PoolAllocator.cpp
${GLARE_CORE_TRUNK}/utils/PoolAllocator.h
${GLARE_CORE_TRUNK}/utils/KillThreadMessage.cpp
${GLARE_CORE_TRUNK}/utils/KillThreadMessage.h
${GLARE_CORE_TRUNK}/utils/ThreadMessage.cpp
${GLARE_CORE_TRUNK}/utils/ThreadMessage.h
${GLARE_CORE_TRUNK}/utils/JSONParser.cpp
${GLARE_CORE_TRUNK}/utils/JSONParser.h
${GLARE_CORE_TRUNK}/utils/Base64.cpp
${GLARE_CORE_TRUNK}/utils/Base64.h
${GLARE_CORE_TRUNK}/utils/UTF8Utils.cpp
${GLARE_CORE_TRUNK}/utils/UTF8Utils.h
)

set(maths
${GLARE_CORE_TRUNK}/maths/SSE.cpp
${GLARE_CORE_TRUNK}/maths/SSE.h
${GLARE_CORE_TRUNK}/maths/Vec4f.cpp
${GLARE_CORE_TRUNK}/maths/Vec4f.h
${GLARE_CORE_TRUNK}/maths/Matrix4f.cpp
${GLARE_CORE_TRUNK}/maths/Matrix4f.h
)


FILE(GLOB double_conversion "${GLARE_CORE_TRUNK}/double-conversion/*.cc" "${GLARE_CORE_TRUNK}/double-conversion/*.h")


set(MESHOPTIMIZER_DIR "${GLARE_CORE_TRUNK}/meshoptimizer/src")

set(meshoptimizer
${MESHOPTIMIZER_DIR}/allocator.cpp
${MESHOPTIMIZER_DIR}/meshoptimizer.h
${MESHOPTIMIZER_DIR}/allocator.cpp
${MESHOPTIMIZER_DIR}/clusterizer.cpp
${MESHOPTIMIZER_DIR}/indexcodec.cpp
${MESHOPTIMIZER_DIR}/indexgenerator.cpp
${MESHOPTIMIZER_DIR}/overdrawanalyzer.cpp
${MESHOPTIMIZER_DIR}/overdrawoptimizer.cpp
${MESHOPTIMIZER_DIR}/simplifier.cpp
${MESHOPTIMIZER_DIR}/spatialorder.cpp
${MESHOPTIMIZER_DIR}/stripifier.cpp
${MESHOPTIMIZER_DIR}/vcacheanalyzer.cpp
${MESHOPTIMIZER_DIR}/vcacheoptimizer.cpp
${MESHOPTIMIZER_DIR}/vertexcodec.cpp
${MESHOPTIMIZER_DIR}/vertexfilter.cpp
${MESHOPTIMIZER_DIR}/vfetchanalyzer.cpp
${MESHOPTIMIZER_DIR}/vfetchoptimizer.cpp   
)

set(jpegsrcdir		"${GLARE_CORE_LIBS}/libjpeg-turbo/libjpeg-turbo-3.0.0")

if(0) # EMSCRIPTEN)
	set(jpeg_src
	${jpegsrcdir}/jaricom.c
	${jpegsrcdir}/jcapimin.c
	${jpegsrcdir}/jcapistd.c
	${jpegsrcdir}/jcarith.c
	${jpegsrcdir}/jccoefct.c
	${jpegsrcdir}/jccolext.c
	${jpegsrcdir}/jccolor.c
	${jpegsrcdir}/jcdctmgr.c
	${jpegsrcdir}/jcdiffct.c
	${jpegsrcdir}/jchuff.c
	${jpegsrcdir}/jcicc.c
	${jpegsrcdir}/jcinit.c
	${jpegsrcdir}/jclhuff.c
	${jpegsrcdir}/jclossls.c
	${jpegsrcdir}/jcmainct.c
	${jpegsrcdir}/jcmarker.c
	${jpegsrcdir}/jcmaster.c
	${jpegsrcdir}/jcomapi.c
	${jpegsrcdir}/jcparam.c
	${jpegsrcdir}/jcphuff.c
	${jpegsrcdir}/jcprepct.c
	${jpegsrcdir}/jcsample.c
	${jpegsrcdir}/jcstest.c
	${jpegsrcdir}/jctrans.c
	${jpegsrcdir}/jdapimin.c
	${jpegsrcdir}/jdapistd.c
	${jpegsrcdir}/jdarith.c
	${jpegsrcdir}/jdatadst-tj.c
	${jpegsrcdir}/jdatadst.c
	${jpegsrcdir}/jdatasrc-tj.c
	${jpegsrcdir}/jdatasrc.c
	${jpegsrcdir}/jdcoefct.c
	${jpegsrcdir}/jdcol565.c
	${jpegsrcdir}/jdcolext.c
	${jpegsrcdir}/jdcolor.c
	${jpegsrcdir}/jddctmgr.c
	${jpegsrcdir}/jddiffct.c
	${jpegsrcdir}/jdhuff.c
	${jpegsrcdir}/jdicc.c
	${jpegsrcdir}/jdinput.c
	${jpegsrcdir}/jdlhuff.c
	${jpegsrcdir}/jdlossls.c
	${jpegsrcdir}/jdmainct.c
	${jpegsrcdir}/jdmarker.c
	${jpegsrcdir}/jdmaster.c
	${jpegsrcdir}/jdmerge.c
	${jpegsrcdir}/jdmrg565.c
	${jpegsrcdir}/jdmrgext.c
	${jpegsrcdir}/jdphuff.c
	${jpegsrcdir}/jdpostct.c
	${jpegsrcdir}/jdsample.c
	${jpegsrcdir}/jdtrans.c
	${jpegsrcdir}/jerror.c
	${jpegsrcdir}/jfdctflt.c
	${jpegsrcdir}/jfdctfst.c
	${jpegsrcdir}/jfdctint.c
	${jpegsrcdir}/jidctflt.c
	${jpegsrcdir}/jidctfst.c
	${jpegsrcdir}/jidctint.c
	${jpegsrcdir}/jidctred.c
	${jpegsrcdir}/jmemmgr.c
	${jpegsrcdir}/jmemnobs.c
	${jpegsrcdir}/jpegtran.c
	${jpegsrcdir}/jquant1.c
	${jpegsrcdir}/jquant2.c
	${jpegsrcdir}/jstdhuff.c
	${jpegsrcdir}/jutils.c
	)
endif()


#================ FreeType =============
set(freetypedir "${GLARE_CORE_TRUNK_DIR_ENV}/freetype")

include_directories("${freetypedir}/include")
include_directories("${freetypedir}/include/freetype/internal")
add_definitions("-DFT2_BUILD_LIBRARY") # NOTE: not sure if needed

file(GLOB freetype_PUBLIC_HEADERS "${freetypedir}/include/ft2build.h" "${freetypedir}/include/freetype/*.h")
file(GLOB freetype_PUBLIC_CONFIG_HEADERS "${freetypedir}/include/freetype/config/*.h")
file(GLOB freetype_PRIVATE_HEADERS "${freetypedir}/include/freetype/internal/*.h")

set(freetype
${freetype_PUBLIC_HEADERS}
${freetype_PUBLIC_CONFIG_HEADERS}
${freetype_PRIVATE_HEADERS}
${freetypedir}/src/autofit/autofit.c
${freetypedir}/src/base/ftbase.c
${freetypedir}/src/base/ftbbox.c
${freetypedir}/src/base/ftbdf.c
${freetypedir}/src/base/ftbitmap.c
${freetypedir}/src/base/ftcid.c
${freetypedir}/src/base/ftfstype.c
${freetypedir}/src/base/ftgasp.c
${freetypedir}/src/base/ftglyph.c
${freetypedir}/src/base/ftgxval.c
${freetypedir}/src/base/ftinit.c
${freetypedir}/src/base/ftmm.c
${freetypedir}/src/base/ftotval.c
${freetypedir}/src/base/ftpatent.c
${freetypedir}/src/base/ftpfr.c
${freetypedir}/src/base/ftstroke.c
${freetypedir}/src/base/ftsynth.c
${freetypedir}/src/base/fttype1.c
${freetypedir}/src/base/ftwinfnt.c
#${freetypedir}/src/bdf/bdf.c
${freetypedir}/src/bzip2/ftbzip2.c
${freetypedir}/src/cache/ftcache.c
${freetypedir}/src/cff/cff.c # OpenType (.otf) driver component
#${freetypedir}/src/cid/type1cid.c
${freetypedir}/src/gzip/ftgzip.c
${freetypedir}/src/lzw/ftlzw.c
#${freetypedir}/src/pcf/pcf.c
#${freetypedir}/src/pfr/pfr.c
${freetypedir}/src/psaux/psaux.c
${freetypedir}/src/pshinter/pshinter.c
${freetypedir}/src/psnames/psnames.c
${freetypedir}/src/raster/raster.c
#${freetypedir}/src/sdf/sdf.c
${freetypedir}/src/sfnt/sfnt.c
${freetypedir}/src/smooth/smooth.c
${freetypedir}/src/svg/svg.c
${freetypedir}/src/truetype/truetype.c
#${freetypedir}/src/type1/type1.c
#${freetypedir}/src/type42/type42.c
#${freetypedir}/src/winfonts/winfnt.c
)

if(UNIX)
	set(freetype ${freetype} "${freetypedir}/builds/unix/ftsystem.c")
elseif(WIN32)
	set(freetype ${freetype} "${freetypedir}/builds/windows/ftsystem.c")
else()
	set(freetype ${freetype} "${freetypedir}/src/base/ftsystem.c")
endif ()

if (WIN32)
	#enable_language(RC)
	#list(APPEND BASE_SRCS builds/windows/ftdebug.c
	#                      src/base/ftver.rc)

	set(freetype ${freetype} "${freetypedir}/builds/windows/ftdebug.c")
else ()
	set(freetype ${freetype} "${freetypedir}/src/base/ftdebug.c")
endif ()


SOURCE_GROUP(indigo FILES ${indigo})
SOURCE_GROUP(graphics FILES ${graphics})
SOURCE_GROUP(opengl FILES ${opengl})
SOURCE_GROUP(utils FILES ${utils})
SOURCE_GROUP(maths FILES ${maths})
SOURCE_GROUP(double-conversion FILES ${double_conversion})
SOURCE_GROUP(imgui FILES ${imgui})
SOURCE_GROUP(meshoptimizer FILES ${meshoptimizer})
SOURCE_GROUP(jpeg-turbo FILES ${jpeg_src})
SOURCE_GROUP(glui FILES ${glui})
SOURCE_GROUP(freetype FILES ${freetype})


add_executable(testbed
testbed.cpp
${indigo}
${graphics}
${opengl}
${opengl_shaders}
${utils}
${maths}
${double_conversion}
${openexr_all_files}
${libjpg}
${libpng}
${zlib}
${pugixml}
${giflib}
${little_cms}
${zstandard}
${imgui}
${tracy_files}
${meshoptimizer}
${jpeg_src}
${glui}
${freetype}
)


#if(WIN32)
#	SET(TURBOJPEG_LIB ${libjpegturbodir}/$(Configuration)/turbojpeg-static.lib)
#else()
#	SET(TURBOJPEG_LIB ${libjpegturbodir}/libjpeg.a)
#endif()


if(NOT EMSCRIPTEN)
	target_link_libraries(testbed
		debug "${SDL_BUILD_DIR}/Debug/SDL2d.lib" 
		debug "${SDL_BUILD_DIR}/Debug/SDL2maind.lib" 
		optimized "${SDL_BUILD_DIR}/RelWithDebInfo/SDL2.lib" 
		optimized "${SDL_BUILD_DIR}/RelWithDebInfo/SDL2main.lib" 
		Opengl32.lib
		debug     "${jpegdir}/lib/turbojpeg-static.lib" 
		optimized "${jpegdir}/lib/turbojpeg-static.lib" 

		#Advapi32.lib # For regristry funcs
	)
endif()

if(EMSCRIPTEN)
	target_link_libraries(testbed
		${GLARE_CORE_LIBS}/libjpeg-turbo/libjpeg-turbo-emscripten-build/libjpeg.a
		${GLARE_CORE_LIBS}/SDL/sdl_build_emscripten/libSDL2.a
	)
	
	set(CMAKE_EXECUTABLE_SUFFIX ".html") # Make emscripten spit out an HTML file for testing
	
	target_link_options(testbed PRIVATE 
		"-s USE_WEBGL=1"
		"-sMIN_WEBGL_VERSION=2"
		"-sMAX_WEBGL_VERSION=2"
		"-sGL_ENABLE_GET_PROC_ADDRESS"
#		"-sNO_DISABLE_EXCEPTION_CATCHING"
		"--emrun"
		"-gsource-map"
		"-fwasm-exceptions"
		"-sALLOW_MEMORY_GROWTH"
		"-pthread"
		"-s USE_SDL=2"
		"-sPTHREAD_POOL_SIZE=40"
		"-O2"
		--preload-file data
	)
	
	set_target_properties(testbed PROPERTIES LINK_DEPENDS "data")
endif()

