# OTB prefetch

This remote module of the [Orfeo ToolBox](https://www.orfeo-toolbox.org/) provides a new 
application to prefetch a pipeline's input in an asynchronous way.

Under the hood, the `otb::PrefetchCacheAsyncFilter` does the work. It tries to guess the 
next requested region, and request it to the upstream pipeline, while the downstream pipeline 
runs on its own.

## Example

In a deep learning application, at inference time, avoiding the extra cost of the GPU idle while GDAL is 
just grabbing the next chunk of image (example with [pyotb](https://github.com/orfeotoolbox/pyotb)):

```python
import pyotb

img_href = "https://.../input.tif"
cal = pyotb.OpticalCalibration(img_href)
prefetch = pyotb.Prefetch(cal)
infer = pyotb.TensorflowModelServe(prefetch, ...)
infer.write("output.tif")
```

After the execution, the filter display some performance metrics:

```commandLine
PrefetchCacheAsyncFilter (0x563812708470): 7.0272e+06 missing guessed pixels (4.68247 %)
PrefetchCacheAsyncFilter (0x563812708470): 1.43047e+08 good guessed pixels (95.3175 %)
PrefetchCacheAsyncFilter (0x563812708470): 351360 extra guessed pixels (0.234123 %)
PrefetchCacheAsyncFilter (0x563812708470): Total wait: 0.000963238s
```

We can see that less that 1ms has bee spent between the GPU calls, saving us some money! 
(for information the input is a Sentinel-2 file in COG format from Microsoft Planetary Computer, 
processed with a `streaming:type=tiled` strategy on the OTB writer). If you want to know 
more about OTB/GDAL writing strategies, [this](https://wiki.orfeo-toolbox.org/index.php/Writing_large_images) 
is a good read.


## Build from source

You can build the remote module
Give a try with one [otbtf development docker image](https://hub.docker.com/layers/mdl4eo/otbtf/4.3.0-cpu-dev/images/sha256-8772aac279ae0c2bb390501aa14c490d481e703273bb5c72a49701233f342909?context=explore):

```commandLine
cd /src/otb/otb/Modules/Remote
git clone https://github.com/remicres/prefetch
cd /src/otb/build/OTB/build
cmake /src/otb/otb -DModule_OTBPrefetch=ON
make install
```

You can then check that the OTB application has been installed:

```commandLine
otbcli_Prefetch --help
```

# License

Apache 2.0

# Contact

Remi Cresson at INRAE

# Acknowledgements

- Bradley Lowekamp
- OTB Dev team 
