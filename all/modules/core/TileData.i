#ifndef _TILEDATA_I
#define _TILEDATA_I

%module TileData

!proxy_imports(carto::TileData, core.BinaryData)

%{
#include "core/TileData.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/BinaryData.i"

!shared_ptr(carto::TileData, core.TileData)

%attribute(carto::TileData, long long, MaxAge, getMaxAge, setMaxAge)
%attribute(carto::TileData, bool, ReplaceWithParent, isReplaceWithParent, setReplaceWithParent)
%attributestring(carto::TileData, std::shared_ptr<carto::BinaryData>, Data, getData)
!standard_equals(carto::TileData);

%include "core/TileData.h"

#endif
