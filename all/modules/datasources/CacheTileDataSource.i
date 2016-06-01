#ifndef _CACHETILEDATASOURCE_I
#define _CACHETILEDATASOURCE_I

%module(directors="1") CacheTileDataSource

!proxy_imports(carto::CacheTileDataSource, core.MapTile, core.StringMap, core.TileData, datasources.TileDataSource)

%{
#include "datasources/CacheTileDataSource.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "datasources/TileDataSource.i"

!polymorphic_shared_ptr(carto::CacheTileDataSource, datasources.CacheTileDataSource)

%feature("director") carto::CacheTileDataSource;

%attribute(carto::CacheTileDataSource, unsigned int, Capacity, getCapacity, setCapacity)

%include "datasources/CacheTileDataSource.h"

#endif
