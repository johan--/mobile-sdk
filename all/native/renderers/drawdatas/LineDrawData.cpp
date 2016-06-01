#include "LineDrawData.h"
#include "core/MapPos.h"
#include "core/MapVec.h"
#include "geometry/LineGeometry.h"
#include "graphics/Bitmap.h"
#include "projections/Projection.h"
#include "styles/LineStyle.h"
#include "vectorelements/Line.h"
#include "vectorelements/Polygon.h"
#include "utils/Const.h"
#include "utils/GLUtils.h"
#include "utils/Log.h"

#include <cmath>
#include <unordered_map>

namespace carto {

    LineDrawData::LineDrawData(const LineGeometry& geometry, const LineStyle& style, const Projection& projection) :
        VectorElementDrawData(style.getColor()),
        _bitmap(style.getBitmap()),
        _clickScale(style.getClickWidth() == -1 ? std::max(1.0f, 1 + (IDEAL_CLICK_WIDTH - style.getWidth()) * CLICK_WIDTH_COEF / style.getWidth()) : style.getClickWidth()),
        _poses(),
        _coords(),
        _normals(),
        _texCoords(),
        _indices()
    {
        // Remove consecutive duplicates and project coordinates to internal coordinate system
        const std::vector<MapPos>& poses = geometry.getPoses();
        _poses.reserve(poses.size());
        for (size_t i = 0; i < poses.size(); i++) {
            MapPos pos = projection.toInternal(poses[i]);
            if (i == 0 || _poses.back() != pos) {
                _poses.push_back(pos);
            }
        }
        
        // Init draw data
        init(style);
    }
    
    LineDrawData::LineDrawData(const PolygonGeometry& geometry, const std::vector<MapPos>& internalPoses, const LineStyle& style, const Projection& projection) :
        VectorElementDrawData(style.getColor()),
        _bitmap(style.getBitmap()),
        _clickScale(std::max(1.0f, 1 + (IDEAL_CLICK_WIDTH - style.getWidth()) * CLICK_WIDTH_COEF / style.getWidth())),
        _poses(),
        _coords(),
        _normals(),
        _texCoords(),
        _indices()
    {
        // Remove consecutive duplicates, do NOT project coordinates
        _poses.reserve(internalPoses.size());
        for (size_t i = 0; i < internalPoses.size(); i++) {
            const MapPos& pos = internalPoses[i];
            if (i == 0 || _poses.back() != pos) {
                _poses.push_back(pos);
            }
        }
        if (!internalPoses.empty()) {
            const MapPos& pos = internalPoses.front();
            if (_poses.back() != pos) {
                _poses.push_back(pos);
            }
        }
        
        // Init draw data
        init(style);
    }
        
    LineDrawData::LineDrawData(const LineDrawData& lineDrawData) :
        VectorElementDrawData(lineDrawData.getColor()),
        _bitmap(lineDrawData._bitmap),
        _clickScale(lineDrawData._clickScale),
        _poses(lineDrawData._poses),
        _coords(),
        _normals(lineDrawData._normals),
        _texCoords(lineDrawData._texCoords),
        _indices(lineDrawData._indices)
    {
        // Recalculate _coord pointers to _poses
        _coords.reserve(lineDrawData._coords.size());
        for (const std::vector<MapPos*>& coords : lineDrawData._coords) {
            _coords.push_back(std::vector<MapPos*>());
            _coords.back().reserve(coords.size());
            const MapPos* prevPos = nullptr;
            int index = -1;
            for (const MapPos* pos : coords) {
                if (pos != prevPos) {
                    prevPos = pos;
                    index++;
                }
                _coords.back().push_back(&_poses[index]);
            }
        }
    }
    
    LineDrawData::~LineDrawData() {
    }
    
    const std::shared_ptr<Bitmap> LineDrawData::getBitmap() const {
        return _bitmap;
    }
    
    float LineDrawData::getClickScale() const {
        return _clickScale;
    }
    
