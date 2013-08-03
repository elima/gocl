__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel void gaussian_blur( __read_only image2d_t image,
                             __write_only image2d_t blurred_image,
                             __constant float *mask,
                             __private int maskSize)
{
  const int2 pos = {get_global_id(0), get_global_id(1)};

  // Collect neighbor values and multiply with Gaussian
  float3 sum = (0.0f, 0.0f, 0.0f);

  for(int a = -maskSize; a < maskSize+1; a++) {
      for(int b = -maskSize; b < maskSize+1; b++) {
          float4 pixel = read_imagef(image, sampler, pos + (int2)(a,b));
          float factor = mask[a+maskSize+(b+maskSize)*(maskSize*2+1)];

          sum += ((float3)(pixel.x,pixel.y,pixel.z)) * factor;
     }
  }

  float4 pixel = read_imagef(image, sampler, pos);

  write_imagef(blurred_image, pos, (float4)(sum.x, sum.y, sum.z, pixel.w));
}
