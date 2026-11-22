C++ version of Weather Station code as at 17/11/2026: functionality complete (but rain is not fully tested) but code (inclusing hourly/daily) is only tested for stability.
All except sensors need hardware testbed to fully assess.
To compile, use:
g++ MeteoC.cpp DB.cpp Devi.cpp Sensors.cpp -lwiringPi -lmysqlclient -o MeteoC
Libraries: pcf8574
The rain had code has been tweaked to allow for corrupted serial port data: if the serial string is missing or fails certain basic sanity checks, no rain update is effected.
