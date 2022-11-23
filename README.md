# meg

> Demo code for a bluetooth-powered EMG device

This is a collection of code for a ESP32 BLE device that:

- Initializes itself as a BLE server
- Takes analog measurements from peripherals
- Writes those measurements to various internal BLE-queryable datastructures
- And also writes those measurements to a SIM card

The software components included here are:

| Component | Purpose |
| - | - |
| `sketches/` | EPS32 code in Arduino IDE sketch form |
| `sketches/meg/meg.ino` | ESP32 code that's uploaded to the device and runs on power-on |
| `libraries/` | ESP32 third-party libraries |
| `requirements.txt` | Python dependencies (install with `pip install -r requirements.txt`) |
| `meg/` | Python package containing development + client code |
| `meg/client` (`meg.client`) | Python module for connecting to the device using [bleak](https://github.com/hbldh/bleak) |
| `meg/cli` (`meg.cli`) | Python command-line client (uses client, but also includes handy development commands) |


# Software Installation

## Device (C++, Arduino IDE)

- Copy `libraries/NimBLE-Arduino-1.4.1` into your Arduino sketchbook library (e.g. `C:\Users\meg\OneDrive\Documents\Arduino`)
- Boot up Arduino IDE
- Confirm the library is installed by going to `Sketch -> Include Library`. Under `Contributed Libraries` you should see `NimBLE-Arduino`
- Open the `meg` sketch in this repository (`sketches/meg/meg.ino`)
- (*optional*): Change `MEG_BLE_SERIAL_NUMBER` to something unique, if you want an extra way of identifying the device
- Upload

## Client (Python 3.8+)

- Install necessary python dependencies with `pip install -r requirements.txt`
- `meg/cli.py` is a runnable python script (e.g. `python meg/cli.py` should work)


# Development

## Device

- Hardware C++ code was entirely written in the Arduino IDE
- Seperate sketches were used for one-off debugging tasks (printouts etc)
- If `MEG_DEBUG` is defined, then extra information is printed via the serial output (handy in Arduino IDE, which has a serial monitor)
- Other "debug"-ish stuff (e.g. perf counters) are written to specialized BLE services, because that makes it possible to do more powerful stuff (e.g. plot changes in a debug counter using `matplotlib`)

## Client

- Client code was written using [vscode](https://code.visualstudio.com/) with the python plugin
- The client module (`meg.client`) is written as an asynchronous library API. This makes it possible to seperately integrate other stuff against that API (e.g. a plotter). `meg.cli`) uses that API internally.
- You can run the CLI via `vscode`, with debugging, by adding a suitable configuration:
  - `Run -> Add Configuration`
  - Add an `"args"` entry that runs the to-be-debugged CLI command. E.g. to run `--help` whenever F5 is pressed in `vscode`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Python: Current File",
            "type": "python",
            "request": "launch",
            "program": "${file}",
            "args": ["--help"],
            "console": "integratedTerminal",
            "justMyCode": true
        }
    ]
}
```
