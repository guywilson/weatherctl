# weatherctl
Weather Station Controller

Runs on a Raspberry Pi, talks to the Arduino running avrw via serial over USB. Written in C++, and compiled using g++ on the Raspberry Pi. Can run as a daemon process in the background (with the -d opton).

```
Usage:
  wctl [options]
  
Options:
  -port <port>          The serial port attached to the Arduino
  -baud <bitrate>       The baud rate to use, must be equal to the baud rate set on the Arduino
  -cfg  <config file>   Absolute path to the config file, defaults to webconfig.cfg in the root directory
  -d                    Daemonise the application, -log must be specified if this option is used
  -log  <log file>      Absolute path to the log file
  -h/?                  Displays usage info
```

Acts as the master device to the Arduino running the AVRWeather code, it sends a request over the serial port every second, which could be one of the following commands, to which the Arduino responds:

- Get current TPH (Temperature, Pressure & Humidity)
- Get minimum TPH
- Get maximum TPH
- Reset min/max values
- Disable WDT reset (for Arduino watchdog testing) - will reset the Arduino device
- Ping

Every 20 seconds, wctl performs an HTTP POST to the web server running the weathersite code, as configured in the configuration file.

This software also exposes an admin web interface on https://ip-address/ to control certain aspects of the Arduino interface, including resetting the Arduino via GPIO pin 12, through a level converter to the reset pin on the Arduino.

I've installed this as a service on Ubuntu 18.04 LTS using the following in a service descriptor: /etc/systemd/system/weatherctl.service

```
[Unit]
Description=Weather Controller service

[Service]
ExecStart=/sandiskusb/bin/wctl -cfg /sandiskusb/weatherctl/wctl.cfg

[Install]
WantedBy=multi-user.target
```

The service can then be controlled with the systemctl command, e.g.

```
sudo systemctl restart weatherctl
```

Continuous build and deployment is handled through the genkins.sh bash script, this is run as a cron job every 5 minutes, in crontab use the following:

```
# /etc/crontab: system-wide crontab
# Unlike any other crontab you don't have to run the `crontab'
# command to install the new version when you edit this file
# and files in /etc/cron.d. These files also have username fields,
# that none of the other crontabs do.

SHELL=/bin/sh
PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin

# m h dom mon dow user  command
17 *    * * *   root    cd / && run-parts --report /etc/cron.hourly
25 6    * * *   root    test -x /usr/sbin/anacron || ( cd / && run-parts --report /etc/cron.daily )
47 6    * * 7   root    test -x /usr/sbin/anacron || ( cd / && run-parts --report /etc/cron.weekly )
52 6    1 * *   root    test -x /usr/sbin/anacron || ( cd / && run-parts --report /etc/cron.monthly )
*/5 *   * * *   root    cd /sandiskusb/build/weatherctl && ./genkins.sh >> /sandiskusb/log/genkins.log
#
```
