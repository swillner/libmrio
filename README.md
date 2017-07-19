[![DOI](https://img.shields.io/badge/DOI-10.5281%2Fzenodo.832054-blue.svg)](http://dx.doi.org/10.5281/zenodo.832054)


# Regional and sectoral disaggregation of multi-regional input-output tables

C++-Implementation of the flexible algorithm for regional and sectoral disaggregation of multi-regional input-output tables as described in:

L. Wenz, S.N. Willner, A. Radebach, R. Bierkandt, J.C. Steckel, A. Levermann  
**[Regional and sectoral disaggregation of multi-regional input-output tables: a flexible algorithm](http://www.pik-potsdam.de/~anders/publications/wenz_willner15.pdf)**  
*Economic Systems Research* 27 (2015), [DOI: 10.1080/09535314.2014.987731](http://dx.doi.org/10.1080/09535314.2014.987731).

It includes a library for handling heterogeneous MRIO tables with up to one level of hierarchy. If you want to use it and have trouble with it just drop me an [email](mailto:sven.willner@pik-potsdam.de).

## Dependencies

The implementation makes use of C++11 and the [https://github.com/Unidata/netcdf-cxx4](NetCDF-CXX4-library) (e.g. package `libnetcdf-c++4-dev` in Ubuntu/Debian).

It also depends on my [https://github.com/swillner/cpp-library](CSV parser) and [https://github.com/swillner/settingsnode](Settings wrapper). These are included as submodules, so make sure to recursively clone this repository:
```
git clone https://github.com/swillner/libmrio.git --recursive
```

## Compiling

A makefile is provided, just use
```
make
```
to create the `mrio_disaggregate` binary. Compiler has to support C++11, only tested with GCC.

## Usage

`mrio_disaggregate` expects the path of a YAML control file as parameter (or `-` and the file is read from stdin):

- YAML control file
see example in `examples/simple`.

- Proxy files
CSV-files with proxy data. Column numbers depend on proxy level (as documented in the paper). First column: Year; Then columns of either region/sector name or column pairs of region/sector name and index (starting with 0) of subregion/subsector; Then value; Concluding with an optional column given the sum (only applies for GDP and population levels).

- Formats
Supported input and output formats are `csv`, `mrio` and `netcdf`.
