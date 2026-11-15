NAME := Gomoku
CXX := c++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Werror -MMD -MP
LDLIBS := -lncurses -pthread
CORE := src/game.cpp src/evaluation.cpp src/patterns.cpp src/ai.cpp src/session.cpp
SOURCES := $(CORE) src/main.cpp
HEADERS := $(wildcard include/*.hpp)
OBJECTS := $(SOURCES:src/%.cpp=build/%.o)
TEST_FLAGS := $(filter-out -MMD -MP,$(CXXFLAGS))
SANITIZE_FLAGS := -std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined

all: $(NAME)

$(NAME): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDLIBS) -o $@

build/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

build/tests: tests/tests.cpp $(CORE) $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(TEST_FLAGS) tests/tests.cpp $(CORE) -pthread -o $@

build/pattern-tests: tests/pattern_tests.cpp $(CORE) $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(TEST_FLAGS) tests/pattern_tests.cpp $(CORE) -pthread -o $@

test: build/tests build/pattern-tests
	./build/tests
	./build/pattern-tests

sanitize:
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(SANITIZE_FLAGS) tests/tests.cpp $(CORE) -pthread -o build/tests-sanitize
	./build/tests-sanitize
	$(CXX) $(CPPFLAGS) $(SANITIZE_FLAGS) tests/pattern_tests.cpp $(CORE) -pthread -o build/patterns-sanitize
	./build/patterns-sanitize

ui-test: $(NAME)
	python3 tests/ui_smoke.py

build/memory-tests: tests/memory.cpp tests/fault_allocator.cpp tests/fault.hpp $(CORE) $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(TEST_FLAGS) tests/memory.cpp tests/fault_allocator.cpp $(CORE) -pthread -o $@

build/Gomoku-oom: src/main.cpp tests/fault_allocator.cpp tests/fault.hpp $(CORE) $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(TEST_FLAGS) src/main.cpp tests/fault_allocator.cpp $(CORE) $(LDLIBS) -o $@

memory-test: build/memory-tests build/Gomoku-oom
	./build/memory-tests
	python3 tests/ui_memory.py

build/performance-tests: tests/performance.cpp $(CORE) $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(TEST_FLAGS) tests/performance.cpp $(CORE) -pthread -o $@

performance-test: build/performance-tests
	./build/performance-tests

build/arena: tests/arena.cpp $(CORE) $(HEADERS)
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(TEST_FLAGS) tests/arena.cpp $(CORE) -pthread -o $@

arena-test: build/arena
	./build/arena

clean:
	rm -rf build

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

-include $(OBJECTS:.o=.d)
.PHONY: all clean fclean re test sanitize ui-test memory-test performance-test arena-test
