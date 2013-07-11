/* A simple OpenCL program */
__kernel void my_kernel (__global char *data, const int size) {
  int2 lid = {get_local_id (0), get_local_id(1)};
  int2 global_work_size = { get_global_size(0), get_global_size(1) };
  int2 local_work_size = { get_local_size(0), get_local_size(1) };
  local_work_size = (global_work_size) / (local_work_size);

  for (int i = 0; i < local_work_size.x; i++) {
    for (int j = 0; j < local_work_size.y; j++) {
      int x = i + lid.x * local_work_size.x;
      int y = j + lid.y * local_work_size.y;
      if (x < get_global_size(0) && y < get_global_size(1))
        data[y * get_global_size(0) + x] = (lid.y << 4) + lid.x;
    }
  }
}
