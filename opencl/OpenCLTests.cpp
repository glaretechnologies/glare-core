/*=====================================================================
OpenCLTests.cpp
---------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "OpenCLTests.h"


#include "OpenCLKernel.h"
#include "OpenCLBuffer.h"
#include "OpenCLPathTracingKernel.h"
#include "../indigo/StandardPrintOutput.h"
#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/PlatformUtils.h"


#if BUILD_TESTS


#if USE_OPENCL


OpenCLTests::OpenCLTests()
{
}


OpenCLTests::~OpenCLTests()
{
}


void OpenCLTests::runTestsOnDevice(const gpuDeviceInfo& opencl_device)
{
	conPrint("OpenCLTests::runTestsOnDevice(), device: " + opencl_device.device_name);

	try
	{
		OpenCL* opencl = getGlobalOpenCL();
		testAssert(opencl != NULL);

		// Initialise OpenCL context and command queue for this device
		cl_context context;
		cl_command_queue command_queue;
		opencl->deviceInit(opencl_device, context, command_queue);


		// Read test kernel from disk
		const std::string kernel_path = TestUtils::getIndigoTestReposDir() + "/opencl/OpenCLPathTracingTestKernel.cl";
		std::string contents;
		FileUtils::readEntireFileTextMode(kernel_path, contents);
		std::vector<std::string> program_lines = ::split(contents, '\n');
		for(size_t i=0; i<program_lines.size(); ++i)
			program_lines[i].push_back('\n'); // Make each line have a newline char at end.
		

		std::string options =	std::string(" -cl-fast-relaxed-math") +
								std::string(" -cl-mad-enable") + 
								std::string(" -I \"") + TestUtils::getIndigoTestReposDir() + "/opencl/\"";

		// Compile and build program.
		StandardPrintOutput print_output;
		cl_program program = opencl->buildProgram(
			program_lines,
			context,
			opencl_device.opencl_device,
			options,
			print_output
		);

		conPrint("Program built.");

		opencl->dumpBuildLog(program, opencl_device.opencl_device, print_output); 

		OpenCLKernelRef testKernel = new OpenCLKernel(program, "testKernel", opencl_device.opencl_device);


		//============== Test-specific buffers ====================
		std::vector<OCLPTTexDescriptor> tex_descriptors(1);

		tex_descriptors[0].xres			= 4;
		tex_descriptors[0].yres			= 4;
		tex_descriptors[0].bpp			= 3;
		tex_descriptors[0].byte_offset	= 0;
		tex_descriptors[0].inv_gamma	= 1.f;
		tex_descriptors[0].quadratic	= 0;
		tex_descriptors[0].scale		= 1;
		tex_descriptors[0].bias			= 0;

		//tex_descriptors[0].texmatrix_rotscale[0] = scene_builder->used_textures[i].first.texmatrix_rotscale[0];
		//tex_descriptors[0].texmatrix_rotscale[1] = scene_builder->used_textures[i].first.texmatrix_rotscale[1];
		//tex_descriptors[0].texmatrix_offset		 = scene_builder->used_textures[i].first.texmatrix_offset;

		OpenCLBuffer cl_texture_desc;
		cl_texture_desc.allocFrom(context, tex_descriptors, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);


		std::vector<uint8> texture_data(48);

		// Make a texture with RGB values steadily increasing as x increases.
		// so val(0, y) = 0, val(1, y) = 0.25*256 = 64, val(2, y) = 128, val(3, y) = 192
		const int W = 4;
		for(int y=0; y<W; ++y)
		for(int x=0; x<W; ++x)
		{
			texture_data[(y*W + x)*3 + 0] = (uint8)(256 * (float)x / W);
			texture_data[(y*W + x)*3 + 1] = (uint8)(256 * (float)x / W);
			texture_data[(y*W + x)*3 + 2] = (uint8)(256 * (float)x / W);
		}


		OpenCLBuffer cl_texture_data;
		cl_texture_data.allocFrom(context, texture_data, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

		//============== End test-specific buffers ====================



		OpenCLBuffer result_buffer(context, sizeof(int32), CL_MEM_READ_WRITE);

		// Launch the kernel:
		testKernel->setNextKernelArg(result_buffer.getDevicePtr());
		testKernel->setNextKernelArg(cl_texture_desc.getDevicePtr());
		testKernel->setNextKernelArg(cl_texture_data.getDevicePtr());

		testKernel->launchKernel(command_queue, 1); 

		// Read back result
		SSE_ALIGN int32 test_result;
		cl_int result = opencl->clEnqueueReadBuffer(
			command_queue,
			result_buffer.getDevicePtr(), // buffer
			CL_TRUE, // blocking read
			0, // offset
			sizeof(int32), // size in bytes
			&test_result, // host buffer pointer
			0, // num events in wait list
			NULL, // wait list
			NULL // event
			);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));


		if(test_result != 0)
		{
			conPrint("Error: An OpenCL unit test failed.");
			exit(10);
		}

		// Free the context and command queue for this device.
		opencl->deviceFree(context, command_queue);
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		failTest(e.what());
	}
}


void OpenCLTests::test()
{
	conPrint("OpenCLTests::test()");

	try
	{
		OpenCL* opencl = getGlobalOpenCL();

		testAssert(opencl != NULL);

		opencl->queryDevices();

		for(size_t i=0; i<opencl->getDeviceInfo().size(); ++i)
		{
			const gpuDeviceInfo& device_info = opencl->getDeviceInfo()[i];
			if(device_info.CPU) // If this is a CPU device:
			{
				runTestsOnDevice(device_info);
			}
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#else // USE_OPENCL


OpenCLTests::OpenCLTests()
{
}


OpenCLTests::~OpenCLTests()
{
}


void OpenCLTests::test()
{
	conPrint("OpenCLTests::test(): Skipping as USE_OPENCL is not defined.");
}


#endif // USE_OPENCL


#endif // BUILD_TESTS
