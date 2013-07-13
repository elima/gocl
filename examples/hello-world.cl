/* A simple OpenCL program */
__kernel void my_kernel (__global char *data, const int size) {
  int2 lid = {get_local_id (0), get_local_id(1)};
  int2 pos = {get_global_id (0), get_global_id(1)};

  data[pos.y * get_global_size(0) + pos.x] = (lid.y << 4) + lid.x;
}
