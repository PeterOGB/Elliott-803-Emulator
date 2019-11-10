# An Elliott-803-Emulator

## Beta Testing Release ##

This is not the final version, but is an early release of a few of the emulator's components to 
allow some testing by other people !

## Requirements ##

The development versions of the following libraries need to beinstalled to build the emulator.

| Library        | Package Name |
|:-------------:|:-------------:|
| Gtk 3  | gtk+-3.0-dev |
| Alsa sound | libasound2-dev |
| iberty | libiberty-dev |

The package names apply for Debian derived distributions

Also the "cmake" application is needed.

## Building the emulator ##

Runing 

```
cmake .
make
```

is all that is required to produce the executable !

If you run the emulator from the build directory with `./803` it will use the resources in
that directory.  An "install" make target will be added later.


![alt text](https://github.com/PeterOGB/Elliott-803-Emulator/doc/Beta1.png "First Release Components")

