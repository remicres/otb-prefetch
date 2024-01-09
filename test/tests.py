from planetary_computer import sign_inplace
from pystac_client import Client
import pyotb
from functools import partial


api = Client.open(
   "https://planetarycomputer.microsoft.com/api/stac/v1",
   modifier=sign_inplace,
)

res = api.search(
  bbox=[4, 42.99, 5, 44.05],
  datetime=["2023-01-01", "2023-01-25"],
  collections=["sentinel-2-l2a"]
)

item = next(res.items())
b4_href = item.assets["B04"].href

apps = [
  partial(pyotb.Smoothing, type="mean", type_mean_radius=32),
#  partial(pyotb.Smoothing, type="gaussian"),
#  partial(pyotb.BandMathX, exp="im1"),
#  partial(pyotb.Mosaic),
#  partial(pyotb.MeanShiftSmoothing)
]

def compare():
  """
  Compare that the application returns the same result 
  for prefetched and vanilla inputs.
  """
  for app in apps:
    pyotb.CompareImages(
      ref_in=app(b4_href),
      meas_in=app(pyotb.Prefetch(b4_href))
    )

def strategies():
  """
  Testing that all applications runs fine for prefetched inputs
  for tiled and stripped streaming strategies.
  """
  ext_fname = {
    "streaming:sizemode": "height",
    "streaming:sizevalue": 256
  }
  for app in apps:
    for strategy in ["tiled", "stripped"]:
      out = "/tmp/out.tif"
      ext_fname["streaming:type"] = strategy
      app(pyotb.Prefetch(b4_href)).write(out, ext_fname=ext_fname)
 
# compare()
strategies()
