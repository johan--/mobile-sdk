#include "CartoVisLoader.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "graphics/Bitmap.h"
#include "datasources/CacheTileDataSource.h"
#include "datasources/HTTPTileDataSource.h"
#include "layers/Layer.h"
#include "layers/SolidLayer.h"
#include "layers/RasterTileLayer.h"
#include "layers/VectorTileLayer.h"
#include "network/HTTPClient.h"
#include "services/CartoMapsService.h"
#include "services/CartoVisBuilder.h"
#include "vectortiles/CartoVectorTileDecoder.h"
#include "utils/AssetPackage.h"
#include "utils/NetworkUtils.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <algorithm>
#include <regex>
#include <unordered_map>
#include <vector>

#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <mapnikvt/CSSColorParser.h>

namespace {
    
    boost::optional<bool> getBool(const picojson::value& val) {
        if (val.is<bool>()) {
            return val.get<bool>();
        }
        if (val.is<std::int64_t>()) {
            return val.get<std::int64_t>() != 0;
        }
        if (val.is<double>()) {
            return val.get<double>() != 0;
        }
        if (val.is<std::string>()) {
            picojson::value parsedVal;
            if (picojson::parse(parsedVal, val.get<std::string>()).empty()) {
                return getBool(parsedVal);
            }
        }
        return boost::optional<bool>();
    }

    boost::optional<int> getInt(const picojson::value& val) {
        if (val.is<bool>()) {
            return val.get<bool>() ? 1 : 0;
        }
        if (val.is<std::int64_t>()) {
            return static_cast<int>(val.get<std::int64_t>());
        }
        if (val.is<double>()) {
            return static_cast<int>(val.get<double>());
        }
        if (val.is<std::string>()) {
            return boost::lexical_cast<int>(val.get<std::string>());
        }
        return boost::optional<int>();
    }

    boost::optional<double> getDouble(const picojson::value& val) {
        if (val.is<bool>()) {
            return val.get<bool>() ? 1 : 0;
        }
        if (val.is<std::int64_t>()) {
            return static_cast<double>(val.get<std::int64_t>());
        }
        if (val.is<double>()) {
            return val.get<double>();
        }
        if (val.is<std::string>()) {
            return boost::lexical_cast<double>(val.get<std::string>());
        }
        return boost::optional<double>();
    }

    boost::optional<std::string> getString(const picojson::value& val) {
        if (val.is<bool>()) {
            return std::string(val.get<bool>() ? "true" : "false");
        }
        if (val.is<std::int64_t>()) {
            return boost::lexical_cast<std::string>(val.get<std::int64_t>());
        }
        if (val.is<double>()) {
            return boost::lexical_cast<std::string>(val.get<double>());
        }
        if (val.is<std::string>()) {
            return val.get<std::string>();
        }
        return boost::optional<std::string>();
    }

    boost::optional<carto::MapPos> getMapPos(const picojson::value& val) {
        if (val.is<picojson::array>()) {
            const picojson::array& arr = val.get<picojson::array>();
            if (arr.size() == 2) {
                if (auto x = getDouble(arr[0])) {
                    if (auto y = getDouble(arr[1])) {
                        return carto::MapPos(*y, *x); // lat/long coordinates are flipped
                    }
                }
            }
        }
        else if (val.is<std::string>()) {
            picojson::value parsedVal;
            if (picojson::parse(parsedVal, val.get<std::string>()).empty()) {
                return getMapPos(parsedVal);
            }
        }
        return boost::optional<carto::MapPos>();
    }

    boost::optional<carto::MapBounds> getMapBounds(const picojson::value& val) {
        if (val.is<picojson::array>()) {
            const picojson::array& arr = val.get<picojson::array>();
            if (arr.size() == 2) {
                if (auto min = getMapPos(arr[0])) {
                    if (auto max = getMapPos(arr[1])) {
                        return carto::MapBounds(*min, *max);
                    }
                }
            }
        }
        else if (val.is<std::string>()) {
            picojson::value parsedVal;
            if (picojson::parse(parsedVal, val.get<std::string>()).empty()) {
                return getMapBounds(parsedVal);
            }
        }
        return boost::optional<carto::MapBounds>();
    }

}

namespace carto {

