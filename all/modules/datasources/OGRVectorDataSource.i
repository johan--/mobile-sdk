#ifndef _OGRVECTORDATASOURCE_I
#define _OGRVECTORDATASOURCE_I

%module(directors="1") OGRVectorDataSource

#ifdef _CARTO_GDAL_SUPPORT

!proxy_imports(carto::OGRVectorDataSource, datasources.OGRVectorDataBase, datasources.OGRFieldType, datasources.OGRGeometryType, core.MapBounds, core.StringVector, geometry.GeometrySimplifier, projections.Projection, renderers.components.CullState, styles.StyleSelector, vectorelements.VectorElement, vectorelements.VectorElementVector)

%{
#include "datasources/OGRVectorDataSource.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "core/MapBounds.i"
%import "core/StringVector.i"
%import "datasources/VectorDataSource.i"
%import "datasources/OGRVectorDataBase.i"
%import "geometry/GeometrySimplifier.i"
%import "styles/StyleSelector.i"
%import "vectorelements/VectorElement.i"

!polymorphic_shared_ptr(carto::OGRVectorDataSource, datasources.OGRVectorDataSource)

%feature("director") carto::OGRVectorDataSource;

%attributeval(carto::OGRVectorDataSource, carto::MapBounds, DataExtent, getDataExtent)
%attribute(carto::OGRVectorDataSource, int, FeatureCount, getFeatureCount)
%attribute(carto::OGRVectorDataSource, OGRGeometryType::OGRGeometryType, GeometryType, getGeometryType)
%attributeval(carto::OGRVectorDataSource, std::vector<std::string>, FieldNames, getFieldNames)
%attributestring(carto::OGRVectorDataSource, std::string, CodePage, getCodePage, setCodePage)
!attributestring_polymorphic(carto::OGRVectorDataSource, geometry.GeometrySimplifier, GeometrySimplifier, getGeometrySimplifier, setGeometrySimplifier)

%include "datasources/OGRVectorDataSource.h"

#endif

#endif
