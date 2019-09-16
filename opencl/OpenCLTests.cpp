/*=====================================================================
OpenCLTests.cpp
---------------
Copyright Glare Technologies Limited 2015 -
=====================================================================*/
#include "OpenCLTests.h"


#include "OpenCLKernel.h"
#include "OpenCLBuffer.h"
#include "OpenCLPathTracingKernel.h"
#include "../indigo/TestUtils.h"
#include "../opencl/OpenCLContext.h"
#include "../opencl/OpenCLCommandQueue.h"
#include "../opencl/OpenCLProgram.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/ConPrint.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Obfuscator.h"


#if BUILD_TESTS


OpenCLTests::OpenCLTests()
{
}


OpenCLTests::~OpenCLTests()
{
}


void OpenCLTests::runTestsOnDevice(const OpenCLDeviceRef& opencl_device)
{
	conPrint("\nOpenCLTests::runTestsOnDevice(), device: " + opencl_device->description());

	std::string build_log;
	try
	{
		OpenCL* opencl = getGlobalOpenCL();
		testAssert(opencl != NULL);

		// Initialise OpenCL context and command queue for this device
		OpenCLContextRef context = new OpenCLContext(opencl_device->opencl_platform_id);
		OpenCLCommandQueueRef command_queue = new OpenCLCommandQueue(context, opencl_device->opencl_device_id, /*enable profiling=*/false);


		// Read test kernel from disk
		const std::string kernel_path = TestUtils::getIndigoTestReposDir() + "/opencl/test_programs/OpenCLPathTracingTestKernel.cl";
		std::string contents = FileUtils::readEntireFileTextMode(kernel_path);

		std::string options =	std::string(" -cl-fast-relaxed-math") +
								std::string(" -cl-mad-enable") + 
								std::string(" -I \"") + TestUtils::getIndigoTestReposDir() + "/opencl/\"";

		// Compile and build program.
		std::vector<OpenCLDeviceRef> devices(1, opencl_device);		
		OpenCLProgramRef program = opencl->buildProgram(
			contents,
			context,
			devices,
			options,
			build_log
		);


		conPrint("Program built.");

		conPrint("Build log:\n" + opencl->getBuildLog(program->getProgram(), opencl_device->opencl_device_id));

		OpenCLKernelRef testKernel = new OpenCLKernel(program, "testKernel", opencl_device->opencl_device_id, /*profile=*/false);


		//============== Test-specific buffers ====================
		js::Vector<OCLPTTexDescriptor, 64> tex_descriptors(1);

		tex_descriptors[0].xres			= 4;
		tex_descriptors[0].yres			= 4;
		tex_descriptors[0].N			= 3;
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


		js::Vector<uint8, 64> texture_data(48);

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



		OpenCLBuffer result_buffer(context, sizeof(int32) * 1000000, CL_MEM_READ_WRITE);

		// Launch the kernel:
		for(int i=0; i<1; ++i)
			testKernel->setNextKernelArg(result_buffer.getDevicePtr());
		//testKernel->setNextKernelArg(cl_texture_desc.getDevicePtr());
		//testKernel->setNextKernelArg(cl_texture_data.getDevicePtr());

		// Run the kernel a few times
		const int N = 5;
		for(int i=0; i<N; ++i)
		{
			Timer timer;
			const double exec_time_s = testKernel->launchKernel(command_queue->getCommandQueue(), 1); 
			const double timer_elapsed_s = timer.elapsed();

			// Read back result
			SSE_ALIGN int32 test_result;
			cl_int result = opencl->clEnqueueReadBuffer(
				command_queue->getCommandQueue(),
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

			conPrint("Kernel profiled exec time: " + doubleToStringNDecimalPlaces(exec_time_s, 8) + " s, timer elapsed: " + doubleToStringNDecimalPlaces(timer_elapsed_s, 8) + " s");
		}
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what() + " build_log: " + build_log);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		failTest(e.what());
	}
}


