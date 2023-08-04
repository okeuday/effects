#-*-Mode:make;coding:utf-8;tab-width:4;c-basic-offset:4-*-
# ex: set ft=make fenc=utf-8 sts=4 ts=4 sw=4 noet nomod:

CXX = g++
CXXFLAGS = -ffp-contract=off -g -O0 -std=c++17
#CXX = clang++
#CXXFLAGS = -ffp-exception-behavior=strict -g -O0 -std=c++17

all: tests
	./tests

clean:
	rm -f tests

tests: tests.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

tests.cpp: effects.hpp
