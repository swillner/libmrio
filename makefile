CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(patsubst src/%.cpp,bin/%.o,$(CPP_FILES))
.SECONDARY: OBJ_FILES
LD_FLAGS := -lstdc++ -lnetcdf_c++4 -lnetcdf -Wl,--gc-sections
CC_FLAGS := -std=c++0x -I include/ -fdata-sections -ffunction-sections -Wshadow

all: fast

fast: CC_FLAGS += -O3
fast: gcc

debug: CC_FLAGS += -g -DDEBUG
debug: gcc

gcc: CXX = g++
gcc: makedir
gcc: binaries

clean:
	@rm -f $(OBJ_FILES) mrio_disaggregate

makedir:
	@mkdir -p bin

binaries: mrio_disaggregate

mrio_disaggregate: $(OBJ_FILES)
	@echo Linking to $@
	@$(CXX) -o $@ $^ $(CC_FLAGS) $(LD_FLAGS)

bin/%.o: src/%.cpp include/%.h
	@echo Compiling $<
	@$(CXX) $(CC_FLAGS) -c -o $@ $<

bin/main.o: src/main.cpp
	@echo Compiling $<
	@$(CXX) $(CC_FLAGS) -c -o $@ $<
