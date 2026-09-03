CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
	-I../../components/fastled \
	-I../../components/fastled/lib8tion \
	-I../../main/led

.PHONY: run clean
run: phase_test
	./phase_test

phase_test: phase_test.cpp ../../main/led/led_phase.h
	$(CXX) $(CXXFLAGS) phase_test.cpp -o phase_test

clean:
	rm -f phase_test
