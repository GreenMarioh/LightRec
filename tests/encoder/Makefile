CXX = g++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Isrc -Itests/encoder/mock_headers/dummy -I/ucrt64/include -I../../src/encoder

LDFLAGS = -L/ucrt64/lib -lvpl -ld3d11 -ldxgi -lmfplat -lmfuuid -lmf -lole32 -luser32

SRC_DIR = ../../src/encoder
SRC_FILES = $(SRC_DIR)/QSVEncoder.cpp \
            $(SRC_DIR)/NVENCEncoder.cpp \
            $(SRC_DIR)/AMFEncoder.cpp \
            $(SRC_DIR)/MFTEncoder.cpp \
            $(SRC_DIR)/EncoderFactory.cpp \
            test_encoder.cpp

OBJ_FILES = QSVEncoder.o NVENCEncoder.o AMFEncoder.o MFTEncoder.o EncoderFactory.o test_encoder.o
TARGET = test_encoder.exe

all: create_dummy $(TARGET)

create_dummy:
	@if not exist mock_headers\dummy mkdir mock_headers\dummy

$(TARGET): $(OBJ_FILES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

QSVEncoder.o: $(SRC_DIR)/QSVEncoder.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

NVENCEncoder.o: $(SRC_DIR)/NVENCEncoder.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

AMFEncoder.o: $(SRC_DIR)/AMFEncoder.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

MFTEncoder.o: $(SRC_DIR)/MFTEncoder.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

EncoderFactory.o: $(SRC_DIR)/EncoderFactory.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test_encoder.o: test_encoder.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	@if exist *.o del *.o
	@if exist $(TARGET) del $(TARGET)
	@if exist mock_headers\dummy rmdir mock_headers\dummy
