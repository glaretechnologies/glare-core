/*=====================================================================
GLMemUsage.h
------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <stdlib.h>


struct GLMemUsage
{
	GLMemUsage() : geom_cpu_usage(0), geom_gpu_usage(0), texture_cpu_usage(0), texture_gpu_usage(0), sum_unused_tex_gpu_usage(0) {}

	size_t geom_cpu_usage;
	size_t geom_gpu_usage;

	size_t texture_cpu_usage;
	size_t texture_gpu_usage;
	size_t sum_unused_tex_gpu_usage;

	size_t totalCPUUsage() const;
	size_t totalGPUUsage() const;

	void operator += (const GLMemUsage& other);
};
