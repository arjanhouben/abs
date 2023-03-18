## This is Arjans Build System, or ABS

After having used CMake for many years, often cursing it's existence, but never able to come up with something better, I decided I should re-write CMake itself. It's UI was obviously doing something right, so why not accept it as given and instead work on changing the string-based language that many of us have learned to hate.

With ABS, you only need to write C++, and learn to bootstrap your project, which is most often as easy as:
- mkdir build
- cd build
- c++ --std=c++20 ../Makefile.cpp -o Makefile
- ./Makefile

Note, this project was NOT written for you, it was written for me, Arjan. There will be parts you don't like. I am sorry. You can always suggest changes, but they may never end up in this repository.

As I don't believe in copyright or the notion that ideas can be caged, I suggest you take the code and change it to your liking. It's yours as much as it is mine.