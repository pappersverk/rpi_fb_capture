defmodule RpiFbCaptureTest do
  use ExUnit.Case

  @width 64
  @height 48

  setup do
    pid = start_supervised!({RpiFbCapture, [width: @width, height: @height]})

    # Workaround: wait for port to start
    Process.sleep(50)

    [server: pid]
  end

  test "running simulator", %{server: server} do
    # Check that we're running the simulator or everything will
    # fail. If you're on Raspbian,
    assert RpiFbCapture.backend(server) == "sim"
  end

  defp generates_expected(server, format, dither \\ :none) do
    :ok = RpiFbCapture.set_dithering(server, dither)
    {:ok, frame} = RpiFbCapture.capture(server, format)

    assert frame.format == format
    assert frame.width == @width
    assert frame.height == @height

    data = IO.iodata_to_binary(frame.data)
    expected_data = File.read!(expected_path(frame.width, frame.height, frame.format, dither))

    assert data == expected_data
  end

  defp expected_path(width, height, format, :none) do
    "test/support/mandelbrot-#{width}x#{height}.#{format}"
  end

  defp expected_path(width, height, format, dither) do
    "test/support/mandelbrot-#{width}x#{height}-#{dither}.#{format}"
  end

  test "generates expected ppm", %{server: server} do
    generates_expected(server, :ppm)
  end

  test "generates expected rgb24", %{server: server} do
    generates_expected(server, :rgb24)
  end

  test "generates expected rgb565", %{server: server} do
    generates_expected(server, :rgb565)
  end

  test "generates expected mono", %{server: server} do
    generates_expected(server, :mono)
  end

  test "generates expected mono_column_scan", %{server: server} do
    generates_expected(server, :mono_column_scan)
  end

  test "generates expected mono w/ floyd_steinberg", %{server: server} do
    generates_expected(server, :mono, :floyd_steinberg)
  end

  test "generates expected mono w/ sierra", %{server: server} do
    generates_expected(server, :mono, :sierra)
  end

  test "generates expected mono w/ sierra_2row", %{server: server} do
    generates_expected(server, :mono, :sierra_2row)
  end

  test "generates expected mono w/ sierra_lite", %{server: server} do
    generates_expected(server, :mono, :sierra_lite)
  end
end
