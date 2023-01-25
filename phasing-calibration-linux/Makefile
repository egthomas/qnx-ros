OS:=$(shell uname -s)

CC=gcc
CFLAGS=-c
common_libs=-lm
linux_libs=-lgsl -lgslcblas
qnx_libs=-lsocket
INCLUDES=-I"../include/" -I"include/" -I"../tsg/include"   
SOURCES=main.c  _open_PLX9050.c _prog_conventions.c ../utils.c 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=phasing_calibration

CONVERT_SOURCES=time_delay.c 
CONVERT_OBJECTS=$(CONVERT_SOURCES:.c=.o)
CONVERT_EXECUTABLE=time_delay_convert

COMPARE_SOURCES=time_compare.c 
COMPARE_OBJECTS=$(COMPARE_SOURCES:.c=.o)
COMPARE_EXECUTABLE=time_delay_compare

BEAM_SOURCES=beamcode_generator.c 
BEAM_OBJECTS=$(BEAM_SOURCES:.c=.o)
BEAM_EXECUTABLE=beamcode_generator

SUMMARY_SOURCES=summary_generator.c 
SUMMARY_OBJECTS=$(SUMMARY_SOURCES:.c=.o)
SUMMARY_EXECUTABLE=summary_generator

FINAL_SOURCES=write_final_beamcodes.c  _open_PLX9050.c _prog_conventions.c ../utils.c 
FINAL_OBJECTS=$(FINAL_SOURCES:.c=.o)
FINAL_EXECUTABLE=write_final_beamcodes

all: $(SOURCES) $(EXECUTABLE) $(CONVERT_SOURCES) $(CONVERT_EXECUTABLE) $(BEAM_SOURCES) $(BEAM_EXECUTABLE) $(FINAL_EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(CONVERT_EXECUTABLE): $(CONVERT_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(CONVERT_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(CONVERT_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(COMPARE_EXECUTABLE): $(COMPARE_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(COMPARE_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(COMPARE_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(BEAM_EXECUTABLE): $(BEAM_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(BEAM_OBJECTS) $(common_libs) $(linux_libs) 
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(BEAM_OBJECTS) $(common_libs) $(qnx_libs) 
endif

$(SUMMARY_EXECUTABLE): $(SUMMARY_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(SUMMARY_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(SUMMARY_OBJECTS) $(common_libs) $(qnx_libs)
endif

$(FINAL_EXECUTABLE): $(FINAL_OBJECTS)
ifeq ($(OS),Linux)
	$(CC) -o $@ $(FINAL_OBJECTS) $(common_libs) $(linux_libs)
endif
ifeq ($(OS),QNX)
	$(CC) -o $@ $(FINAL_OBJECTS) $(common_libs) $(qnx_libs)
endif

.c.o:
	$(CC) $(CFLAGS) $< -o $@ $(INCLUDES)

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE) $(CONVERT_OBJECTS) $(CONVERT_EXECUTABLE) $(BEAM_OBJECTS) $(BEAM_EXECUTABLE)



