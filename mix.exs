defmodule RpiFbCapture.MixProject do
  use Mix.Project

  def project do
    [
      app: :rpi_fb_capture,
      version: "0.1.0",
      elixir: "~> 1.7",
      description: description(),
      package: package(),
      source_url: "https://github.com/fhunleth/rpi_fb_capture",
      compilers: [:elixir_make | Mix.compilers()],
      make_targets: ["all"],
      make_clean: ["clean"],
      docs: [extras: ["README.md"], main: "readme"],
      aliases: [format: [&format_c/1, "format"]],
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp description do
    "Capture the Raspberry Pi's framebuffer"
  end

  defp package do
    %{
      files: [
        "lib",
        "src/*.[ch]",
        "src/*.sh",
        "mix.exs",
        "README.md",
        "LICENSE",
        "Makefile"
      ],
      licenses: ["Apache-2.0"],
      links: %{"GitHub" => "https://github.com/fhunleth/rpi_fb_capture"}
    }
  end

  defp deps do
    [
      {:elixir_make, "~> 0.4", runtime: false},
      {:ex_doc, "~> 0.11", only: :dev, runtime: false},
      {:dialyxir, "1.0.0-rc.4", only: :dev, runtime: false}
    ]
  end

  defp format_c([]) do
    astyle =
      System.find_executable("astyle") ||
        Mix.raise("""
        Could not format C code since astyle is not available.
        """)

    System.cmd(astyle, ["-n", "src/*.c"], into: IO.stream(:stdio, :line))
  end

  defp format_c(_args), do: true
end
