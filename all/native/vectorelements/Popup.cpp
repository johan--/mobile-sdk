#include "Popup.h"
#include "styles/PopupStyle.h"
#include "renderers/drawdatas/PopupDrawData.h"

namespace carto {
    
    Popup::Popup(const std::shared_ptr<Billboard>& baseBillboard, const std::shared_ptr<PopupStyle>& style) :
        Billboard(baseBillboard),
        _anchorPointX(0),
        _anchorPointY(-1),
        _style(style)
    {
    }
    
    Popup::Popup(const std::shared_ptr<Geometry>& geometry, const std::shared_ptr<PopupStyle>& style) :
        Billboard(geometry),
        _anchorPointX(0),
        _anchorPointY(-1),
        _style(style)
    {
    }
    
    Popup::Popup(const MapPos& pos, const std::shared_ptr<PopupStyle>& style) :
        Billboard(pos),
        _anchorPointX(0),
        _anchorPointY(-1),
        _style(style)
    {
    }
    
    Popup::~Popup() {
    }
        
    float Popup::getAnchorPointX() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _anchorPointX;
    }
    
    float Popup::getAnchorPointY() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _anchorPointY;
    }
    
    void Popup::setAnchorPoint(float anchorPointX, float anchorPointY) {
        std::lock_guard<std::mutex> lock(_mutex);
        _anchorPointX = anchorPointX;
        _anchorPointY = anchorPointY;
    }
    
    std::shared_ptr<PopupStyle> Popup::getStyle() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _style;
    }
    
    void Popup::setStyle(const std::shared_ptr<PopupStyle>& style) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _style = style;
        }
        notifyElementChanged();
    }
    
}
