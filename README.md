# weatherctl
Weather Station Controller

Runs on a Raspberry Pi, talks to the Arduino running avrw via serial over USB. Written in C++, runs as a daemon process in the background.

'''
Usage:
  wctl [options]
  
Options:
  -port <port>          The serial port attached to the Arduino
  -baud <bitrate>       The baud rate to use, must be equal to the baud rate set on the Arduino
  -cfg  <config file>   Absolute path to the config file, defaults to webconfig.cfg in the root directory
  -d                    Daemonise the application, -log must be specified if this option is used
  -log  <log file>      Absolute path to the log file
  -lock <lock file>     Absolute path to the PID lock file (prevents the daemon being loaded twice)
  -h/?                  Displays usage info
'''

Acts as the master device to the Arduino running the AVRWeather code, it sends a request over the serial port every second, which could be one of the following commands, to which the Arduino responds:

- Get current TPH (Temperature, Pressure & Humidity)
- Get minimum TPH
- Get maximum TPH
- Reset min/max values
- Disable WDT reset (for Arduino watchdog testing) - will reset the Arduino device
- Ping

Every 20 seconds, wctl performs an HTTP POST to the web server running the weathersite code, as configured in the configuration file.

This software also exposes an admin web interface on http://<ip-address>/avr/cmd/index.html to control certain aspects of the Arduino interface, including resetting the Arduino via GPIO pin 12, through a level converter to the reset pin on the Arduino.
