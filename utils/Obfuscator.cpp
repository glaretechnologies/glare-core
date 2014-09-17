/*=====================================================================
Obfuscator.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue May 25 18:32:39 +1200 2010
=====================================================================*/
#include "Obfuscator.h"


#include "Parser.h"
#include <string>
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Exception.h"
#include "../utils/FileUtils.h"
#include <fstream>
#include "Transmungify.h"
#include <stdlib.h>


Obfuscator::Obfuscator(bool collapse_whitespace_, bool remove_comments_, bool change_tokens_, bool cryptic_tokens_)
:	collapse_whitespace(collapse_whitespace_),
	remove_comments(remove_comments_),
	change_tokens(change_tokens_),
	cryptic_tokens(cryptic_tokens_),
	rng(1)
{
	c = 0;

	const char* keywords_[] = {
"auto",
"bool",
"break",
"case",
"char",
"const",
"continue",
"default",
"do",
"double",
"else",
"enum",
"extern",
"false",
"float",
"for",
"goto",
"if",
"int",
"long",
"register",
"return",
"short",
"signed",
"sizeof",
"static",
"struct",
"switch",
"true",
"typedef",
"union",
"unsigned",
"void",
"volatile",
"restrict",
"while",

// CUDA and OpenCL keywords

"as_int",
"float2",
"float3",
"float4",
"uint",
"uint4",
"texture",
"__device__",
"__constant__",
"__global__",
"__shared__",
"tex1Dfetch",
"__float_as_int",

"abs",
"acos",
"asin",
"atan2",
"cos",
"exp",
"fabs",
"pow",
"rsqrt",
"sin",
"tan",
"sqrt",

"min",
"max",
"floor",
"ceil",
"clamp",

"printf",

// float4 elements:
"x",
"y",
"z",
"w",

// swizzles:
"xyz",

// OpenCL: see '6.1.9 Keywords' in OpenCL 1.1 spec.
"__global",
"global",
"__local",
"local",
"__constant",
"constant",
"__private",
"private",

"__kernel", 
"kernel",

"__read_only", "read_only", "__write_only", "write_only", 
"__read_write","read_write", 
 

"sampler_t",
"image2d_t",
"read_imagef",
"int2",
"get_local_id",
"get_global_id",
"inline",
"atom_add",
"fmin",
"fmax",
"as_uint4",
"as_float",
"as_uint",

"dot",
"cross",
"length",
"normalize",

"CLK_NORMALIZED_COORDS_FALSE", "CLK_ADDRESS_CLAMP_TO_EDGE", "CLK_FILTER_NEAREST",

// preprocessor definitions: need to be unchanged.

// OpenCL path tracer defs:

// XXX TODO this should be renamed to something cryptic
"ENVMAP_SPHERE", "IMAGE_XRES", "IMAGE_YRES", "BUCKET_SIZE",
"USE_FLOAT3_SWIZZLES",
"RANDOMISE_SAMPLES",
"NUM_LIGHTS",

"OPENCL_BACKGROUND_SPHERE", "OBS",
"OPENCL_RAY_BLOCK_HEIGHT", "ORBH",

// single level defs:
"OPENCL_MAX_TEX_XRES", "OMTX",
"MESH_STACK_SIZE", "MSS",
"TERMINATOR_NODE", "TN",

// 2 level defs:
"OBJECT_STACK_SIZE", "OSS",
"OPENCL_MOTION_BLUR", "OMB",
"MESH_STACK_SIZE", "MSS",
"OPENCL_SKIP_LIST", "OSL",

// Externally accesible functions
"zero_kernel",
"QMC_kernel",
"RayTracingKernel",
"RayTracingKernelSkip",
"RTK",
"RTKS",


NULL
	};

	for(int i=0; keywords_[i]; i++)
	{
		//conPrint(keywords_[i]);
		this->keywords.insert(keywords_[i]);
	}
}


Obfuscator::~Obfuscator()
{

}


