/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_MAPNIKVT_FEATUREDECODER_H_
#define _CARTO_MAPNIKVT_FEATUREDECODER_H_

#include "FeatureData.h"
#include "Geometry.h"

#include <memory>
#include <string>
#include <unordered_set>

namespace carto { namespace mvt {
    class FeatureDecoder {
    public:
        class FeatureIterator {
        public:
            virtual ~FeatureIterator() = default;

            virtual bool valid() const = 0;
            virtual void advance() = 0;

            virtual long long getFeatureId() const = 0;
            virtual std::shared_ptr<FeatureData> getFeatureData() const = 0;
            virtual std::shared_ptr<Geometry> getGeometry() const = 0;
        };

        virtual ~FeatureDecoder() = default;
    };
} }

#endif
