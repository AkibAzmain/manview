manview.so:
	g++ -shared manview.cpp -fPIC -std=c++17 $(CXXFLAGS) -o manview.so -ldocview $(LDFLAGS)
