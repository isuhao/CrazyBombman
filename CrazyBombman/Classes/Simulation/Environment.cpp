//
//  Environment.cpp
//  CrazyBomber
//
//  Created by Yao Melo on 6/18/13.
//
//

#include "Environment.h"
#include "Player.h"
#include "TileUtils.h"
#include "PhysicsUtil.h"
#include "Explosion.h"
#include "AttachInfo.h"

using namespace cocos2d;

namespace Simulation
{
    
    Environment* Environment::create(SceneLevelParams const& slp)
    {
        Environment* env = new Environment(slp);
        if(env->init())
        {
            env->autorelease();
            return env;
        }
        else
        {
            CC_SAFE_DELETE(env);
            return NULL;
        }
    }
    
    void Environment::spawnPlayer()
    {
        Player* player = Player::create();
        player->createNode();
        CCTMXObjectGroup *group = _tileMap->objectGroupNamed(TILE_MAP_OBJ_LAYER);
        CCDictionary *spawn = group->objectNamed(TILE_MAP_SPAWN_OBJ);
        float x = ((CCString*)spawn->objectForKey("x"))->floatValue();
        float y = ((CCString*)spawn->objectForKey("y"))->floatValue();
        //sample 357,800
        CCPoint p = ccp(x,y);
        player->getNode()->setPosition(p);
        _ppDelegate->addNode(player->getNode(),Z_PLAYER);
        _ppDelegate->updatePlayerPostion(p);
        setPlayer(player);
    }
    
    bool Environment::init()
    {
        _bombs = CCArray::create();
        _bombs->retain();
        _explosions = CCArray::create();
        _explosions->retain();
        _mobsSystem = MobsSystem::create();
        _mobsSystem->retain();
        _mobsSystem->setCollisionDetector(this);
        _world = new b2World(b2Vec2(0.0,0.0));
        _world->SetAllowSleeping(true);
        return true;
    }
    
    void Environment::setTileMap(CCTMXTiledMap *tileMap)
    {
        if(tileMap!=_tileMap)
        {
            CC_SAFE_RETAIN(tileMap);
            CC_SAFE_RELEASE(_tileMap);
            _tileMap = tileMap;
 
        }
    }
    
    void Environment::buildPhysicalMap(CCTMXTiledMap *tilemap)
    {
        CCTMXLayer* layer = tilemap->layerNamed(TILE_MAP_MATERIAL_LAYER);
        CCSize size = layer->getLayerSize();
        CCPoint mapcoord;
        for (int i = 0; i<size.height; ++i) {
            for (int j = 0; j<size.width; ++j) {
                mapcoord.x = j;
                mapcoord.y = i;
                int gid = layer->tileGIDAt(mapcoord);
                if (gid) {
                    CCDictionary* dic = tilemap->propertiesForGID(gid);
                    CCString* matStr =(CCString*)dic->objectForKey(TILE_MAP_MATERIAL_KEY);
                    if (matStr) {
                        int mat = matStr->intValue();
                        if(mat == eSolid || mat == eDestroyable || mat == eNonBlock)
                        {
                            CCRect tileRect;
                            tileRect.size = layer->getMapTileSize();
                            tileRect.origin = layer->positionAt(mapcoord);
                            b2Body* body = Utility::CreateBodyForRect(_world, tileRect);
                            TileInfo* tileInfo = new TileInfo();
                            tileInfo->mapcoord = mapcoord;
                            tileInfo->material = static_cast<Material>(mat);
                            AttachInfo *ai = new AttachInfo(AttachTile,tileInfo);
                            body->SetUserData(ai);
                            
                        }
                    }
                }
            }
        }
        
    }
    
    void Environment::update(float dt)
    {
        CCObject::update(dt);
        
        updateBombs(dt);
        updateExplosions(dt);
        CCPoint p = _player->getPlayerPosition();
        _player ->update(dt);
        CCPoint newP = _player->getPlayerPosition();
        if(!p.equals(newP))
        {//collision happen, back to the old position.
            if(checkCollision(newP, p))
            {
                _player->getNode()->setPosition(newP);
            }
            
            if(_ppDelegate)
            {
                _ppDelegate -> updatePlayerPostion(newP);
            }
        }
        updateMob(dt);
    }
    
    void Environment::updateExplosions(float dt)
    {
        std::vector<int> indecies;
        for (int i = 0;i<_explosions->count();i++)
        {
            Explosion* exp = (Explosion*)_explosions->objectAtIndex(i);
            exp->update(dt);
            if(exp->isFinished())
            {
                indecies.push_back(i);
            }
        }
        for(std::vector<int>::iterator it = indecies.begin();it!=indecies.end();it++)
        {
            _explosions->removeObjectAtIndex(*it);
        }
    }
    
