C++ version of Weather Station code as at 11/09/2026: functionality complete (except for rain: awaiting details from Richard) but code is only tested for stability, and daily code written but partly tested.
All except sensors need hardware testbed to fully assess.
To compile, use:
g++ MeteoC.cpp DB.cpp Devi.cpp Sensors.cpp -lwiringPi -lmysqlclient -o MeteoC
Libraries: pcf8574
