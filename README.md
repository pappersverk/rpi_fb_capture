# RpiFbCapture

[![CircleCI](https://circleci.com/gh/fhunleth/rpi_fb_capture.svg?style=svg)](https://circleci.com/gh/fhunleth/rpi_fb_capture)
[![Hex version](https://img.shields.io/hexpm/v/rpi_fb_capture.svg "Hex version")](https://hex.pm/packages/rpi_fb_capture)

This library can capture the Raspberry Pi's framebuffer. This lets you do things
like:

* Use Scenic on I2C and SPI-connected LCD and OLED displays
* Take screenshots
* Implement remote device control in Elixir

The capture occurs in a separate port process which can be supervised like other
Elixir processes.

The native Raspberry Pi framebuffer is captured in 16-bit RGB. That raw buffer
can be reported or any of these formats:

* [PPM](https://en.wikipedia.org/wiki/Netpbm_format) format - suitable for
  writing to a file and viewing
* Raw 24-bit RGB
* Raw 1-bpp
* Raw 1-bbp scanned vertically - useful for some LCDs

## Installation

The package can be installed by adding `rpi_fb_capture` to your list of
dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:rpi_fb_capture, "~> 0.1.0"}
  ]
end
```

## Example use

To try it out, you'll need a Raspberry Pi. It doesn't need to be attached to a
display.

```elixir
# Specify width and height if you're only interested in part of the display.
# Specify 0's or leave missing to capture everything.
iex> {:ok, cap} = RpiFbCapture.start_link(width: 0, height: 0)
{:ok, #PID<0.4794.0>}

# Perform the capture - choose ppm since it's easy to view
iex> {:ok, frame} = RpiFbCapture.capture(cap, :ppm)
{:ok,
 %RpiFbCapture.Capture{
   data: [
     "P6 720 480 255\n",
     <<0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ...>>
   ],
   format: :ppm,
   height: 480,
   width: 720
 }}

# Save the capture to a file
iex> File.write("/tmp/capture.ppm", frame.data)
:ok
```

If you're using Nerves, use sftp to copy the file off the device and view or if
on Raspbian, view it locally.
