CPP_FILES := $(wildcard src/*.cpp)
OBJ_FILES := $(patsubst src/%.cpp,bin/%.o,$(CPP_FILES))
LD_FLAGS := -lstdc++
CC_FLAGS := -std=c++0x -I include/ -Wshadow

fast: CC_FLAGS += -O3
fast: gcc

gcc: CXX = g++
gcc: makedir
gcc: binaries

debug: CC_FLAGS += -g -DDEBUG
debug: gcc

clean:
	@rm -f $(OBJ_FILES) bin/disaggregation

makedir:
	@mkdir -p bin

binaries: bin/disaggregation

bin/disaggregation: $(OBJ_FILES)
	@echo Linking to $@
	@$(CXX) -o $@ $^ $(CC_FLAGS) $(LD_FLAGS)

bin/%.o: src/%.cpp include/%.h
	@echo Compiling $<
	@$(CXX) $(CC_FLAGS) -c -o $@ $<

bin/main.o: src/main.cpp
	@echo Compiling $<
	@$(CXX) $(CC_FLAGS) -c -o $@ $<
