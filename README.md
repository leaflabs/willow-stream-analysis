# willow-stream-analysis
This repository contains stream analysis programs for WillowGUI. To add an
analysis program, create a new folder in the top level and put inside of it an
executable named "main". This executable can be anything - a MATLAB script, a
Python GUI, a compiled program, etc. We require that analysis programs take as
their first command-line arguments the directory of the proto2bytes utility that
ships with the leafysd daemon program (by default, /path/to/leafysd/util).
In order to run the stream analysis process, however, another process (such as
the WillowGUI) must consume your analysis script's stderr as described
[below] (#datanode-transactions).

To make this work with WillowGUI, set the "Stream Analysis Directory"
parameter in the Config Wizard, or else in the Settings menu during runtime.
Then, any subdirectory containing a "main" executable will appear as an option
for custom analysis in the Stream Dialog. WillowGUI will launch the
executable as a subprocess, with its working directory set to that of the
executable - so metadata (e.g. probe mappings) or library files can be kept in
the same folder alongside the exectuable. stdout and stderr will be piped to
"oFile" and "eFile" in the working directory, so avoid using these filenames
for metadata, as they will get overwritten upon execution.

# Datanode transactions

Streaming analysis programs will need to be able to conduct some types of
transactions with the Willow datanode. They can gain information about what data
is currently being streamed with the proto2bytes utility mentioned above.

However, to request that the datanode start or stop recording, scripts must
send strings over stderr that constitute parsable requests to the WillowGUI.
These strings must be formatted as follows:

* they begin with `"hwif_req: "` (space after the `:`)
* they contain the name of a single function for the WillowGUI to call, plus any
  additional arguments to that function, separated by commas and spaces (`", "`)

Some example acceptable request messages (currently, this list is exhaustive):

* `"stopStreaming"`: request that the datanode stop streaming data
* `"setSubsamples_byChip, <int>"`: request that the datanode to stream data from
  chip number <int>
* `"startStreaming_subsamples"`: request that the datanode stream subsamples
  from the chip set by `setSubsamples_byChip`.
* `"startStreaming_boardsamples"`: request that the datanode stream boardsamples
  from the chip set by `setSubsamples_byChip`
