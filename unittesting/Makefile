CXX:=g++
OUTPUT:=build

ETL_INCLUDE:=../thirdparty/etl/include
FPM_INCLUDE:=../thirdparty/fpm/include
TROLL_INCLUDE:=../thirdparty/troll-string-util/include
CATCH_DIR:=../thirdparty/Catch2/extras

WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable
CFLAGS_LIB:=-g -pipe -static -fno-builtin \
	-fno-rtti -fno-exceptions -std=gnu++17 -I $(ETL_INCLUDE) -I $(FPM_INCLUDE) -I $(TROLL_INCLUDE) \
	-Wno-return-type -I $(CATCH_DIR)
CFLAGS:=$(CFLAGS_LIB) $(WARNINGS) -DIS_TRACK_A=1

SOURCES := $(wildcard *.cpp) $(CATCH_DIR)/catch_amalgamated.cpp ../track_new.cpp ../track_graph.cpp ../track_consts.cpp
# Create .o and .d files for every .cpp
OBJECTS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.cpp, %.o, $(notdir $(SOURCES))))
DEPENDS := $(patsubst %, $(OUTPUT)/%, $(patsubst %.cpp, %.d, $(notdir $(SOURCES))))

all: $(OUTPUT) $(OUTPUT)/unittest.out

$(OUTPUT):
	mkdir -p $(OUTPUT)

clean:
	rm -rf $(OUTPUT)

$(OUTPUT)/catch_amalgamated.o: $(CATCH_DIR)/catch_amalgamated.cpp Makefile
	$(CXX) $(CFLAGS_LIB) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: ../%.cpp Makefile
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/%.o: %.cpp Makefile
	$(CXX) $(CFLAGS) -MMD -MP -c $< -o $@

$(OUTPUT)/unittest.out: $(OBJECTS)
	$(CXX) $(CFLAGS) $^ -o $@

run: $(OUTPUT) $(OUTPUT)/unittest.out
	$(word 2,$^)

.PHONY: run

-include $(DEPENDS)
