# indigo libs

FILE(GLOB fftss "../../../fftss-3.0-20071031/fftss-3.0/libfftss/fftss*.c")

set(fftss 
${fftss}
${fftssdir}/r4_cmplx_n.c  ${fftssdir}/r4_fma_n.c  ${fftssdir}/r4_fma_o.c  ${fftssdir}/r4_fma_u1.c  ${fftssdir}/r4_fma_u4.c ${fftssdir}/r4_n.c ${fftssdir}/r4_o.c 
${fftssdir}/r4_sse2_1_n.c ${fftssdir}/r4_sse2_o.c ${fftssdir}/r4_sse2_u1.c ${fftssdir}/r4_sse2_u4.c
${fftssdir}/r4_sse3_h_n.c ${fftssdir}/r4_sse3_h_o.c ${fftssdir}/r4_sse3_h_u1.c ${fftssdir}/r4_sse3_n.c ${fftssdir}/r4_sse3_o.c ${fftssdir}/r4_sse3_u1.c ${fftssdir}/r4_sse3_u4.c 
${fftssdir}/r4_u1.c ${fftssdir}/r4_u4.c

${fftssdir}/r8_fma_n.c ${fftssdir}/r8_fma_o.c ${fftssdir}/r8_fma_u1.c ${fftssdir}/r8_n.c ${fftssdir}/r8_o.c 
${fftssdir}/r8_sse2_n.c ${fftssdir}/r8_sse2_o.c ${fftssdir}/r8_sse2_u1.c ${fftssdir}/r8_u1.c
)

#MESSAGE(${fftss})


FILE(GLOB ilmbase "../../../ilmbase-1.0.1/Half/*.cpp" "../../../ilmbase-1.0.1/Iex/*.cpp" "../../../ilmbase-1.0.1/IlmThread/*.cpp" "../../../ilmbase-1.0.1/Imath/*.cpp")
FILE(GLOB openexr "../../../openexr-1.6.1/IlmImf/*.cpp")
FILE(GLOB lib3ds "../../../lib3ds-1.2.0/lib3ds/*.c")
FILE(GLOB libjpg "${jpegdir}/ja*.c" "${jpegdir}/jc*.c" "${jpegdir}/jd*.c" "${jpegdir}/jf*.c" "${jpegdir}/ji*.c" "${jpegdir}/jq*.c"
"${jpegdir}/jutils.c"
"${jpegdir}/jmemmgr.c"
"${jpegdir}/jmemnobs.c"
"${jpegdir}/jerror.c"
)

set(libpng
${pngdir}/png.c ${pngdir}/pngerror.c ${pngdir}/pngget.c ${pngdir}/pngmem.c ${pngdir}/pngpread.c ${pngdir}/pngread.c ${pngdir}/pngrio.c ${pngdir}/pngrtran.c
${pngdir}/pngrutil.c ${pngdir}/pngset.c ${pngdir}/pngtrans.c ${pngdir}/pngwio.c ${pngdir}/pngwrite.c ${pngdir}/pngwtran.c ${pngdir}/pngwutil.c
)

set(libtiff
${tiffdir}/tif_aux.c ${tiffdir}/tif_close.c 
${tiffdir}/tif_codec.c ${tiffdir}/tif_color.c ${tiffdir}/tif_compress.c ${tiffdir}/tif_dir.c 
${tiffdir}/tif_dirinfo.c ${tiffdir}/tif_dirread.c 
${tiffdir}/tif_dirwrite.c ${tiffdir}/tif_dumpmode.c ${tiffdir}/tif_error.c ${tiffdir}/tif_extension.c
${tiffdir}/tif_fax3.c ${tiffdir}/tif_fax3sm.c ${tiffdir}/tif_flush.c ${tiffdir}/tif_getimage.c ${tiffdir}/tif_jpeg.c 
${tiffdir}/tif_luv.c ${tiffdir}/tif_lzw.c ${tiffdir}/tif_next.c ${tiffdir}/tif_ojpeg.c 
${tiffdir}/tif_open.c ${tiffdir}/tif_packbits.c ${tiffdir}/tif_pixarlog.c ${tiffdir}/tif_predict.c ${tiffdir}/tif_print.c
${tiffdir}/tif_read.c ${tiffdir}/tif_strip.c ${tiffdir}/tif_swab.c ${tiffdir}/tif_thunder.c ${tiffdir}/tif_tile.c 
${tiffdir}/tif_unix.c ${tiffdir}/tif_version.c ${tiffdir}/tif_warning.c ${tiffdir}/tif_write.c ${tiffdir}/tif_zip.c
)

FILE(GLOB lua "../../../lua-5.1.4/src/*.c")

set(zlib
${zlibdir}/adler32.c ${zlibdir}/compress.c ${zlibdir}/crc32.c ${zlibdir}/deflate.c ${zlibdir}/gzclose.c ${zlibdir}/gzlib.c ${zlibdir}/gzread.c ${zlibdir}/gzwrite.c
${zlibdir}/infback.c ${zlibdir}/inffast.c ${zlibdir}/inflate.c ${zlibdir}/inftrees.c ${zlibdir}/trees.c ${zlibdir}/uncompr.c ${zlibdir}/zutil.c
${zlibdir}/contrib/minizip/unzip.c ${zlibdir}/contrib/minizip/zip.c ${zlibdir}/contrib/minizip/ioapi.c
)


set(tinyxml
${tinyxmldir}/tinyxml.cpp
${tinyxmldir}/tinyxmlerror.cpp
${tinyxmldir}/tinyxmlparser.cpp
)

# MESSAGE(${libpng})


SOURCE_GROUP(fftss FILES ${fftss})
SOURCE_GROUP(ilmbase FILES ${ilmbase})
SOURCE_GROUP(openexr FILES ${openexr})
SOURCE_GROUP(lib3ds FILES ${lib3ds})
SOURCE_GROUP(libjpg FILES ${libjpg})
SOURCE_GROUP(libpng FILES ${libpng})
SOURCE_GROUP(libtiff FILES ${libtiff})
#SOURCE_GROUP(lua FILES ${lua})
SOURCE_GROUP(zlib FILES ${zlib})
SOURCE_GROUP(tinyxml FILES ${tinyxml})


add_definitions(-DOPENEXR_SUPPORT -DPNG_ALLOW_BENIGN_ERRORS -D_SECURE_SCL=0 -DLIB3DS_EXPORTS -DTIXML_USE_STL)

add_definitions(/MP)

add_library(indigo_libs
${fftss}
${ilmbase}
${openexr}
${lib3ds}
${libjpg}
${libpng}
${libtiff}
#${lua}
${zlib}
${tinyxml}
)