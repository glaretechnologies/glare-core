/*=====================================================================
Task.cpp
--------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "Task.h"


namespace glare
{


Task::Task()
:	task_group(NULL), allocator(NULL), in_queue(false), is_quit_runner_task(false)
{}


Task::~Task()
{
}


} // end namespace glare 
