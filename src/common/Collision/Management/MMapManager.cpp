/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "MMapManager.h"
#include "Errors.h"
#include "Log.h"
#include "MMapDefines.h"

namespace MMAP
{
    constexpr char MAP_FILE_NAME_FORMAT[] = "{}mmaps/{:03}.mmap";
    constexpr char TILE_FILE_NAME_FORMAT[] = "{}mmaps/{:03}{:02}{:02}.mmtile";

    // ######################## MMapManager ########################
    MMapManager::~MMapManager()
    {
        for (std::pair<uint32 const, MMapData*>& loadedMMap : loadedMMaps)
            delete loadedMMap.second;

        // by now we should not have maps loaded
        // if we had, tiles in MMapData->mmapLoadedTiles, their actual data is lost!
    }

    void MMapManager::InitializeThreadUnsafe(const std::vector<uint32>& mapIds)
    {
        // the caller must pass the list of all mapIds that will be used in the VMapManager2 lifetime
        for (uint32 const& mapId : mapIds)
            loadedMMaps.insert(MMapDataSet::value_type(mapId, nullptr));

        thread_safe_environment = false;
    }

    MMapDataSet::const_iterator MMapManager::GetMMapData(uint32 mapId) const
    {
        // return the iterator if found or end() if not found/NULL
        MMapDataSet::const_iterator itr = loadedMMaps.find(mapId);
        if (itr != loadedMMaps.cend() && !itr->second)
            itr = loadedMMaps.cend();

        return itr;
    }

    bool MMapManager::loadMapData(std::string const& basePath, uint32 mapId)
    {
        // we already have this map loaded?
        MMapDataSet::iterator itr = loadedMMaps.find(mapId);
        if (itr != loadedMMaps.end())
        {
            if (itr->second)
                return true;
        }
        else
        {
            if (thread_safe_environment)
                itr = loadedMMaps.insert(MMapDataSet::value_type(mapId, nullptr)).first;
            else
                ABORT_MSG("Invalid mapId %u passed to MMapManager after startup in thread unsafe environment", mapId);
        }

        // load and init dtNavMesh - read parameters from file
        std::string fileName = Trinity::StringFormat(MAP_FILE_NAME_FORMAT, basePath, mapId);
        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            TC_LOG_DEBUG("maps", "MMAP:loadMapData: Error: Could not open mmap file '{}'", fileName);
            return false;
        }

        dtNavMeshParams params;
        uint32 count = uint32(fread(&params, sizeof(dtNavMeshParams), 1, file));
        fclose(file);
        if (count != 1)
        {
            TC_LOG_DEBUG("maps", "MMAP:loadMapData: Error: Could not read params from file '{}'", fileName);
            return false;
        }

        dtNavMesh* mesh = dtAllocNavMesh();
        ASSERT(mesh);
        if (dtStatusFailed(mesh->init(&params)))
        {
            dtFreeNavMesh(mesh);
            TC_LOG_ERROR("maps", "MMAP:loadMapData: Failed to initialize dtNavMesh for mmap {:03} from file {}", mapId, fileName);
            return false;
        }

        TC_LOG_DEBUG("maps", "MMAP:loadMapData: Loaded {:03}.mmap", mapId);

        // store inside our map list
        MMapData* mmap_data = new MMapData(mesh);