const std::string Obfuscator::mapToken(const std::string& t)
{
	if(!change_tokens)
		return t;

	if(this->keywords.find(t) != this->keywords.end())
		return t;

	if(token_map.find(t) == token_map.end())
	{
		std::string new_token;
		if(cryptic_tokens)
		{
			while(1)
			{
				new_token = "l";
				
				for(int i=0; i<10; ++i)
				{
					if(rng.unitRandom() < 0.5f)
						::concatWithChar(new_token, '1');
					else
						::concatWithChar(new_token, 'l');
				}
				if(new_tokens.find(new_token) == new_tokens.end()) // If we haven't already randomly generated this token...
				{
					new_tokens.insert(new_token);
					break;
				}
			}
		}
		else
		{
			new_token = "token_" + ::toString(c);
			c++;
		}

		token_map[t] = new_token;
	}
	else
	{
		
	}

	return token_map[t];
}

const std::string Obfuscator::obfuscate(const std::string& s)
{
	Parser p(s.c_str(), (unsigned int)s.length());

	std::string res;


	const std::string ignore_tokens = "f[](){}<>/*-+=;,.^&!|?:%";


	while(p.notEOF())
	{
		std::string t;

		//const char debug_current_char = p.current();
		//conPrint(std::string(1, debug_current_char));

		//if((p.current() == 'x' && p.nextIsChar('<')) ||
		//	(p.current() == '>' && p.nextIsChar('>')))
		//{
		//	conPrint("found 'x  ='");
		//}

		if(p.current() == '/' && p.nextIsChar('/'))
		{
			int last_currentpos = p.currentPos();
			p.parseLine();
			if(!remove_comments)
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos); // append comment to output
			continue;
		}
		else if(p.current() == '#') // preprocessor def
		{
			int last_currentpos = p.currentPos();

			if(s.substr(last_currentpos, 7) == "#define")
			{
				// skip past the "#define " string to token
				for(int z = 0; z < 8; ++z)
					p.advance();

				// Parse the token
				bool valid_token_parse = p.parseIdentifier(t);
				if(valid_token_parse)
					conPrint("found #define " + t);

				// Make sure the output has a newline at the end (defines have to be on their own line I think)
				if(res.substr(res.size() - 1, 1) != "\n")
					res += "\r\n";

				// Output the (modified) #define
				res += "#define " + mapToken(t);

				// Read in the the definition of the define:
				int last_currentpos = p.currentPos();
				p.advancePastLine(); // Go past line with #define in it.

				// Output the rest of the define line
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
				continue;
			}

			p.advancePastLine();
			if(res.substr(res.size() - 1, 1) != "\n")
				res += "\r\n";
			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(p.current() == '"') // string
		{
			int last_currentpos = p.currentPos();
			p.parseChar('"');

			while(p.notEOF() && p.current() != '"')
			{
				p.advance();
			}

			p.parseChar('"');

			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);

			continue;
		}
		else if(p.current() == '/' && p.nextIsChar('*'))
		{
			int last_currentpos = p.currentPos();
			p.parseChar('/');
			p.parseChar('*');

			while(p.notEOF() && !(p.current() == '*' && p.nextIsChar('/')))
			{
				p.advance();
			}
			p.parseChar('*');
			p.parseChar('/');
			if(!remove_comments)
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos); // append comment to output
			continue;
		}
		else if(::isWhitespace(p.current())) // Note that if we are removing whitespace and not removing comments, then newlines at end of comments can give incorrect results
		{
			int last_currentpos = p.currentPos();
			p.parseWhiteSpace();
			if(collapse_whitespace)
				res += " ";
			else
				res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(p.current() == '0' && p.next() == 'x') // Hex number
		{
			const int num_hex_chars = 10 + 6 + 1; // 0-9, a-e and x
			const char hex_chars[num_hex_chars] =
			{
				'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
				'a', 'b', 'c', 'd', 'e', 'f',
				'x'
			};

			int last_currentpos = p.currentPos();

			while(p.notEOF())
			{
				const char current_char = toLowerCase(p.current());

				bool found_hex_char = false;
				for(int z = 0; z < num_hex_chars; ++z)
					if(current_char == hex_chars[z])
					{
						found_hex_char = true;
						break;
					}

				if(!found_hex_char) break;
				else p.advance();
			}

			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(p.parseIdentifier(t))
		{
			res += mapToken(t);
			continue;
		}
		else if(p.fractionalNumberNext())
		{
			int last_currentpos = p.currentPos();
			double val;
			p.parseDouble(val);
			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}
		else if(::isNumeric(p.current()))
		{
			int last_currentpos = p.currentPos();

			int val;
			const bool valid_int_parse = p.parseInt(val);
			if(!valid_int_parse)
			{
				// Try parsing as an unsigned int
				uint32 uint_val;
				const bool valid_uint_parse = p.parseUnsignedInt(uint_val);

				// Fail, might need to try 64bit integer
				if(!valid_uint_parse)
					throw Indigo::Exception("Int parse failed at start of '" + s.substr(p.currentPos(), 10) + "'");
			}

			res += s.substr(last_currentpos, p.currentPos() - last_currentpos);
			continue;
		}

		/*if(p.parseInt())
		{
			int last_currentpos = p.currentPos();
			p.parseDouble();
			res += s.substr(last_current, p.currentPos() - last_currentpos);
		}*/

		{
			bool found = false;
			for(size_t i=0; i<ignore_tokens.size() && !found; ++i)
			{
				if(p.parseChar(ignore_tokens[i]))
				{
					res += ignore_tokens[i];
					found = true;
				}
			}
			if(found)
				continue;
		}

		//if(!processed)
		//{
			throw Indigo::Exception("Unhandled character at start of '" + s.substr(p.currentPos(), 10) + "'");
		//}
	}

	return res;
}


