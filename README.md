# KARTINA TV
[Kartina TV](http://kartina.tv/) PVR client addon for [Kodi](http://kodi.tv)

## Build instructions

To clone:
```
git clone https://github.com/stanionascu/pvr.kartina.tv.git
cd pvr.kartina.tv && mkdir build && cd build
```

To generate project files and build: 
```
cmake /path/to/xbmc/project/cmake/addons \
	-DADDONS_TO_BUILD=pvr.kartina.tv \
	-DADDON_SRC_PREFIX=/path/to/addon/parent/dir \
	-DCMAKE_INSTALL_PREFIX=/path/where/to/install
cmake --build .
```

When targeting Windows add `-G"Visual Studio 12 2013"` to generate Visual Studio solution.
More on [CMake Generators](http://www.cmake.org/cmake/help/v3.3/manual/cmake-generators.7.html).

### Alternative

```
git clone https://github.com/stanionascu/xbmc.git
cd xbmc/project/cmake/addons && mkdir build && cd build
cmake .. \
	-DADDONS_TO_BUILD=pvr.kartina.tv \
	-DCMAKE_INSTALL_PREFIX=/path/where/to/install
cmake --build .
``` 

## Useful links

* [Kodi's PVR user support](http://forum.kodi.tv/forumdisplay.php?fid=167)
* [Kodi's PVR development support](http://forum.kodi.tv/forumdisplay.php?fid=136)

### Other useful links
Some cmake/depends scripts for jsoncpp come from [pvr.argustv](https://github.com/kodi-pvr/pvr.argustv)