    void Environment::updateBombs(float dt)
    {
        std::vector<int> indecies;
        for (int i = 0;i<_bombs->count();i++) {
            Bomb* bomb = (Bomb*)(_bombs->objectAtIndex(i));

            bomb->update(dt);
            if(bomb->isExplode())
            {
                //FIXME add explosion
                //remove bomb node
                CCNode* bombNode = bomb->getNode();
                bombNode->getParent()->removeChild(bombNode);
                indecies.push_back(i);
                Explosion *exp = Explosion::create();
                exp->createNodesAt(bombNode->getPosition(),_tileMap);
                exp->destroyBlocks(_tileMap);
                _explosions->addObject(exp);
                for(int i =0;i<exp->getNodesCount();i++)
                {
                    _ppDelegate->addNode(exp->getNodeAt(i), Z_EXPLOSION);
                }
            }
        }
        for(std::vector<int>::iterator it = indecies.begin();it!=indecies.end();it++)
        {
            _bombs->removeObjectAtIndex(*it);
        }
    }
    
    void Environment::updateMob(float dt)
    {
        _mobsSystem->update(dt);
        _mobsSystem->updateMobsOnMap(_tileMap, _ppDelegate);
    }
    
    bool testTileCollidable(CCTMXTiledMap *tilemap,CCPoint const& mapCoord,CCRect* tileRect)
    {
        CCTMXLayer *blocks = tilemap->layerNamed(TILE_MAP_MATERIAL_LAYER);
        CCSize mapSize = blocks->getLayerSize();
        if(mapCoord.x<0||mapCoord.x>=mapSize.width || mapCoord.y<0 || mapCoord.y>=mapSize.height)
        {
            if(tileRect)
            {
                *tileRect = CCRectZero;
            }
            //collide with map border.
            return true;
        }
        int gid = blocks->tileGIDAt(mapCoord);
        

        if(gid)
        {
            CCDictionary *dictionary = tilemap->propertiesForGID(gid);
            CCString* mat = (CCString*)dictionary->objectForKey(TILE_MAP_MATERIAL_KEY);
            if(mat)
            {
                Material material = static_cast<Material>(mat->intValue());
                if(material == eDestroyable || material == eSolid)
                {
                    if(tileRect)
                    {
                        *tileRect = Utility::GetBoundingBox(blocks->tileAt(mapCoord));
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool Environment::checkMoveCollision(cocos2d::CCPoint& dest,cocos2d::CCPoint const& start,cocos2d::CCSize const& subjectSize)
    {
        CCPoint dir = dest- start;
        CCPoint aspectVec = ccp(subjectSize.width/2,subjectSize.height/2);
        float dot = dir.dot(aspectVec);
        dir = dir * (fabs(dot)/dir.getLengthSq());
        CCPoint contactVec;
        bool collide = Utility::TestSegCollision(_tileMap, dest, dest+dir, testTileCollidable,&contactVec);
        if (collide) {
            if(!contactVec.equals(CCPointZero))
            {
                dest = dest+contactVec;
            }
            else
            {
                dest = start;
            }
        }
        return collide;
    }
    
    bool Environment::checkCollision(CCPoint& p, CCPoint const& prevP)
    {
        return this->checkMoveCollision(p, prevP, _player->getNode()->getContentSize());
    }
    
    
    void Environment::addBomb(Simulation::Bomb *bomb)
    {
        CCObject *b;
        CCARRAY_FOREACH(_bombs, b)
        {
            if(((Bomb*)b)->getNode()->getPosition().equals(bomb->getNode()->getPosition()))
            {
                return;
            }
        }
        _bombs->addObject(bomb);
        _ppDelegate->addNode(bomb->getNode(), Z_BOMB);
    }
    
    Environment::~Environment()
    {
        CC_SAFE_RELEASE(_player);
        CC_SAFE_RELEASE(_bombs);
        CC_SAFE_RELEASE(_explosions);
        if(_world)
        {
            int bodycount = _world->GetBodyCount();
            int i = 0;
            for (b2Body* b=_world->GetBodyList(); i < bodycount; ++i,++b) {
                AttachInfo* info = static_cast<AttachInfo*>(b->GetUserData());
                b->SetUserData(NULL);
                CC_SAFE_RELEASE(info->userObj);
                delete info;
                _world->DestroyBody(b);
            }
            delete _world;
        }
    }
    
}