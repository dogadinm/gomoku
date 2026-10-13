NAME := Gomoku
CXX := c++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Werror -MMD -MP
LDLIBS := -lncurses -pthread
SOURCES := src/game.cpp src/ai.cpp src/session.cpp src/main.cpp
OBJECTS := $(SOURCES:src/%.cpp=build/%.o)
all: $(NAME)
$(NAME): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDLIBS) -o $@
build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
build/tests: tests/tests.cpp src/game.cpp src/ai.cpp src/session.cpp include/game.hpp include/ai.hpp include/session.hpp
	@mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/tests.cpp src/game.cpp src/ai.cpp src/session.cpp -pthread -o $@
test: build/tests
	./build/tests
sanitize:
	@mkdir -p build
	$(CXX) $(CPPFLAGS) -std=c++17 -g -O1 -fsanitize=address,undefined tests/tests.cpp src/game.cpp src/ai.cpp src/session.cpp -pthread -o build/tests-sanitize
	./build/tests-sanitize
clean:
	rm -rf build
fclean: clean
	rm -f $(NAME)
re: fclean
	$(MAKE) all
-include $(OBJECTS:.o=.d)
.PHONY: all clean fclean re test sanitize

ui-test: $(NAME)
	python3 tests/ui_smoke.py
.PHONY: ui-test
