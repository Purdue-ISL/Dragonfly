################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/AbrAlgorithm.cpp \
../src/BandwidthPredictor.cpp \
../src/Client.cpp \
../src/ClientNetworkLayer.cpp \
../src/Decoder.cpp \
../src/Server.cpp \
../src/TilePredictor.cpp \
../src/VideoPlayer.cpp 

OBJS += \
./src/AbrAlgorithm.o \
./src/BandwidthPredictor.o \
./src/Client.o \
./src/ClientNetworkLayer.o \
./src/Decoder.o \
./src/Server.o \
./src/TilePredictor.o \
./src/VideoPlayer.o 

CPP_DEPS += \
./src/AbrAlgorithm.d \
./src/BandwidthPredictor.d \
./src/Client.d \
./src/ClientNetworkLayer.d \
./src/Decoder.d \
./src/Server.d \
./src/TilePredictor.d \
./src/VideoPlayer.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++11 -D__GXX_EXPERIMENTAL_CXX0X__ -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


