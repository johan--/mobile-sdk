/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_LINESYMBOLIZER_H_
#define _CARTO_MAPNIKVT_LINESYMBOLIZER_H_

#include "GeometrySymbolizer.h"

namespace carto { namespace mvt {
    class LineSymbolizer : public GeometrySymbolizer {
    public:
        explicit LineSymbolizer(std::shared_ptr<Logger> logger) : GeometrySymbolizer(std::move(logger)) { }

        virtual void build(const FeatureCollection& featureCollection, const SymbolizerContext& symbolizerContext, const ExpressionContext& exprContext, vt::TileLayerBuilder& layerBuilder) override;

    protected:
        virtual void bindParameter(const std::string& name, const std::string& value) override;

        static std::shared_ptr<vt::BitmapPattern> createDashBitmapPattern(const std::vector<float>& strokeDashArray);

        enum { MIN_SUPERSAMPLING_FACTOR = 2, MAX_SUPERSAMPLING_FACTOR = 16 };

        vt::Color _stroke = vt::Color(0xff000000);
        float _strokeWidth = 1.0f;
        float _strokeOpacity = 1.0f;
        std::string _strokeLinejoin = "miter";
        std::string _strokeLinecap = "butt";
        std::string _strokeDashArray;
    };
} }

#endif
