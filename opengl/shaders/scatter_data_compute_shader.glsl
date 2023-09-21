
struct DataUpdateStruct
{
	PerObjectVertUniformsStruct data;

	uint write_index; // location to write data to

	uint padding_0;
	uint padding_1;
	uint padding_2;
};



layout(std430, binding=0) buffer DataUpdate
{
	DataUpdateStruct data_updates[];
};


layout(std430, binding=1) buffer PerObjectVertUniforms
{
	PerObjectVertUniformsStruct per_object_data[];
};


layout(local_size_x = /*WORK_GROUP_SIZE*/1, local_size_y = 1, local_size_z = 1) in;

void main()
{
	const uint i = gl_GlobalInvocationID.x;

	per_object_data[data_updates[i].write_index] = data_updates[i].data;
}