    CartoVisLoader::CartoVisLoader() :
        _defaultVectorLayerMode(false),
        _strictMode(false),
        _vectorTileAssetPackage(),
        _defaultUsername(),
        _mutex()
    {
    }

    CartoVisLoader::~CartoVisLoader() {
    }

    bool CartoVisLoader::isDefaultVectorLayerMode() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _defaultVectorLayerMode;
    }

    void CartoVisLoader::setDefaultVectorLayerMode(bool vectorLayerMode) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _defaultVectorLayerMode = vectorLayerMode;
    }

    bool CartoVisLoader::isStrictMode() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _strictMode;
    }
    
    void CartoVisLoader::setStrictMode(bool strictMode) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _strictMode = strictMode;
    }

    std::shared_ptr<AssetPackage> CartoVisLoader::getVectorTileAssetPackage() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _vectorTileAssetPackage;
    }

    void CartoVisLoader::setVectorTileAssetPackage(const std::shared_ptr<AssetPackage>& assetPackage) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _vectorTileAssetPackage = assetPackage;
    }

    void CartoVisLoader::loadVis(const std::shared_ptr<CartoVisBuilder>& builder, const std::string& visURL) const {
        if (!builder) {
            throw NullArgumentException("Null builder");
        }

        std::lock_guard<std::recursive_mutex> lock(_mutex);

        HTTPClient client(Log::IsShowDebug());
        std::shared_ptr<BinaryData> responseData;
        std::map<std::string, std::string> responseHeaders;
        if (client.get(visURL, std::map<std::string, std::string>(), responseHeaders, responseData) != 0) {
            std::string result;
            if (responseData) {
                result = std::string(reinterpret_cast<const char*>(responseData->data()), responseData->size());
            }
            throw GenericException("Failed to read visJSON configuration", result);
        }

        // Read VisJSON
        std::string result(reinterpret_cast<const char*>(responseData->data()), responseData->size());
        picojson::value visJSON;
        std::string err = picojson::parse(visJSON, result);
        if (!err.empty()) {
            throw ParseException("Failed to parse visJSON configuration", result);
        }

        // Set default username
        _defaultUsername.clear();
        std::string hostName = NetworkUtils::ParseURLHostName(visURL);
        std::string::size_type hostSepPos = hostName.find('.');
        if (hostSepPos != std::string::npos) {
            _defaultUsername = hostName.substr(0, hostSepPos);
        }
        std::string urlPath = NetworkUtils::ParseURLPath(visURL);
        if (urlPath.substr(0, 3) == "/u/") {
            std::string::size_type urlPathPos = urlPath.find('/', 3);
            if (urlPathPos != std::string::npos) {
                _defaultUsername = urlPath.substr(3, urlPathPos - 3);
            }
        }

        // Base options
        if (auto center = getMapPos(visJSON.get("center"))) {
            builder->setCenter(*center);
        }

        if (auto zoom = getDouble(visJSON.get("zoom"))) {
            builder->setZoom(static_cast<float>(*zoom));
        }

        if (auto bounds = getMapBounds(visJSON.get("bounds"))) {
            builder->setBounds(*bounds);
        }

        builder->setDescription(Variant::FromPicoJSON(visJSON));

        // Configue layers
        const picojson::value& layersOption = visJSON.get("layers");
        if (layersOption.is<picojson::array>()) {
            const picojson::array& layerConfigs = layersOption.get<picojson::array>();

            // Build layer orders and sort
            std::vector<std::pair<int, std::size_t> > layerOrders;
            for (const picojson::value& layerConfig : layerConfigs) {
                int order = static_cast<int>(layerOrders.size());
                if (auto orderOpt = getInt(layerConfig.get("order"))) {
                    order = *orderOpt;
                }
                layerOrders.emplace_back(order, layerOrders.size());
            }

            std::sort(layerOrders.begin(), layerOrders.end());

            // Create layers
            for (std::size_t i = 0; i < layerOrders.size(); i++) {
                createLayers(builder, layerConfigs[layerOrders[i].second]);
            }
        }
    }

    void CartoVisLoader::readLayerAttributes(picojson::object& attributes, const picojson::value& options) {
        if (auto name = getString(options.get("name"))) {
            attributes["name"] = picojson::value(*name);
        }
        if (auto attribution = getString(options.get("attribution"))) {
            attributes["attribution"] = picojson::value(*attribution);
        }
    }

    void CartoVisLoader::configureMapsService(CartoMapsService& mapsService, const picojson::value& options) const {
        if (auto userName = getString(options.get("user_name"))) {
            mapsService.setUsername(*userName);
        }
        else {
            mapsService.setUsername(_defaultUsername);
        }

        if (auto mapsAPITemplate = getString(options.get("maps_api_template"))) {
            mapsService.setAPITemplate(*mapsAPITemplate);
        }

        if (auto statTag = getString(options.get("stat_tag"))) {
            mapsService.setStatTag(*statTag);
        }

        if (auto filter = getString(options.get("filter"))) {
            std::vector<std::string> filterList;
            boost::split(filterList, *filter, boost::is_any_of(","));
            mapsService.setLayerFilter(filterList);
        }

        std::string tilerProtocol = DEFAULT_TILER_PROTOCOL;
        if (auto tilerProtocolOpt = getString(options.get("tiler_protocol"))) {
            tilerProtocol = *tilerProtocolOpt;
        }
        std::string tilerDomain = DEFAULT_TILER_DOMAIN;
        if (auto tilerDomainOpt = getString(options.get("tiler_domain"))) {
            tilerDomain = *tilerDomainOpt;
        }
        std::string tilerPort = tilerProtocol == "https" ? "443" : "80";
        if (auto tilerPortOpt = getString(options.get("tiler_port"))) {
            tilerPort = *tilerPortOpt;
        }
        mapsService.setTilerURL(tilerProtocol + "://" + (options.contains("user_name") ? "{user}." : "") + tilerDomain + ":" + tilerPort);

        const picojson::value& cdnURLsOption = options.get("cdn_url");
        if (cdnURLsOption.is<picojson::object>()) {
            const picojson::object cdnURLs = cdnURLsOption.get<picojson::object>();

            std::map<std::string, std::string> urls;
            for (auto it = cdnURLs.begin(); it != cdnURLs.end(); it++) {
                if (auto url = getString(it->second)) {
                    urls[it->first] = *url;
                }
            }
            mapsService.setCDNURLs(urls);
        }

        mapsService.setDefaultVectorLayerMode(_defaultVectorLayerMode);
        mapsService.setStrictMode(_strictMode);
        mapsService.setVectorTileAssetPackage(_vectorTileAssetPackage);
    }
    
    void CartoVisLoader::configureLayerInteractivity(Layer& layer, const picojson::value& options) const {
        if (!options.is<picojson::null>()) {
            if (auto tileLayer = dynamic_cast<TileLayer*>(&layer)) {
                std::shared_ptr<TileDataSource> dataSource = tileLayer->getDataSource();
                while (auto cacheDataSource = std::dynamic_pointer_cast<CacheTileDataSource>(dataSource)) {
                    dataSource = cacheDataSource->getDataSource();
                }
                if (auto httpDataSource = std::dynamic_pointer_cast<HTTPTileDataSource>(dataSource)) {
                    std::string baseURL = httpDataSource->getBaseURL();
                    std::string::size_type pos = baseURL.rfind('.');
                    if (pos != std::string::npos) {
                        baseURL = baseURL.substr(0, pos);
                    }
                    
                    auto gridDataSource = std::make_shared<HTTPTileDataSource>(dataSource->getMinZoom(), dataSource->getMaxZoom(), baseURL + ".grid.json");
                    tileLayer->setUTFGridDataSource(gridDataSource);
                }
            }
        }
    }
    
    void CartoVisLoader::createLayers(const std::shared_ptr<CartoVisBuilder>& builder, const picojson::value& layerConfig) const {
        std::string type;
        if (auto typeOpt = getString(layerConfig.get("type"))) {
            type = boost::algorithm::to_lower_copy(*typeOpt);
        }
        picojson::value options = picojson::value(picojson::object());
        if (layerConfig.get("options").is<picojson::object>()) {
            options = layerConfig.get("options");
        }

        // Create layer based on type and options
        std::vector<LayerInfo> layerInfos;
        if (type == "tiled") {
            if (auto layerInfo = createTiledLayer(options)) {
                layerInfos.push_back(*layerInfo);
            }
        }
        else if (type == "background") {
            if (auto layerInfo = createBackgroundLayer(options)) {
                layerInfos.push_back(*layerInfo);
            }
        }
        else if (type == "torque" || type == "cartodb" || type == "carto") {
            if (auto layerInfo = createCartoLayer(type, options, layerConfig.get("legend"))) {
                layerInfos.push_back(*layerInfo);
            }
        }
        else if (type == "namedmap") {
            layerInfos = createNamedLayers(options);
        }
        else if (type == "layergroup") {
            layerInfos = createLayerGroup(options, layerConfig.get("infowindow"));
        }
        else {
            Log::Warnf("CartoVisLoader::createLayers: Unsupported layer type: %s", type.c_str());
        }

        // Final layer configuration
        for (const LayerInfo& layerInfo : layerInfos) {
            if (auto visible = getBool(layerConfig.get("visible"))) {
                layerInfo.layer->setVisible(*visible);
            }

            builder->addLayer(layerInfo.layer, Variant::FromPicoJSON(picojson::value(layerInfo.attributes)));
        }
    }

    boost::optional<CartoVisLoader::LayerInfo> CartoVisLoader::createTiledLayer(const picojson::value& options) const {
        // Create data source, based on URL template and min/max zoom
        std::string urlTemplate = *getString(options.get("urlTemplate"));
        boost::optional<int> minZoom = getInt(options.get("minZoom"));
        boost::optional<int> maxZoom = getInt(options.get("maxZoom"));
        auto dataSource = std::make_shared<HTTPTileDataSource>(minZoom ? *minZoom : 0, maxZoom ? *maxZoom : Const::MAX_SUPPORTED_ZOOM_LEVEL, urlTemplate);
        
        // Set subdomains for {s} tag
        std::vector<std::string> subdomains = { "a", "b", "c" };
        if (options.get("subdomains").is<picojson::array>()) {
            const picojson::array& subdomainsOption = options.get("subdomains").get<picojson::array>();
            subdomains.clear();
            std::transform(subdomainsOption.begin(), subdomainsOption.end(), std::back_inserter(subdomains), [](const picojson::value& subdomain) {
                return *getString(subdomain);
            });
        }
        else if (options.get("subdomains").is<std::string>()) {
            const std::string& subdomainsOption = options.get("subdomains").get<std::string>();
            subdomains.clear();
            std::transform(subdomainsOption.begin(), subdomainsOption.end(), std::back_inserter(subdomains), [](char subdomain) {
                return std::string(1, subdomain);
            });
        }
        dataSource->setSubdomains(subdomains);

        // TMS scheme flag
        auto tms = getBool(options.get("tms"));
        dataSource->setTMSScheme(tms ? *tms : false);

        // Create raster layer
        auto rasterLayer = std::make_shared<RasterTileLayer>(dataSource);
        picojson::object attributes;
        readLayerAttributes(attributes, options);
        return LayerInfo(rasterLayer, attributes);
    }

    boost::optional<CartoVisLoader::LayerInfo> CartoVisLoader::createBackgroundLayer(const picojson::value& options) const {
        // Read color
        Color color(255, 255, 255, 255);
        if (options.get("color").is<std::string>()) {
            unsigned int value = 0;
            if (mvt::parseCSSColor(options.get("color").get<std::string>(), value)) {
                color = Color(value);
            }
            else {
                Log::Warn("CartoVisLoader::createBackgroundLayer: Failed to parse CSS color");
            }
        }

        // Read optional image
        std::shared_ptr<Bitmap> image;
        if (options.get("image").is<std::string>()) {
            std::shared_ptr<BinaryData> responseData;
            if (NetworkUtils::GetHTTP(options.get("image").get<std::string>(), responseData, true)) {
                image = Bitmap::CreateFromCompressed(responseData);
            }
            else {
                Log::Warn("CartoVisLoader::createBackgroundLayer: Failed to load background image");
            }
        }

        // Create solid layer
        auto solidLayer = std::make_shared<SolidLayer>(color);
        solidLayer->setBitmap(image);

        picojson::object attributes;
        readLayerAttributes(attributes, options);
        return LayerInfo(solidLayer, attributes);
    }

    boost::optional<CartoVisLoader::LayerInfo> CartoVisLoader::createCartoLayer(const std::string& type, const picojson::value& options, const picojson::value& legend) const {
        std::vector<std::shared_ptr<Layer> > layers;

        if (options.get("named_map").is<picojson::object>()) {
            // Using a layer from a named map
            const picojson::value& namedMap = options.get("named_map");

            // Read name, layer index and parameters
            std::string name = *getString(namedMap.get("name"));
            int layerIndex = 0;
            if (auto layerIndexOpt = getInt(namedMap.get("layer_index"))) {
                layerIndex = *layerIndexOpt;
            }

            std::map<std::string, Variant> params;
            const picojson::object& paramConfig = namedMap.get("params").get<picojson::object>();
            for (auto it = paramConfig.begin(); it != paramConfig.end(); it++) {
                params[it->first] = Variant::FromPicoJSON(it->second);
            }

            // Configure map service and get the layers
            CartoMapsService mapsService;
            configureMapsService(mapsService, options);
            mapsService.setLayerIndices(std::vector<int>{ layerIndex });
            layers = mapsService.buildNamedMap(name, params);
        }
        else {
            // Build map config for Maps service
            std::map<std::string, Variant> layerOptions;

            if (auto tableName = getString(options.get("table_name"))) {
                layerOptions["table_name"] = Variant(*tableName);
            }
            else if (auto layerName = getString(options.get("layer_name"))) {
                layerOptions["table_name"] = Variant(*layerName);
            }

            layerOptions["sql"] = Variant(std::string("SELECT * FROM ") + layerOptions["table_name"].getString());
            if (auto sql = getString(options.get("sql"))) {
                layerOptions["sql"] = Variant(*sql);
            }
            else if (auto query = getString(options.get("query"))) {
                layerOptions["sql"] = Variant(*query);
            }

            if (auto cartoCSS = getString(options.get("cartocss"))) {
                layerOptions["cartocss"] = Variant(*cartoCSS);
            }
            else if (auto tileStyle = getString(options.get("tile_style"))) {
                layerOptions["cartocss"] = Variant(*tileStyle);
            }
            layerOptions["cartocss_version"] = Variant(std::string("2.1.1"));
            if (auto cartoCSSVersion = getString(options.get("cartocss_version"))) {
                layerOptions["cartocss_version"] = Variant(*cartoCSSVersion);
            }

            if (auto source = getString(options.get("source"))) {
                layerOptions["source"] = Variant(*source);
            }

            std::map<std::string, Variant> layerConfig;
            layerConfig["type"] = Variant(type);
            layerConfig["options"] = Variant(layerOptions);

            std::map<std::string, Variant> mapConfig;
            mapConfig["version"] = Variant(std::string("1.5.0"));
            mapConfig["layers"] = Variant(std::vector<Variant> { Variant(layerConfig) });
            
            CartoMapsService mapsService;
            configureMapsService(mapsService, options);
            layers = mapsService.buildMap(Variant(mapConfig));
        }

        if (layers.empty()) {
            return boost::optional<LayerInfo>();
        }

        // Create attributes for the layer
        picojson::object attributes;
        readLayerAttributes(attributes, options);
        if (!legend.is<picojson::null>()) {
            attributes["legend"] = legend;
        }
        
        // Create layer info
        return LayerInfo(layers.front(), attributes);
    }

    std::vector<CartoVisLoader::LayerInfo> CartoVisLoader::createNamedLayers(const picojson::value& options) const {
        const picojson::value& namedMap = options.get("named_map");

        // Read name and parameters
        std::string name = *getString(namedMap.get("name"));
        
        std::map<std::string, Variant> params;
        const picojson::object& paramConfig = namedMap.get("params").get<picojson::object>();
        for (auto it = paramConfig.begin(); it != paramConfig.end(); it++) {
            params[it->first] = Variant::FromPicoJSON(it->second);
        }

        // Configue Maps service and get the layers
        CartoMapsService mapsService;
        configureMapsService(mapsService, options);
        if (auto statTag = getString(namedMap.get("stat_tag"))) {
            mapsService.setStatTag(*statTag);
        }
        std::vector<std::shared_ptr<Layer> > layers = mapsService.buildNamedMap(name, params);
        return createLayerInfos(layers, options, namedMap);
    }

    std::vector<CartoVisLoader::LayerInfo> CartoVisLoader::createLayerGroup(const picojson::value& options, const picojson::value& infoWindow) const {
        picojson::value layerDefinition = options.get("layer_definition");
        
        // Manually reset layer definition
        layerDefinition.get("version") = picojson::value(std::string("1.5.0"));

        // Translate layer types
        picojson::array& layersOption = layerDefinition.get("layers").get<picojson::array>();
        for (picojson::value& layerOption : layersOption) {
            std::string type = boost::to_lower_copy(layerOption.get("type").get<std::string>());
            if (type == "cartodb") {
                type = "mapnik";
            }
            layerOption.get("type").get<std::string>() = type;
        }

        // Configue Maps service and get the layers
        CartoMapsService mapsService;
        configureMapsService(mapsService, options);
        std::vector<std::shared_ptr<Layer> > layers = mapsService.buildMap(Variant::FromPicoJSON(layerDefinition));
        return createLayerInfos(layers, options, layerDefinition);
    }

    std::vector<CartoVisLoader::LayerInfo> CartoVisLoader::createLayerInfos(const std::vector<std::shared_ptr<Layer> >& layers, const picojson::value& options, const picojson::value& layerDefinition) const {
        // Create layer attributes
        std::map<std::shared_ptr<Layer>, picojson::object> layerAttributes;
        if (layerDefinition.get("layers").is<picojson::array>()) {
            const picojson::array& layerConfigs = layerDefinition.get("layers").get<picojson::array>();
            for (std::size_t i = 0; i < layerConfigs.size(); i++) {
                const picojson::value& layerConfig = layerConfigs[i];
                std::string layerId = *getString(layerConfig.get("id"));

                std::shared_ptr<Layer> layer;
                int subLayerIndex = -1;
                {
                    std::size_t remainder = i;
                    for (std::size_t j = 0; j < layers.size(); j++) {
                        if (auto vectorTileLayer = std::dynamic_pointer_cast<VectorTileLayer>(layers[j])) {
                            if (auto cartoVectorTileDecoder = std::dynamic_pointer_cast<CartoVectorTileDecoder>(vectorTileLayer->getTileDecoder())) {
                                if (remainder < cartoVectorTileDecoder->getLayerIds().size()) {
                                    subLayerIndex = remainder;
                                    layer = vectorTileLayer;
                                    break;
                                }
                                remainder -= cartoVectorTileDecoder->getLayerIds().size();
                                continue;
                            }
                        }
                        if (remainder == 0) {
                            layer = layers[j];
                            break;
                        }
                        remainder--;
                    }
                }
                if (!layer) {
                    break;
                }

                if (subLayerIndex < 0) {
                    configureLayerInteractivity(*layer, layerConfig.get("interactivity"));
                }

                if (auto visible = getBool(layerConfig.get("visible"))) {
                    if (subLayerIndex < 0) {
                        layer->setVisible(*visible);
                    }
                    else {
                        auto vectorTileLayer = std::dynamic_pointer_cast<VectorTileLayer>(layer);
                        auto cartoVectorTileDecoder = std::dynamic_pointer_cast<CartoVectorTileDecoder>(vectorTileLayer->getTileDecoder());
                        cartoVectorTileDecoder->setLayerVisible(layerId, *visible);
                    }
                }
                
                picojson::object attributes;
                readLayerAttributes(attributes, options);
                if (layerConfig.contains("infowindow")) {
                    attributes["infowindow"] = layerConfig.get("infowindow");
                }
                if (layerConfig.contains("legend")) {
                    attributes["legend"] = layerConfig.get("legend");
                }
                if (layerConfig.contains("layer_name")) {
                    attributes["name"] = layerConfig.get("layer_name");
                }
                if (subLayerIndex < 0) {
                    layerAttributes[layer] = attributes;
                }
                else {
                    picojson::array groupAttributes;
                    if (layerAttributes[layer]["sublayers"].is<picojson::array>()) {
                        groupAttributes = layerAttributes[layer]["sublayers"].get<picojson::array>();
                    }
                    groupAttributes.push_back(picojson::value(attributes));
                    layerAttributes[layer]["sublayers"] = picojson::value(groupAttributes);
                }
            }
        }

        // Create layer info list
        std::vector<LayerInfo> layerInfos;
        for (const std::shared_ptr<Layer>& layer : layers) {
            layerInfos.push_back(LayerInfo(layer, layerAttributes[layer]));
        }
        return layerInfos;
    }

    const std::string CartoVisLoader::DEFAULT_TILER_PROTOCOL = "http";
    const std::string CartoVisLoader::DEFAULT_TILER_DOMAIN = "carto.com";

}