void Obfuscator::obfuscateKernels(const std::string& kernel_dir)
{
	//std::string header;

	const bool transmungify_kernels = true;

	try
	{
		// Single level kernel
		{
			std::string s;
			FileUtils::readEntireFile(FileUtils::join(kernel_dir, "OpenCLSingleLevelRayTracingKernel.cl"), s);

			Obfuscator ob(
				true, // collapse_whitespace
				true, // remove_comments
				true, // change tokens
				true // cryptic_tokens
				);


			// Obfuscate the code
			const std::string ob_s = ob.obfuscate(s);

			// Add to header
			/*header += "const int OpenCLSingleLevelRayTracingKernel_size = " + toString(ob_s.size()) + ";\n";
			header += "const unsigned int OpenCLSingleLevelRayTracingKernel[" + toString(dwords.size()) + "] = {\n";
			for(int i=0; i<dwords.size(); ++i)
			{
				header += "0x" + ::toHexString(dwords[i]) + ", ";
				if((i > 0) && (i%10 == 0))
					header += "\n";
			}
			header += "\n};\n";*/


			const std::string outpath = "data/OSL"; // "OpenCLSingleLevelRayTracingKernel_obfuscated.cl";

			if(!transmungify_kernels)
			{
				FileUtils::writeEntireFile(outpath, ob_s);
			}
			else
			{
				// Transmungify
				std::vector<unsigned int> dwords;
				Transmungify::encrypt(ob_s, dwords);

				FileUtils::writeEntireFile(outpath, (const char*)&dwords[0], dwords.size() * sizeof(unsigned int));
			}

			conPrint("Written '" + outpath + "'");


			/*{
			const std::string outpath = "data/OSL";
			FileUtils::writeEntireFile(outpath, ob_s);
			conPrint("Written '" + outpath + "'");
			}*/
			
			//{
			//	// Write to dist dir.  Note: this kinda sux.
			//	const std::string outpath = PlatformUtils::getEnvironmentVariable("INDIGO_TEST_REPOS_DIR") + "dist/shared/data/OSL";
			//	FileUtils::writeEntireFile(outpath, ob_s);
			//	conPrint("Written '" + outpath + "'");
			//}
		}

		// Two level kernel
		{
			std::string s;
			FileUtils::readEntireFile(FileUtils::join(kernel_dir, "OpenCLRayTracingKernel.cl"), s);

			Obfuscator ob(
				true, // collapse_whitespace
				true, // remove_comments
				true, // change tokens
				true // cryptic_tokens
				);


			// Obfuscate the code
			const std::string ob_s = ob.obfuscate(s);

			// Transmungify
			std::vector<unsigned int> dwords;
			Transmungify::encrypt(ob_s, dwords);

			// Add to header
			/*header += "\n\n";
			header += "const int OpenCLRayTracingKernel_size = " + toString(ob_s.size()) + ";\n";
			header += "const unsigned int OpenCLRayTracingKernel[" + toString(dwords.size()) + "] = {\n";
			for(int i=0; i<dwords.size(); ++i)
			{
				header += "0x" + ::toHexString(dwords[i]) + ", ";
				if((i > 0) && (i%10 == 0))
					header += "\n";
			}
			header += "\n};\n";*/

			const std::string outpath = "data/ODL"; // "OpenCLRayTracingKernel_obfuscated.cl";

			if(!transmungify_kernels)
			{
				FileUtils::writeEntireFile(outpath, ob_s);
			}
			else
			{
				// Transmungify
				std::vector<unsigned int> dwords;
				Transmungify::encrypt(ob_s, dwords);

				FileUtils::writeEntireFile(outpath, (const char*)&dwords[0], dwords.size() * sizeof(unsigned int));
			}

			conPrint("Written '" + outpath + "'");	

			/*{
			const std::string outpath = "data/ODL";
			FileUtils::writeEntireFile(outpath, ob_s);
			conPrint("Written '" + outpath + "'");
			}*/
		}

		// OpenCL path tracer kernels
		{
			const int num_files = 8;
			std::string files[num_files] =
			{
				// Include files. Note that the order here matters, because we can't use #include statements and #pragma once etc
				"OpenCLPathTracingKernel.h",
				"ptkernel_spectral.h",
				"ptkernel_bvh.h",
				"ptkernel_maths.h",
				"ptkernel_samplingutils.h",
				"ptkernel_textures.h",
				"ptkernel_materials.h",

				// Main kernel file
				"OpenCLPathTracingKernel.cl"
			};

			std::string complete_kernel_string;
			for(int z = 0; z < num_files; ++z)
			{
				std::string s;
				FileUtils::readEntireFile(FileUtils::join(kernel_dir, files[z]), s);

				complete_kernel_string += s + "\n";
			}

			Obfuscator ob(
				false, // collapse_whitespace
				false, // remove_comments
				true, // change tokens
				false // cryptic_tokens
				);

			// Obfuscate the code
			const std::string ob_s = ob.obfuscate(complete_kernel_string);

			const std::string outpath = std::string("data/OPT");

			FileUtils::writeEntireFile(outpath, ob_s);

			conPrint("Wrote '" + outpath + "'");	
		}
	}
	catch(Indigo::Exception& e)
	{
		conPrint("Error: " + e.what());
		exit(1);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		conPrint("Error: " + e.what());
		exit(1);
	}
}


#if BUILD_TESTS


void Obfuscator::test()
{
	//const std::string s = "int /*a = b[3] + */a;";
	const std::string s = "x  = (x ^ 12345391) * 2654435769;";

	//std::string s;
	//FileUtils::readEntireFile("c:\\programming\\indigo\\trunk\\opencl\\OpenCLSingleLevelRayTracingKernel.cl", s);

	
	Obfuscator ob(
		true, // collapse_whitespace
		true, // remove_comments
		true, // change tokens
		true // cryptic_tokens
	);

	try
	{
		const std::string ob_s = ob.obfuscate(s);

		conPrint(ob_s);

		{
			//std::ofstream f("munged.cpp");
			//f << ob_s;
		//	const std::string outpath = "OpenCLSingleLevelRayTracingKernel_obfuscated.cl";
		//	FileUtils::writeEntireFile(outpath, ob_s);
		//	conPrint("Written '" + outpath + "'");
		}
	}
	catch(Indigo::Exception& e)
	{
		conPrint("Error: " + e.what());
		exit(1);
	}
}


#endif
