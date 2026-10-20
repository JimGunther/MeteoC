C++ version of Weather Station code as at 20/10/2026: functionality complete (but rain is not fully tested) but code is only tested for stability, and daily code seems OK (minor logging issue corrected).
All except sensors need hardware testbed to fully assess.
To compile, use:
g++ MeteoC.cpp DB.cpp Devi.cpp Sensors.cpp -lwiringPi -lmysqlclient -o MeteoC
Libraries: pcf8574
This version appears to be stable and no segmentation faults / bus errors have been seen but code has been running only a few hours. Worth loading to Pi3 at Shed for more extended testing.
Extended testing needed to check interface with new hardware, especially rain Wemos and serial connection.
