/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TILERENDERER_H_
#define _CARTO_TILERENDERER_H_

#include "graphics/Color.h"
#include "graphics/ViewState.h"
#include "vt/TileId.h"
#include "vt/Tile.h"

#include <memory>
#include <mutex>
#include <map>
#include <vector>

namespace carto {
    class MapPos;
    class Projection;
    class Shader;
    class ShaderManager;
    class TextureManager;
    class TileDrawData;
    class MapRenderer;
    class ViewState;
    namespace vt {
        class GLTileRenderer;
    }
    
    class TileRenderer : public std::enable_shared_from_this<TileRenderer> {
    public:
        TileRenderer(const std::weak_ptr<MapRenderer>& mapRenderer, bool useFBO, bool useDepth, bool useStencil);
        virtual ~TileRenderer();
    
        void setLabelOrder(int labelOrder);

        void offsetLayerHorizontally(double offset);
    
        void onSurfaceCreated(const std::shared_ptr<ShaderManager>& shaderManager, const std::shared_ptr<TextureManager>& textureManager);
        bool onDrawFrame(float deltaSeconds, const ViewState& viewState);
        bool onDrawFrame3D(float deltaSeconds, const ViewState& viewState);
        void onSurfaceDestroyed();
    
        void setBackgroundColor(const Color& color);
        void setBackgroundPattern(const std::shared_ptr<const vt::BitmapPattern>& pattern);
        bool cullLabels(const ViewState& viewState);
        bool refreshTiles(const std::vector<std::shared_ptr<TileDrawData> >& drawDatas);
    
    private:
        std::weak_ptr<MapRenderer> _mapRenderer;
        std::shared_ptr<vt::GLTileRenderer> _glRenderer;
        std::shared_ptr<std::mutex> _glRendererMutex;
        bool _useFBO;
        bool _useDepth;
        bool _useStencil;
        int _labelOrder;
        double _horizontalLayerOffset;
        std::map<vt::TileId, std::shared_ptr<const vt::Tile> > _tiles;

        mutable std::mutex _mutex;
    };
    
}

#endif
