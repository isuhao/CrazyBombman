//
//  Explosion.cpp
//  CrazyBombman
//
//  Created by Yao Melo on 6/23/13.
//
//

#include "Explosion.h"
#include "Simulation.h"
#include "TileUtils.h"
#include "ArtworkLoader.h"
#include "Environment.h"
#include "PhysicsUtil.h"


namespace Simulation
{
    Explosion::Explosion():_animateNodes(),_range(INITIAL_EXPLOSION_RANGE),_isFinished(false),_destroyMapcoords()
    {
        
    }
    
    Explosion::~Explosion()
    {
        
    }
    
    void Explosion::update(float dt)
    {
        bool isFinished = true;
        for (std::vector<AnimatedNode>::iterator it=_animateNodes.begin(); it!=_animateNodes.end();) {
            cocos2d::CCNode* node =  it->getNode();
            cocos2d::CCAction *anim = it->getAction();
            if(!it->isAnimated && anim && node)
            {
                node->setVisible(true);
                node->runAction(anim);
                it->isAnimated = true;
                isFinished = false;
            }
            else if(anim && anim->isDone())
            {
                node->getParent()->removeChild(node);
                it = _animateNodes.erase(it);
                continue;
            }
            else
            {
                isFinished = false;
            }
            it++;
        }
        _isFinished = isFinished;
        
    }
    
    bool Explosion::init()
    {
        return true;
    }
    
    
    Material getTileMaterial(cocos2d::CCTMXTiledMap *tileMap,cocos2d::CCTMXLayer *layer, cocos2d::CCPoint const& p, cocos2d::CCPoint& outputMapCoord)
    {
        cocos2d::CCSize layerSize = layer->getLayerSize();
       //check why 3,0 is at last of the list
        outputMapCoord = Utility::GetMapCoords(tileMap, p);
        if(outputMapCoord.x<0 || outputMapCoord.y < 0 || outputMapCoord.x>= layerSize.width || outputMapCoord.y >=layerSize.height)
        {
            return eSolid;
        }
        int gid = layer->tileGIDAt(outputMapCoord);
        if(gid)
        {
            cocos2d::CCDictionary* properties = tileMap->propertiesForGID(gid);
            if(properties)
            {
                cocos2d::CCObject *val = properties->objectForKey(TILE_MAP_MATERIAL_KEY);
                if(val)
                {
                    int material = static_cast<cocos2d::CCString*>(val) ->intValue();
                    return static_cast<Material>(material);
                }
            }
        }
        return ePlain;
    }

    void Explosion::createNodesAt(cocos2d::CCPoint const& center, cocos2d::CCTMXTiledMap *tileMap)
    {
        using namespace cocos2d;
        _center = center;
        CCSprite *explode = CCSprite::create();
        explode->setPosition(center);
        explode->setVisible(false);
        CCAnimation *animation = Utility::ArtworkLoader::explosionAnimation();
        
        CCAnimate* action = CCAnimate::create(animation);
        AnimatedNode node;
        node.setAction(action);
        node.setNode(explode);
        _animateNodes.push_back(node);
        cocos2d::CCTMXLayer *blockLayer = tileMap->layerNamed(TILE_MAP_MATERIAL_LAYER);
        cocos2d::CCPoint mapCoord;
        for(int d = 0;d < 4;d++)
        {//4 direction.
            CCPoint p = center;
            float* property = (d%2) == 0? &(p.x) : &(p.y);
            int sign = (d/2) == 0? -1:1;
            
            for (int radius = 1,end = (_range- 16)/16; radius <= end; radius ++) {
                (*property)+= (sign * 16);
                if(radius%2 == 1)
                {
                    Material mat = getTileMaterial(tileMap, blockLayer, p,mapCoord);
                    printf("material for tile:(%d,%d) is %d\n",(int)mapCoord.x,(int)mapCoord.y,mat);
                    if(mat == eSolid)
                    {
                        break;
                    }
                    else if(mat == eDestroyable || mat == eNonBlock)
                    {
                        _destroyMapcoords.push_back(mapCoord);
                    }
                }
                CCAnimate* action = CCAnimate::create(animation);
                CCSequence* seq = CCSequence::create(CCDelayTime::create(radius*0.05),action,NULL);
                
                CCSprite* expSprite = CCSprite::create();
                expSprite -> setPosition(p);
                expSprite->setVisible(false);
                node.setAction(seq);
                node.setNode(expSprite);
                _animateNodes.push_back(node);
            }
       
        }
    }
    

    
    void Explosion::destroyBlocks(cocos2d::CCTMXTiledMap *tileMap, cocos2d::CCArray *tileBodyArr,b2World* world)
    {
        using namespace cocos2d;
        CCTMXLayer *layer = tileMap->layerNamed(TILE_MAP_MATERIAL_LAYER);
        CCTMXLayer *blockLayer = tileMap->layerNamed(TILE_MAP_BLOCKS_LAYER);
        printf("destroy size:%ld\n",_destroyMapcoords.size());
        for (std::vector<CCPoint>::iterator it = _destroyMapcoords.begin(); it < _destroyMapcoords.end(); ++it)
        {
            CCSprite *tile = blockLayer ->tileAt(*it);
            if(tile)
            {
                printf("destory tile(%d,%d) with animation, tile addr:%d\n",(int)(*it).x,(int)(*it).y, reinterpret_cast<unsigned int>(tile));


                blockLayer->removeTileAt(*it);
                layer->removeTileAt(*it);
                for(int i=tileBodyArr->count()-1;i>=0;--i)
                {
                    TileInfo *physObj = static_cast<TileInfo*>(tileBodyArr->objectAtIndex(i));
                    if(physObj->mapcoord.equals(*it))
                    {                        
                        physObj->clearBody(world);
                        tileBodyArr->removeObjectAtIndex(i);
                    }

                }
            }
        }
        _destroyMapcoords.clear();
    }
    
    
    
    unsigned int Explosion::getNodesCount()
    {
        return _animateNodes.size();
    }
    
    cocos2d::CCNode* Explosion::getNodeAt(unsigned int index)
    {
        return _animateNodes[index].getNode();
    }
    
    bool Explosion::isFinished()
    {
        return _isFinished;
    }
    
    PhysicalType Explosion::getPhysicalType()
    {
        return PhysExplosion;
    }
    
  
    
    b2Body* Explosion::createBody(b2World *world)
    {
        
        b2PolygonShape shape;
        float size = EXPLOSION_MAX_COLLIDE_SIZE;
        float hs = 0.5*size;
        CCRect rect;
        rect.setRect(_center.x - hs, _center.y+hs, size, _range);
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position = CCPoint2Vec(_center);
        b2Body* body = world->CreateBody(&def);
        Utility::AddFixturesFilled(body, rect, _center);
        rect.setRect(_center.x - hs, _center.y-hs - _range, size, _range);
        Utility::AddFixturesFilled(body, rect, _center);
        rect.setRect(_center.x + hs, _center.y-hs , _range, size);
        Utility::AddFixturesFilled(body, rect, _center);
        rect.setRect(_center.x - hs-_range, _center.y-hs, _range, size);
        Utility::AddFixturesFilled(body, rect, _center);
        return body;
    }
    
    void Explosion::collideWith(Simulation::PhysicsObject *other)
    {
        
    }
    
    bool Explosion::acceptCollide(Simulation::PhysicsObject *other)
    {
        return false;
    }
}