        itr->second = mmap_data;
        return true;
    }

    uint32 MMapManager::packTileID(int32 x, int32 y)
    {
        return uint32(x << 16 | y);
    }

    bool MMapManager::loadMap(std::string const& basePath, uint32 mapId, int32 x, int32 y)
    {
        // make sure the mmap is loaded and ready to load tiles
        if (!loadMapData(basePath, mapId))
            return false;

        // get this mmap data
        MMapData* mmap = loadedMMaps[mapId];
        ASSERT(mmap->navMesh);

        // check if we already have this tile loaded
        uint32 packedGridPos = packTileID(x, y);
        if (mmap->loadedTileRefs.find(packedGridPos) != mmap->loadedTileRefs.end())
            return false;

        // load this tile :: mmaps/MMMXXYY.mmtile
        std::string fileName = Trinity::StringFormat(TILE_FILE_NAME_FORMAT, basePath, mapId, x, y);
        FILE* file = fopen(fileName.c_str(), "rb");
        if (!file)
        {
            TC_LOG_DEBUG("maps", "MMAP:loadMap: Could not open mmtile file '{}'", fileName);
            return false;
        }

        // read header
        MmapTileHeader fileHeader;
        if (fread(&fileHeader, sizeof(MmapTileHeader), 1, file) != 1 || fileHeader.mmapMagic != MMAP_MAGIC)
        {
            TC_LOG_ERROR("maps", "MMAP:loadMap: Bad header in mmap {:03}{:02}{:02}.mmtile", mapId, x, y);
            fclose(file);
            return false;
        }

        if (fileHeader.mmapVersion != MMAP_VERSION)
        {
            TC_LOG_ERROR("maps", "MMAP:loadMap: {:03}{:02}{:02}.mmtile was built with generator v{}, expected v{}",
                mapId, x, y, fileHeader.mmapVersion, MMAP_VERSION);
            fclose(file);
            return false;
        }

        long pos = ftell(file);
        fseek(file, 0, SEEK_END);
        if (pos < 0 || static_cast<int32>(fileHeader.size) > ftell(file) - pos)
        {
            TC_LOG_ERROR("maps", "MMAP:loadMap: {:03}{:02}{:02}.mmtile has corrupted data size", mapId, x, y);
            fclose(file);
            return false;
        }

        fseek(file, pos, SEEK_SET);

        unsigned char* data = (unsigned char*)dtAlloc(fileHeader.size, DT_ALLOC_PERM);
        ASSERT(data);

        size_t result = fread(data, fileHeader.size, 1, file);
        if (!result)
        {
            TC_LOG_ERROR("maps", "MMAP:loadMap: Bad header or data in mmap {:03}{:02}{:02}.mmtile", mapId, x, y);
            fclose(file);
            return false;
        }

        fclose(file);

        dtMeshHeader* header = (dtMeshHeader*)data;
        dtTileRef tileRef = 0;

        // memory allocated for data is now managed by detour, and will be deallocated when the tile is removed
        if (dtStatusSucceed(mmap->navMesh->addTile(data, fileHeader.size, DT_TILE_FREE_DATA, 0, &tileRef)))
        {
            mmap->loadedTileRefs.insert(std::pair<uint32, dtTileRef>(packedGridPos, tileRef));
            ++loadedTiles;
            TC_LOG_DEBUG("maps", "MMAP:loadMap: Loaded mmtile {:03}[{:02}, {:02}] into {:03}[{:02}, {:02}]", mapId, x, y, mapId, header->x, header->y);
            return true;
        }
        else
        {
            TC_LOG_ERROR("maps", "MMAP:loadMap: Could not load {:03}{:02}{:02}.mmtile into navmesh", mapId, x, y);
            dtFree(data);
            return false;
        }
    }

    bool MMapManager::loadMapInstance(std::string const& basePath, uint32 mapId, uint32 instanceId)
    {
        if (!loadMapData(basePath, mapId))
            return false;

        MMapData* mmap = loadedMMaps[mapId];
        auto [queryItr, inserted] = mmap->navMeshQueries.try_emplace(instanceId, nullptr);
        if (!inserted)
            return true;

        // allocate mesh query
        dtNavMeshQuery* query = dtAllocNavMeshQuery();
        ASSERT(query);
        if (dtStatusFailed(query->init(mmap->navMesh, 1024)))
        {
            dtFreeNavMeshQuery(query);
            mmap->navMeshQueries.erase(queryItr);
            TC_LOG_ERROR("maps", "MMAP:GetNavMeshQuery: Failed to initialize dtNavMeshQuery for mapId {:03} instanceId {}", mapId, instanceId);
            return false;
        }

        TC_LOG_DEBUG("maps", "MMAP:GetNavMeshQuery: created dtNavMeshQuery for mapId {:03} instanceId {}", mapId, instanceId);
        queryItr->second = query;
        return true;
    }

    bool MMapManager::unloadMap(uint32 mapId, int32 x, int32 y)
    {
        // check if we have this map loaded
        MMapDataSet::const_iterator itr = GetMMapData(mapId);
        if (itr == loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            TC_LOG_DEBUG("maps", "MMAP:unloadMap: Asked to unload not loaded navmesh map. {:03}{:02}{:02}.mmtile", mapId, x, y);
            return false;
        }

        MMapData* mmap = itr->second;

        // check if we have this tile loaded
        uint32 packedGridPos = packTileID(x, y);
        if (mmap->loadedTileRefs.find(packedGridPos) == mmap->loadedTileRefs.end())
        {
            // file may not exist, therefore not loaded
            TC_LOG_DEBUG("maps", "MMAP:unloadMap: Asked to unload not loaded navmesh tile. {:03}{:02}{:02}.mmtile", mapId, x, y);
            return false;
        }

        dtTileRef tileRef = mmap->loadedTileRefs[packedGridPos];

        // unload, and mark as non loaded
        if (dtStatusFailed(mmap->navMesh->removeTile(tileRef, nullptr, nullptr)))
        {
            // this is technically a memory leak
            // if the grid is later reloaded, dtNavMesh::addTile will return error but no extra memory is used
            // we cannot recover from this error - assert out
            TC_LOG_ERROR("maps", "MMAP:unloadMap: Could not unload {:03}{:02}{:02}.mmtile from navmesh", mapId, x, y);
            ABORT();
        }
        else
        {
            mmap->loadedTileRefs.erase(packedGridPos);
            --loadedTiles;
            TC_LOG_DEBUG("maps", "MMAP:unloadMap: Unloaded mmtile {:03}[{:02}, {:02}] from {:03}", mapId, x, y, mapId);
            return true;
        }

        return false;
    }

    bool MMapManager::unloadMap(uint32 mapId)
    {
        MMapDataSet::iterator itr = loadedMMaps.find(mapId);
        if (itr == loadedMMaps.end() || !itr->second)
        {
            // file may not exist, therefore not loaded
            TC_LOG_DEBUG("maps", "MMAP:unloadMap: Asked to unload not loaded navmesh map {:03}", mapId);
            return false;
        }

        // unload all tiles from given map
        MMapData* mmap = itr->second;
        for (MMapTileSet::iterator i = mmap->loadedTileRefs.begin(); i != mmap->loadedTileRefs.end(); ++i)
        {
            uint32 x = (i->first >> 16);
            uint32 y = (i->first & 0x0000FFFF);
            if (dtStatusFailed(mmap->navMesh->removeTile(i->second, nullptr, nullptr)))
                TC_LOG_ERROR("maps", "MMAP:unloadMap: Could not unload {:03}{:02}{:02}.mmtile from navmesh", mapId, x, y);
            else
            {
                --loadedTiles;
                TC_LOG_DEBUG("maps", "MMAP:unloadMap: Unloaded mmtile {:03}[{:02}, {:02}] from {:03}", mapId, x, y, mapId);
            }
        }

        delete mmap;
        itr->second = nullptr;
        TC_LOG_DEBUG("maps", "MMAP:unloadMap: Unloaded {:03}.mmap", mapId);

        return true;
    }

    bool MMapManager::unloadMapInstance(uint32 mapId, uint32 instanceId)
    {
        // check if we have this map loaded
        MMapDataSet::const_iterator itr = GetMMapData(mapId);
        if (itr == loadedMMaps.end())
        {
            // file may not exist, therefore not loaded
            TC_LOG_DEBUG("maps", "MMAP:unloadMapInstance: Asked to unload not loaded navmesh map {:03}", mapId);
            return false;
        }

        MMapData* mmap = itr->second;
        auto queryItr = mmap->navMeshQueries.find(instanceId);
        if (queryItr == mmap->navMeshQueries.end())
        {
            TC_LOG_DEBUG("maps", "MMAP:unloadMapInstance: Asked to unload not loaded dtNavMeshQuery mapId {:03} instanceId {}", mapId, instanceId);
            return false;
        }

        dtFreeNavMeshQuery(queryItr->second);
        mmap->navMeshQueries.erase(queryItr);
        TC_LOG_DEBUG("maps", "MMAP:unloadMapInstance: Unloaded mapId {:03} instanceId {}", mapId, instanceId);

        return true;
    }

    dtNavMesh const* MMapManager::GetNavMesh(uint32 mapId)
    {
        MMapDataSet::const_iterator itr = GetMMapData(mapId);
        if (itr == loadedMMaps.end())
            return nullptr;

        return itr->second->navMesh;
    }

    dtNavMeshQuery const* MMapManager::GetNavMeshQuery(uint32 mapId, uint32 instanceId)
    {
        auto itr = GetMMapData(mapId);
        if (itr == loadedMMaps.end())
            return nullptr;

        auto queryItr = itr->second->navMeshQueries.find(instanceId);
        if (queryItr == itr->second->navMeshQueries.end())
            return nullptr;

        return queryItr->second;
    }
}
