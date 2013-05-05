var Mainloop = imports.mainloop;
var Gocl = imports.gi.Gocl;

const SIZE = 1920 * 1080;

var source =
  "" +
    "__kernel void my_kernel (__global char *data, const int size) {" +
  "  int tid = get_global_id (0);" +
  "  int local_work_size = get_local_size (0);" +
  "" +
  "  for (int i=tid * local_work_size; i<tid * local_work_size + local_work_size; i++) {" +
  "    if (i < size)" +
  "      data[i] += 1;" +
  "  }" +
  "}";

// get a context
var context;
try {
    context = new Gocl.Context.get_default_gpu ();
}
catch (e) {
    context = new Gocl.Context.get_default_cpu ();
}
print ("Context created");
print ("Num devices: " + context.get_num_devices ());

// get first device in context
var device = context.get_device_by_index (0);

// create a program
var program = Gocl.Program.new (context, [source], -1);
print ("Program created");

// build program
program.build ("", null,
               function (prog, result) {
                   try {
                       prog.build_finish (result);
                   }
                   catch (e) {
                       print ("ERROR: " + e.message);
                       Mainloop.quit ("main");
                       return;
                   }

                   print ("Program built");
                   onProgramBuilt ();
               });

function onProgramBuilt () {
    // get a kernel
    var kernel = program.get_kernel ("my_kernel");
    print ("Kernel created");

    // calculate workgroup sizes
    var maxWorkgroupSize = device.get_max_work_group_size ();
    print ("Max work group size: " + maxWorkgroupSize);

    var size = SIZE;
    var localWorksize = 10; //MIN (size, max_workgroup_size);
    var globalWorksize = (size % localWorksize == 0) ?
        size :
        (size / localWorksize + 1) * localWorksize;

    print ("Global work size: ", globalWorksize);
    print ("Local work size: " + localWorksize);

    // create a buffer
    var buffer = context.create_buffer (Gocl.BufferFlags.READ_WRITE,
                                        size,
                                        null);
    print ("Buffer created");

    // set kernel arguments
    kernel.set_argument_buffer (0, buffer);
    kernel.set_argument_int32 (1, [size]);

    // run the kernel asynchronously
    print ("Kernel execution starts");
    var event = kernel.run_in_device (device,
                                      globalWorksize,
                                      localWorksize,
                                      []);

    event.then (function (event, error, userData) {
                    if (error != null)
                        throw (error.message);
                    else {
                        print ("Kernel execution finished");
                        Mainloop.quit ("main");
                    }
                },
                null);

}

Mainloop.run ("main");
