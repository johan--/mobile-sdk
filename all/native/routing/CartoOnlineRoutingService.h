/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CARTOONLINEROUTINGSERVICE_H_
#define _CARTO_CARTOONLINEROUTINGSERVICE_H_

#include "routing/RoutingService.h"

#include <memory>
#include <string>

namespace carto {
    class Projection;

    /**
     * An online routing service that connects to Nutiteq-provided routing server.
     */
    class CartoOnlineRoutingService : public RoutingService {
    public:
        /**
         * Constructs a new CartoOnlineRoutingService instance given service source.
         * @param source The source id of the service.
         */
        CartoOnlineRoutingService(const std::string& source);
        virtual ~CartoOnlineRoutingService();

        virtual std::shared_ptr<RoutingResult> calculateRoute(const std::shared_ptr<RoutingRequest>& request) const;

    protected:
        std::string _source;
        
        static const double COORDINATE_SCALE;
    };
    
}

#endif