    const std::vector<std::vector<MapPos*> >& LineDrawData::getCoords() const {
        return _coords;
    }
    
    const std::vector<std::vector<cglib::vec2<float> > >& LineDrawData::getNormals() const {
        return _normals;
    }
    
    const std::vector<std::vector<cglib::vec2<float> > >& LineDrawData::getTexCoords() const {
        return _texCoords;
    }
    
    const std::vector<std::vector<unsigned int> >& LineDrawData::getIndices() const {
        return _indices;
    }
    
    void LineDrawData::offsetHorizontally(double offset) {
        for (MapPos& pos : _poses) {
            pos.setX(pos.getX() + offset);
        }
        setIsOffset(true);
    }
    
    void LineDrawData::init(const LineStyle& style) {
        if (_poses.size() < 2) {
            _coords.clear();
            _normals.clear();
            _texCoords.clear();
            _indices.clear();
            return;
        }
    
        // Detect looped line
        bool loopedLine = (_poses.front() == _poses.back()) && (_poses.size() > 2);
    
        // Calculate angles between lines and buffers sizes
        std::size_t coordCount = (_poses.size() - 1) * 4;
        std::size_t indexCount = (_poses.size() - 1) * 6;
        std::vector<float> deltaAngles(_poses.size() - 1);
        float prevAngle = 0;
        if (style.getLineJointType() != LineJointType::LINE_JOINT_TYPE_NONE) {
            for (std::size_t i = 0; i < _poses.size(); i++) {
                if (!loopedLine && i + 1 >= _poses.size()) {
                    break;
                }
    
                const MapPos& pos = _poses[i];
                const MapPos& nextPos = (i + 1 <  _poses.size()) ? _poses[i + 1] : _poses[1];
                MapVec nextLine(nextPos - pos);
                float nextAngle = std::atan2(nextLine.getY(), nextLine.getX()) * Const::RAD_TO_DEG - 90;
    
                if (i > 0) {
                    float deltaAngle = std::fmod((nextAngle - prevAngle + 360.0f), 360.0f);
                    if (deltaAngle >= 180) {
                        deltaAngle -= 360;
                    }
                    deltaAngles[i - 1] = deltaAngle;
    
                    int segments = 0;
                    if (style.getLineJointType() == LineJointType::LINE_JOINT_TYPE_STRAIGHT) {
                        segments = deltaAngle != 0 ? 1 : 0;
                    } else { //style.getLineJoinType() == LineJoinType::ROUND
                        segments = static_cast<int>(std::ceil(std::abs(deltaAngle) * style.getWidth() * LINE_ENDPOINT_TESSELATION_FACTOR));
                    }
    
                    coordCount += segments;
                    indexCount += segments * 3;
                }
    
                prevAngle = nextAngle;
            }
        }
    
        // Endpoints need a bigger buffer
        if (!loopedLine && style.getLineEndType() == LineEndType::LINE_END_TYPE_ROUND) {
            int segments = static_cast<int>(180 * style.getWidth() * LINE_ENDPOINT_TESSELATION_FACTOR);
            coordCount += segments * 2;
            indexCount += segments * 3 * 2;
        }
    
        _coords.reserve(coordCount);
        _normals.reserve(indexCount);
        _texCoords.reserve(coordCount);
        _indices.reserve(indexCount);
    
        // Texture bounds
        float texCoordX = 1.0f;
        float texCoordY = 0.0f;
        float texCoordYScale = 1.0f / (style.getStretchFactor() * _bitmap->getHeight());
        bool useTexCoordY = _bitmap->getHeight() > 1;
    
        // Instead of calculating actual vertex positions calculate vertex origins and normals
        // Actual vertex positions are view dependent and will be calculated in the renderer
        std::vector<MapPos*> coords;
        std::vector<cglib::vec2<float> > normals;
        std::vector<cglib::vec2<float> > texCoords;
        std::vector<unsigned int> indices;
        MapVec firstLine;
        MapVec lastLine;
        MapPos* prevPos = nullptr;
        unsigned int vertexIndex = 0;
        for (std::size_t i = 0; i < _poses.size(); i++) {
            MapPos& pos = _poses[i];
            if (prevPos) {
                // Calculate line body
                MapVec prevLine(pos - *prevPos);
                if (i == 1) {
                    firstLine = prevLine;
                }
                if (i == _poses.size() - 1) {
                    lastLine = prevLine;
                }
                float prevLineLength = prevLine.length();
                prevLine /= prevLineLength;
                MapVec prevPerpVec1(-prevLine.getY(), prevLine.getX(), 0);
                MapVec prevPerpVec2(prevLine.getY(), -prevLine.getX(), 0);
                prevPerpVec1 *= style.getWidth() / 2;
                prevPerpVec2 *= style.getWidth() / 2;
                
                // Add line vertices, normals and indices
                coords.push_back(prevPos);
                coords.push_back(prevPos);
                coords.push_back(&pos);
                coords.push_back(&pos);
                
                if (useTexCoordY) {
                    float texCoordYOffset = prevLineLength * texCoordYScale;
                    texCoords.push_back(cglib::vec2<float>(0, texCoordY));
                    texCoords.push_back(cglib::vec2<float>(texCoordX, texCoordY));
                    texCoords.push_back(cglib::vec2<float>(0, texCoordY + texCoordYOffset));
                    texCoords.push_back(cglib::vec2<float>(texCoordX, texCoordY + texCoordYOffset));
                    texCoordY += texCoordYOffset;
                } else {
                    texCoords.push_back(cglib::vec2<float>(0, 0));
                    texCoords.push_back(cglib::vec2<float>(texCoordX, 0));
                    texCoords.push_back(cglib::vec2<float>(0, 1));
                    texCoords.push_back(cglib::vec2<float>(texCoordX, 1));
                }
                
                normals.push_back(cglib::vec2<float>(prevPerpVec1.getX(), prevPerpVec1.getY()));
                normals.push_back(cglib::vec2<float>(prevPerpVec2.getX(), prevPerpVec2.getY()));
                normals.push_back(cglib::vec2<float>(prevPerpVec1.getX(), prevPerpVec1.getY()));
                normals.push_back(cglib::vec2<float>(prevPerpVec2.getX(), prevPerpVec2.getY()));
                
                indices.push_back(vertexIndex + 0);
                indices.push_back(vertexIndex + 1);
                indices.push_back(vertexIndex + 2);
                indices.push_back(vertexIndex + 1);
                indices.push_back(vertexIndex + 3);
                indices.push_back(vertexIndex + 2);
                
                vertexIndex += 4;
                
                // Calculate line joints, if necessary
                if ((i + 1 <  _poses.size() || loopedLine) && style.getLineJointType() != LineJointType::LINE_JOINT_TYPE_NONE) {
                    float deltaAngle = deltaAngles[i - 1];
                    
                    int segments = 0;
                    if (style.getLineJointType() == LineJointType::LINE_JOINT_TYPE_STRAIGHT) {
                        segments = deltaAngle != 0 ? 1 : 0;
                    } else { //style.getLineJoinType() == LineJoinType::ROUND
                        segments = static_cast<int>(std::ceil(std::abs(deltaAngle) * style.getWidth() * LINE_JOINT_TESSELATION_FACTOR));
                    }
                    if (segments > 0) {
                        float segmentDeltaAngle = deltaAngle / segments;
                        float sin = std::sin(segmentDeltaAngle * Const::DEG_TO_RAD);
                        float cos = std::cos(segmentDeltaAngle * Const::DEG_TO_RAD);
                        
                        // Add the t vertex
                        coords.push_back(&pos);
                        normals.push_back(cglib::vec2<float>(0, 0));
                        texCoords.push_back(cglib::vec2<float>(0.5f, texCoordY));
                        
                        // Add vertices and normals, do not create double vertices anywhere
                        bool leftTurn = (deltaAngle <= 0);
                        MapVec rotVec(leftTurn ? prevPerpVec1 : prevPerpVec2);
                        for (int j = 0; j < segments - 1; j++) {
                            rotVec.rotate2D(sin, cos);
                            coords.push_back(&pos);
                            normals.push_back(cglib::vec2<float>(rotVec.getX(), rotVec.getY()));
                            texCoords.push_back(cglib::vec2<float>(leftTurn ? 0 : 1, texCoordY));
                        }
                        
                        // Add indices, make use of existing and future line's vertices
                        if (deltaAngle <= 0) {
                            for (int j = 0; j < segments; j++) {
                                indices.push_back(vertexIndex);
                                if (j == segments - 1) {
                                    indices.push_back((i == _poses.size() - 1) ? 0 : (vertexIndex + j + 1));
                                } else {
                                    indices.push_back(vertexIndex + j + 1);
                                }
                                indices.push_back((j == 0) ? vertexIndex - 2 : (vertexIndex + j));
                            }
                        } else {
                            for (int j = 0; j < segments; j++) {
                                indices.push_back(vertexIndex);
                                indices.push_back((j == 0) ? vertexIndex - 1 : (vertexIndex + j));
                                if (j == segments - 1) {
                                    indices.push_back((i == _poses.size() - 1) ? 1 : (vertexIndex + j + 2));
                                } else {
                                    indices.push_back(vertexIndex + j + 1);
                                }
                            }
                        }
                        
                        vertexIndex += segments;
                    }
                }
            }
    
            prevPos = &pos;
        }
        
        // Calculate line end points
        if (!loopedLine && style.getLineEndType() == LineEndType::LINE_END_TYPE_ROUND) {
            int segments = static_cast<int>(180 * style.getWidth() * LINE_ENDPOINT_TESSELATION_FACTOR);
            if (segments > 0) {
                float segmentDeltaAngle = 180 / segments;
                float sin = std::sin(segmentDeltaAngle * Const::DEG_TO_RAD);
                float cos = std::cos(segmentDeltaAngle * Const::DEG_TO_RAD);
                
                // Last end point, prevLine contains the last valid line segment
                MapVec rotVec(lastLine.getNormalized());
                rotVec.setCoords(rotVec.getY(), -rotVec.getX());
                rotVec *= style.getWidth() / 2;
                MapVec uvRotVec(-1, 0);
                
                // Add the t vertex
                coords.push_back(&_poses[_poses.size() - 1]);
                normals.push_back(cglib::vec2<float>(0, 0));
                texCoords.push_back(cglib::vec2<float>(0.5f, texCoordY));
                
                // Vertices
                for (int i = 0; i < segments - 1; i++) {
                    rotVec.rotate2D(sin, cos);
                    uvRotVec.rotate2D(sin, cos);
                    coords.push_back(&_poses[_poses.size() - 1]);
                    normals.push_back(cglib::vec2<float>(rotVec.getX(), rotVec.getY()));
                    texCoords.push_back(cglib::vec2<float>(uvRotVec.getX() * 0.5f + 0.5f, texCoordY));
                }
                
                // Indices
                for (int j = 0; j < segments; j++) {
                    indices.push_back(vertexIndex);
                    indices.push_back((j == 0) ? vertexIndex - 1 : (vertexIndex + j));
                    indices.push_back((j == segments - 1) ? vertexIndex - 2 : (vertexIndex + j + 1));
                }
                vertexIndex += segments;
                
                // First end point, firstLine contains the first valid line segment
                rotVec = firstLine.getNormalized();
                rotVec.setCoords(-rotVec.getY(), rotVec.getX());
                rotVec *= style.getWidth() / 2;
                uvRotVec = MapVec(1, 0);
                
                // Add the t vertex
                coords.push_back(&_poses[0]);
                normals.push_back(cglib::vec2<float>(0, 0));
                texCoords.push_back(cglib::vec2<float>(0.5f, 0));
                
                // Vertices
                for (int i = 0; i < segments - 1; i++) {
                    rotVec.rotate2D(sin, cos);
                    uvRotVec.rotate2D(sin, cos);
                    coords.push_back(&_poses[0]);
                    normals.push_back(cglib::vec2<float>(rotVec.getX(), rotVec.getY()));
                    texCoords.push_back(cglib::vec2<float>(uvRotVec.getX() * 0.5f + 0.5f, 0));
                }
                
                // Indices
                for (int j = 0; j < segments; j++) {
                    indices.push_back(vertexIndex);
                    indices.push_back((j == 0) ? 0 : (vertexIndex + j));
                    indices.push_back((j == segments - 1) ? 1 : (vertexIndex + j + 1));
                }
                vertexIndex += segments;
    
            }
        }
        
        _coords.push_back(std::vector<MapPos*>());
        _normals.push_back(std::vector<cglib::vec2<float> >());
        _texCoords.push_back(std::vector<cglib::vec2<float> >());
        _indices.push_back(std::vector<unsigned int>());
        if (indices.size() <= GLUtils::MAX_VERTEXBUFFER_SIZE) {
            _coords.back().swap(coords);
            _normals.back().swap(normals);
            _texCoords.back().swap(texCoords);
            _indices.back().swap(indices);
        } else {
            // Buffers too big, split into multiple buffers
            _coords.back().reserve(std::min(coords.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
            _normals.back().reserve(std::min(normals.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
            _texCoords.back().reserve(std::min(texCoords.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
            _indices.back().reserve(std::min(indices.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
            std::unordered_map<unsigned int, unsigned int> indexMap;
            for (size_t i = 0; i < indices.size(); i += 3) {
                
                // Check for possible GL buffer overflow
                if (_indices.back().size() + 3 > GLUtils::MAX_VERTEXBUFFER_SIZE) {
                    // The buffer is full, create a new one
                    _coords.back().shrink_to_fit();
                    _coords.push_back(std::vector<MapPos*>());
                    _coords.back().reserve(std::min(coords.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
                    _normals.back().shrink_to_fit();
                    _normals.push_back(std::vector<cglib::vec2<float> >());
                    _normals.back().reserve(std::min(normals.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
                    _texCoords.back().shrink_to_fit();
                    _texCoords.push_back(std::vector<cglib::vec2<float> >());
                    _texCoords.back().reserve(std::min(texCoords.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
                    _indices.back().shrink_to_fit();
                    _indices.push_back(std::vector<unsigned int>());
                    _indices.back().reserve(std::min(indices.size(), GLUtils::MAX_VERTEXBUFFER_SIZE));
                    indexMap.clear();
                }
                
                for (int j = 0; j < 3; j++) {
                    unsigned int index = static_cast<unsigned int>(indices[i + j]);
                    auto it = indexMap.find(index);
                    if (it == indexMap.end()) {
                        unsigned int newIndex = static_cast<unsigned int>(_coords.back().size());
                        _coords.back().push_back(coords[index]);
                        _normals.back().push_back(normals[index]);
                        _texCoords.back().push_back(texCoords[index]);
                        _indices.back().push_back(newIndex);
                        indexMap[index] = newIndex;
                    } else {
                        _indices.back().push_back(it->second);
                    }
                }
            }
        }
        
        _coords.back().shrink_to_fit();
        _normals.back().shrink_to_fit();
        _texCoords.back().shrink_to_fit();
        _indices.back().shrink_to_fit();
    }
    
    const float LineDrawData::LINE_ENDPOINT_TESSELATION_FACTOR = 0.004f;
    const float LineDrawData::LINE_JOINT_TESSELATION_FACTOR = 0.0018f;
    
    const float LineDrawData::CLICK_WIDTH_COEF = 0.5f;
    
}
