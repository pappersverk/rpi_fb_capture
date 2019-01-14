defmodule RpiFbCapture.Capture do
  @moduledoc """
  Capture data and metadata for one frame.
  """
  defstruct data: [],
            width: 0,
            height: 0,
            format: :rgb565

  @type t :: %__MODULE__{
          data: iodata(),
          width: non_neg_integer(),
          height: non_neg_integer(),
          format: RpiFbCapture.format()
        }
end