#if 0
static void miscompilationTest()
{
	conPrint("miscompilationTest()");

	if(false)
	{
		Obfuscator opencl_ob(
			false, // collapse_whitespace
			true, // remove_comments
			true, // change tokens
			Obfuscator::Lang_OpenCL
		);

		const std::string obf_src = opencl_ob.obfuscateOpenCLC(FileUtils::readEntireFileTextMode(TestUtils::getIndigoTestReposDir() + "/opencl/miscompilation_test1.cl"));

		FileUtils::writeEntireFileTextMode(TestUtils::getIndigoTestReposDir() + "/opencl/miscompilation_test1_obfuscated.cl", obf_src);
	}



	try
	{
		OpenCL* opencl = getGlobalOpenCL();
		testAssert(opencl != NULL);

		const OpenCLDevice& opencl_device = opencl->getOpenCLDevices()[0];


		// Initialise OpenCL context and command queue for this device
		cl_context context;
		cl_command_queue command_queue;
		//opencl->deviceInit(opencl_device, /*enable_profiling=*/true, context, command_queue);


		// Read test kernel from disk
		const std::string kernel_path = TestUtils::getIndigoTestReposDir() + "/opencl/test_programs/miscompilation_test1_obfuscated.cl";
		const std::string contents = FileUtils::readEntireFileTextMode(kernel_path);

		std::string options = "";//"-cl-opt-disable";
		/*std::string options =	std::string(" -cl-fast-relaxed-math") +
								std::string(" -cl-mad-enable") + 
								std::string(" -I \"") + TestUtils::getIndigoTestReposDir() + "/opencl/\"";*/

		// Compile and build program.
		std::string build_log;
		cl_program program; /*= opencl->buildProgram(
			contents,
			context,
			opencl_device.opencl_device_id,
			options,
			build_log
		);*/
		assert(0);

		conPrint("Program built.");

		conPrint("Build log:\n" + opencl->getBuildLog(program, opencl_device.opencl_device_id));

		OpenCLKernelRef testKernel = new OpenCLKernel(program, "testKernel", opencl_device.opencl_device_id, /*profile=*/true);


		//============== Test-specific buffers ====================
		/*js::Vector<float, 64> input(4);

		input[0] = 0;
		input[1] = 0;
		input[2] = 1;
		input[3] = 0;

		OpenCLBuffer input_cl_buffer;
		input_cl_buffer.allocFrom(context, input, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
		*/

		js::Vector<float, 64> output(4);

		OpenCLBuffer output_cl_buffer;
		output_cl_buffer.allocFrom(context, output, CL_MEM_READ_WRITE);

		// Launch the kernel:
		//testKernel->setNextKernelArg(input_cl_buffer.getDevicePtr());
		testKernel->setNextKernelArg(output_cl_buffer.getDevicePtr());


		testKernel->launchKernel(command_queue, 1);
		
		// Read back result
		cl_int result = opencl->clEnqueueReadBuffer(
			command_queue,
			output_cl_buffer.getDevicePtr(), // buffer
			CL_TRUE, // blocking read
			0, // offset
			output.dataSizeBytes(), // size in bytes
			output.data(), // host buffer pointer
			0, // num events in wait list
			NULL, // wait list
			NULL // event
		);
		if(result != CL_SUCCESS)
			throw Indigo::Exception("clEnqueueReadBuffer failed: " + OpenCL::errorString(result));

		Vec4f res(output[0], output[1], output[2], output[3]);
		conPrint("Res: " + res.toString());
		/*if(test_result != 0)
		{
			conPrint("Error: An OpenCL unit test failed.");
			exit(10);
		}*/

		// Free the context and command queue for this device.
		//opencl->deviceFree(context, command_queue);
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
#endif


void OpenCLTests::test()
{
	conPrint("OpenCLTests::test()");

	//miscompilationTest();
	
	try
	{
		OpenCL* opencl = getGlobalOpenCL();

		testAssert(opencl != NULL);

		opencl->queryDevices();

		for(size_t i=0; i<opencl->getOpenCLDevices().size(); ++i)
		{
			const OpenCLDeviceRef& device_info = opencl->getOpenCLDevices()[i];
			if(device_info->opencl_device_type != CL_DEVICE_TYPE_CPU)
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


#endif // BUILD_TESTS
