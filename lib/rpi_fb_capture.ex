defmodule RpiFbCapture do
  use GenServer

  @moduledoc """
  Capture the Raspberry Pi's frame buffer
  """

  @type option ::
          {:width, non_neg_integer()}
          | {:height, non_neg_integer()}
          | {:display, non_neg_integer()}
  @type format :: :ppm | :rgb24 | :rgb565 | :mono | :mono_column_scan
  @type dithering :: :none | :floyd_steinberg

  defmodule State do
    @moduledoc false
    defstruct port: nil,
              width: 0,
              height: 0,
              display_width: 0,
              display_height: 0,
              display_id: 0,
              backend_name: "unknown",
              request: nil
  end

  @doc """
  Start up the capture process

  NOTE: The Raspberry Pi capture hardware has limitations on the window
  size. In general, capturing the whole display is fine. Keeping the width
  as a multiple of 16 appears to be good.

  Options:

  * `:width` - the width of the capture window (0 for the display width)
  * `:height` - the height of the capture window (0 for the display width)
  * `:display` - which display to capture (defaults to 0)
  """
  @spec start_link([option()]) :: :ignore | {:error, any()} | {:ok, pid()}
  def start_link(args \\ []) when is_list(args) do
    GenServer.start_link(__MODULE__, args)
  end

  @doc """
  Stop the capture process
  """
  @spec stop(GenServer.server()) :: :ok
  def stop(server) do
    GenServer.stop(server)
  end

  @doc """
  Capture the screen in the specified format.

  Formats include:

  * `:ppm` - PPM-formatted data
  * `:rgb24` - Raw 24-bit RGB data 8-bits R, G, then B
  * `:rgb565` - Raw 16-bit data 5-bits R, 6-bits G, 5-bits B
  * `:mono` - Raw 1-bpp data
  * `:mono_column_scan` - Raw 1-bpp data, but scanned down columns
  """
  @spec capture(GenServer.server(), format()) ::
          {:ok, RpiFbCapture.Capture.t()} | {:error, atom()}
  def capture(server, format) do
    GenServer.call(server, {:capture, format})
  end

  @doc """
  Adjust the value that pixels are on for monochromatic conversion.

  The threshold should be 8-bits. The capture buffer is rgb565, so the
  threshold will be reduced to 5 or 6 bits for the actual comparisons.
  """
  @spec set_mono_threshold(GenServer.server(), byte()) :: :ok | {:error, atom()}
  def set_mono_threshold(server, threshold) do
    GenServer.call(server, {:mono_threshold, threshold})
  end

  @doc """
  Set dithering algorithm.

  Algorithms include:

  * `:none` - No dithering s applied
  * `:floyd_steinberg` - Floyd–Steinberg
  """
  @spec set_dithering(GenServer.server(), dithering()) :: :ok | {:error, atom()}
  def set_dithering(server, algorithm) do
    GenServer.call(server, {:dithering, algorithm})
  end

  @doc """
  Helper method for saving a screen capture to a file

  Example:

  ```elixir
  iex> {:ok, cap} = RpiFbCapture.start_link()
  iex> RpiRbCapture(cap, "/tmp/capture.ppm")
  :ok
  ```
  """
  @spec save(GenServer.server(), Path.t(), format()) :: :ok | {:error, atom()}
  def save(server, path, format \\ :ppm) do
    with {:ok, cap} = capture(server, format) do
      File.write(path, cap.data)
    end
  end

  @doc """
  Return the name of the active capture backend

  Example backend names:

  * `"sim"` for the simulator
  * `"dispmanx"` for the Raspberry Pi
  * `"unknown"` if the backend hasn't reported its name
  """
  @spec backend(GenServer.server()) :: String.t()
  def backend(server) do
    GenServer.call(server, :backend)
  end

  # Server (callbacks)

  @impl true
  def init(args) do
    executable = Application.app_dir(:rpi_fb_capture, ["priv", "rpi_fb_capture"])
    width = Keyword.get(args, :width, 0)
    height = Keyword.get(args, :height, 0)
    display = Keyword.get(args, :display, 0)

    port =
      Port.open({:spawn_executable, to_charlist(executable)}, [
        {:args, [to_string(display), to_string(width), to_string(height)]},
        {:packet, 4},
        :use_stdio,
        :binary,
        :exit_status
      ])

    state = %State{port: port, width: width, height: height}
    {:ok, state}
  end

  @impl true
  def handle_call({:capture, format}, from, state) do
    case state.request do
      nil ->
        new_state = start_capture(state, from, format)
        {:noreply, new_state}

      _outstanding_request ->
        {:reply, {:error, :only_one_capture_at_a_time}, state}
    end
  end

  @impl true
  def handle_call({:mono_threshold, threshold}, _from, state) do
    Port.command(state.port, port_cmd(:mono_threshold, threshold))
    {:reply, :ok, state}
  end

  @impl true
  def handle_call(:backend, _from, state) do
    {:reply, state.backend_name, state}
  end

  @impl true
  def handle_call({:dithering, algorithm}, _from, state) do
    Port.command(state.port, port_cmd(:dithering, algorithm))
    {:reply, :ok, state}
  end

  @impl true
  def handle_info({port, {:data, data}}, %{port: port} = state) do
    handle_port(state, data)
  end

  @impl true
  def handle_info({port, {:exit_status, _status}}, %{port: port} = state) do
    if state.request do
      {from, _format} = state.request
      GenServer.reply(from, {:error, :port_crashed})
    end

    {:stop, :port_crashed}
  end

  defp handle_port(
         state,
         <<backend_name::16-bytes, display_id::native-32, display_width::native-32,
           display_height::native-32, capture_width::native-32, capture_height::native-32>>
       ) do
    # Capture information is 20 bytes - framebuffers are safely
    # larger, so there's no chance of an accident here.
    new_state = %{
      state
      | width: capture_width,
        height: capture_height,
        display_width: display_width,
        display_height: display_height,
        display_id: display_id,
        backend_name: trim_c_string(backend_name)
    }

    {:noreply, new_state}
  end

  defp handle_port(%{request: {from, format}} = state, data) do
    result_data = process_response(state, format, data)

    result = %RpiFbCapture.Capture{
      data: result_data,
      width: state.width,
      height: state.height,
      format: format
    }

    GenServer.reply(from, {:ok, result})
    {:noreply, %{state | request: nil}}
  end

  defp trim_c_string(string) do
    :binary.split(string, <<0>>) |> hd()
  end

  defp start_capture(state, from, format) do
    Port.command(state.port, port_cmd(:capture, format))

    %{state | request: {from, format}}
  end

  defp port_cmd(:capture, :ppm), do: <<2>>
  defp port_cmd(:capture, :rgb24), do: <<2>>
  defp port_cmd(:capture, :rgb565), do: <<3>>
  defp port_cmd(:capture, :mono), do: <<4>>
  defp port_cmd(:capture, :mono_column_scan), do: <<5>>
  defp port_cmd(:mono_threshold, value), do: <<6, value>>
  defp port_cmd(:dithering, :none), do: <<7, 0>>
  defp port_cmd(:dithering, :floyd_steinberg), do: <<7, 1>>

  defp process_response(state, :ppm, data) do
    ["P6 #{state.width} #{state.height} 255\n", data]
  end

  defp process_response(_state, _format, data), do: data
end
