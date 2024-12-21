CFLAGS = -std=c++20 -O2
LDFLAGS = -lSDL3 -lvulkan 

vk_tut: main.cpp
	g++ $(CFLAGS) -o vktest main.cpp $(LDFLAGS)

.PHONY: test clean

test: vk_tut
	./vktest

clean:
	rm -f vktest
