# Regional and sectoral disaggregation of multi-regional input-output tables

C++-Implementation of the flexible algorithm for regional and sectoral disaggregation of multi-regional input-output tables as described in:

L. Wenz, S.N. Willner, A. Radebach, R. Bierkandt, J.C. Steckel, A. Levermann  
**[Regional and sectoral disaggregation of multi-regional input-output tables: a flexible algorithm](http://www.pik-potsdam.de/~anders/publications/wenz_willner15.pdf)**  
*Economic Systems Research* 27 (2015), [DOI: 10.1080/09535314.2014.987731](http://dx.doi.org/10.1080/09535314.2014.987731).

It includes a library for handling heterogeneous MRIO tables with up to one level of hierarchy. If you want to use it and have trouble with it just drop me an [email](mailto:sven.willner@pik-potsdam.de).

## Dependencies

The implementation makes use of C++11 and the [https://github.com/Unidata/netcdf-cxx4](NetCDF-CXX4-library) (e.g. package `libnetcdf-c++4-dev` in Ubuntu/Debian).

## Compiling

A makefile is provided, just use
```
make
```
and the binary can be found in the `bin` folder. Compiler has to support C++11, only tested with GCC.

## Usage

`disaggregation` expects five filenames as parameters (example given under `examples/simple`):

1. Index file
CSV-file with two columns: 1. region name; 2. sector name. Its rows correspond to the rows and columns of the basetable file.

2. Basetable file
CSV-file of the basetable to be disaggregated

3. Settings file
CSV-file to control which regions/sectors to disaggregate into how many subregions/subsectors. Has three columns: 1. `region` or `sector`, depending on which to disaggregate; 2. Name of the region/sector to disaggregate; 3. Number of subregions/subsectors to disaggregate into.

4. Proxy file
CSV-file with the proxies. Column numbers depend on proxy level (as documented in the paper). First column: Proxy level; Then columns of either region/sector name or column pairs of region/sector name and index (starting with 0) of subregion/subsector; Then value; Concluding with an optional column given the sum (only applies for GDP and population levels).

5. Output file
Name of file to write disaggregated table to. It is given in CSV-format, rows and columns sorted by regions (subregions replacing original aggregated region) first, then sectors (subsectors replacing original aggregated sector).

Additionally an optional threshold parameter for only considering basetable values greater than the threshold can be given using `-t THRESHOLD` or `-threshold=THRESHOLD`.
