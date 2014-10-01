CC=g++
SOURCES=blingc.cc cclex.cc
RELOP=-O2 -Wall
DBGOP=-g -Wall
OUT=blingc

release:
	mkdir -p ../release
	$(CC) $(RELOP) $(SOURCES) -o ../release/$(OUT)

debug:
	mkdir -p ../debug
	$(CC) $(DBGOP) $(SOURCES) -o ../debug/$(OUT)

