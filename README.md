C++ version of Weather Station code as at 08/10/2026: functionality complete (but rain is not fully tested) but code is only tested for stability, and daily code written but partly tested.
All except sensors need hardware testbed to fully assess.
To compile, use:
g++ MeteoC.cpp DB.cpp Devi.cpp Sensors.cpp -lwiringPi -lmysqlclient -o MeteoC
Libraries: pcf8574
