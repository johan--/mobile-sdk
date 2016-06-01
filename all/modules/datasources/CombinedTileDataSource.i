#ifndef _COMBINEDTILEDATASOURCE_I
#define _COMBINEDTILEDATASOURCE_I

%module(directors="1") CombinedTileDataSource

!proxy_imports(carto::CombinedTileDataSource, core.MapTile, core.StringMap, core.TileData, datasources.TileDataSource)

%{
#include "datasources/CombinedTileDataSource.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "datasources/TileDataSource.i"

!polymorphic_shared_ptr(carto::CombinedTileDataSource, datasources.CombinedTileDataSource)

%feature("director") carto::CombinedTileDataSource;

%include "datasources/CombinedTileDataSource.h"

#endif
