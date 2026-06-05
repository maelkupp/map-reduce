CXX      := g++
CXXFLAGS := -std=c++17 -O2 -pthread -I.
TFLAGS   := -std=c++17 -O1 -pthread -I. -fsanitize=thread -g
AFLAGS   := -std=c++17 -O1 -pthread -I. -fsanitize=address -g

.PHONY: sudoku queens sort clean



sudoku:
	$(CXX) $(CXXFLAGS) sudoku.cpp -o sudoku && ./sudoku

queens:
	$(CXX) $(CXXFLAGS) N_queens.cpp -o queens && ./queens

sort:
	$(CXX) $(CXXFLAGS) sort.cpp -o sort && ./sort

clean:
	rm -f sudoku queens sort q1 q2 q3 q4 bench q2_tsan q4_tsan q4_asan
