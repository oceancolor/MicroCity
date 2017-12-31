#include "Game.h"
#include "Draw.h"
#include "Interface.h"
#include "Font.h"
#include "Strings.h"

const uint8_t TileImageData[] PROGMEM = 
{ 
  #include "TileData.h"
//  0, 0, 0, 0, 0, 0, 0, 0,
//  0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 
};

// Currently visible tiles are cached so they don't need to be recalculated between frames
uint8_t VisibleTileCache[VISIBLE_TILES_X * VISIBLE_TILES_Y];
int8_t CachedScrollX, CachedScrollY;
uint8_t AnimationFrame = 0;

const uint8_t* GetTileData(uint8_t tile)
{
	return TileImageData + (tile * 8);
}

bool HasHighTraffic(int x, int y)
{
  // First check for buildings
  for(int n = 0; n < MAX_BUILDINGS; n++)
  {
    Building* building = &State.buildings[n];
    
    if(building->type && building->heavyTraffic)
    {
      if(x < building->x - 1 || y < building->y - 1)
        continue;
      const BuildingInfo* info = GetBuildingInfo(building->type);
      uint8_t width = pgm_read_byte(&info->width);
      uint8_t height = pgm_read_byte(&info->height);
      if(x > building->x + width || y > building->y + height)
        continue;

      return true;
    }
  }  

  return false;
}

// Calculate which visible tile to use
uint8_t CalculateTile(int x, int y)
{
	// Check out of range
	if(x < 0 || y < 0 || x >= MAP_WIDTH || y >= MAP_HEIGHT)
		return 0;
	
	// First check for buildings
	for(int n = 0; n < MAX_BUILDINGS; n++)
	{
		Building* building = &State.buildings[n];
		
		if(building->type)
		{
			if(x < building->x || y < building->y)
				continue;
			const BuildingInfo* info = GetBuildingInfo(building->type);
			uint8_t width = pgm_read_byte(&info->width);
			uint8_t height = pgm_read_byte(&info->height);
      uint8_t tile = pgm_read_byte(&info->drawTile);
			if(x < building->x + width && y < building->y + height)
			{
        if(building->type == Industrial || building->type == Commercial || building->type == Residential)
        {
          if(building->populationDensity >= MAX_POPULATION_DENSITY - 1)
          {
            tile += 48;
          }
          else if(x != building->x + 1 || y != building->y + 1)
          {
            uint16_t density = GetRandFromSeed(y * MAP_WIDTH + x);
          }
        }
      
        tile += (y - building->y) * 16;
        tile += (x - building->x);
				return tile;
			}
		}
	}
	
	// Next check for roads / powerlines
	uint8_t connections = GetConnections(x, y);
	
	if(connections == RoadMask)
	{
		int variant = GetConnectivityTileVariant(x, y, connections);
    // todo: traffic
    if(HasHighTraffic(x, y))
      return FIRST_ROAD_TRAFFIC_TILE + variant;
		return FIRST_ROAD_TILE + variant;
	}
	else if(connections == PowerlineMask)
	{
		int variant = GetConnectivityTileVariant(x, y, connections);
		return FIRST_POWERLINE_TILE + variant;
	}
	else if(connections == (PowerlineMask | RoadMask))
	{
    int variant = GetConnectivityTileVariant(x, y, RoadMask) & 1;
		return FIRST_POWERLINE_ROAD_TILE + variant;
	}
	
	return GetTerrainTile(x, y);
}

inline uint8_t GetCachedTile(int x, int y)
{
  //return CalculateTile(CachedScrollX + x, CachedScrollY + y);
	uint8_t tile = VisibleTileCache[y * VISIBLE_TILES_X + x];

  if(tile >= FIRST_WATER_TILE && tile <= LAST_WATER_TILE)
  {
    tile = FIRST_WATER_TILE + ((tile - FIRST_WATER_TILE + (AnimationFrame >> 1)) & 3);
  }

  if((AnimationFrame & 4) && tile >= FIRST_ROAD_TRAFFIC_TILE && tile <= LAST_ROAD_TRAFFIC_TILE)
  {
    tile += 16;
  }
  
  return tile;
}

void ResetVisibleTileCache()
{
	CachedScrollX = UIState.scrollX >> TILE_SIZE_SHIFT;
	CachedScrollY = UIState.scrollY >> TILE_SIZE_SHIFT;
	
	for(int y = 0; y < VISIBLE_TILES_Y; y++)
	{
		for(int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
	}
}

/*
// TODO: optimise loop to use the system's internal display format to write out a whole byte at a time
void DrawTiles()
{
	int tileY = 0;
	int offsetY = UIState.scrollY & (TILE_SIZE - 1);
	
	for(int row = 0; row < DISPLAY_HEIGHT; row++)
	{
		int tileX = 0;
		int offsetX = UIState.scrollX & (TILE_SIZE - 1); 
		uint8_t currentTile = GetCachedTile(tileX, tileY);
		uint8_t readBuf = pgm_read_byte(&GetTileData(currentTile)[offsetY]);
		readBuf >>= offsetX;
		
		for(int col = 0; col < DISPLAY_WIDTH; col++)
		{
			PutPixel(col, row, readBuf & 1);
			
			offsetX = (offsetX + 1) & 7;
			readBuf >>= 1;

			if(!offsetX)
			{
				tileX++;
				currentTile = GetCachedTile(tileX, tileY);
				readBuf = pgm_read_byte(&GetTileData(currentTile)[offsetY]);
			}
		}
		
		offsetY = (offsetY + 1) & 7;
		if(!offsetY)
		{
			tileY++;
		}
	}
}*/

void DrawTiles()
{
  int tileX = 0;
  int offsetX = UIState.scrollX & (TILE_SIZE - 1); 
  
  for(int col = 0; col < DISPLAY_WIDTH; col++)
  {
    int tileY = 0;
    int offsetY = UIState.scrollY & (TILE_SIZE - 1);
    uint8_t currentTile = GetCachedTile(tileX, tileY);
    uint8_t readBuf = pgm_read_byte(&GetTileData(currentTile)[offsetX]);
    readBuf >>= offsetY;
    
    for(int row = 0; row < DISPLAY_HEIGHT; row++)
    {
      PutPixel(col, row, readBuf & 1);
      
      offsetY = (offsetY + 1) & 7;
      readBuf >>= 1;

      if(!offsetY)
      {
        tileY++;
        currentTile = GetCachedTile(tileX, tileY);
        readBuf = pgm_read_byte(&GetTileData(currentTile)[offsetX]);
      }
    }
    
    offsetX = (offsetX + 1) & 7;
    if(!offsetX)
    {
      tileX++;
    }
  }
}

void ScrollUp(int amount)
{
	CachedScrollY -= amount;
	int y = VISIBLE_TILES_Y - 1;
	
	for(int n = 0; n < VISIBLE_TILES_Y - amount; n++)
	{
		for(int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[(y - amount) * VISIBLE_TILES_X + x];
		}
		y--;
	}
	
	for(y = 0; y < amount; y++)
	{
		for(int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
	}
}

void ScrollDown(int amount)
{
	CachedScrollY += amount;
	int y = 0;
	
	for(int n = 0; n < VISIBLE_TILES_Y - amount; n++)
	{
		for(int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[(y + amount) * VISIBLE_TILES_X + x];
		}
		y++;
	}
	
	y = VISIBLE_TILES_Y - 1;
	for(int n = 0; n < amount; n++)
	{
		for(int x = 0; x < VISIBLE_TILES_X; x++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
		y--;
	}
}

void ScrollLeft(int amount)
{
	CachedScrollX -= amount;
	int x = VISIBLE_TILES_X - 1;
	
	for(int n = 0; n < VISIBLE_TILES_X - amount; n++)
	{
		for(int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[y * VISIBLE_TILES_X + x - amount];
		}
		x--;
	}
	
	for(x = 0; x < amount; x++)
	{
		for(int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
	}
}

void ScrollRight(int amount)
{
	CachedScrollX += amount;
	int x = 0;
	
	for(int n = 0; n < VISIBLE_TILES_X - amount; n++)
	{
		for(int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = VisibleTileCache[y * VISIBLE_TILES_X + x + amount];
		}
		x++;
	}
	
	x = VISIBLE_TILES_X - 1;
	for(int n = 0; n < amount; n++)
	{
		for(int y = 0; y < VISIBLE_TILES_Y; y++)
		{
			VisibleTileCache[y * VISIBLE_TILES_X + x] = CalculateTile(x + CachedScrollX, y + CachedScrollY);
		}
		x--;
	}
}

void DrawCursorRect(uint8_t cursorDrawX, uint8_t cursorDrawY, uint8_t cursorWidth, uint8_t cursorHeight)
{
    for(int n = 0; n < cursorWidth; n++)
    {
      uint8_t colour = ((n + AnimationFrame) & 4) != 0 ? 1 : 0;
      PutPixel(cursorDrawX + n, cursorDrawY + cursorHeight - 1, colour);
      PutPixel(cursorDrawX + cursorWidth - n - 1, cursorDrawY, colour);
    }

    for(int n = 0; n < cursorHeight; n++)
    {
      uint8_t colour = ((n + AnimationFrame) & 4) != 0 ? 1 : 0;
      PutPixel(cursorDrawX, cursorDrawY + n, colour);
      PutPixel(cursorDrawX + cursorWidth - 1, cursorDrawY + cursorHeight - n - 1, colour);
    }  
}

void DrawCursor()
{
	uint8_t cursorX, cursorY;
	int cursorWidth = TILE_SIZE, cursorHeight = TILE_SIZE;
	
	if(UIState.brush >= FirstBuildingBrush)
	{
		BuildingType buildingType = (BuildingType)(UIState.brush - FirstBuildingBrush + 1);
		GetBuildingBrushLocation(buildingType, &cursorX, &cursorY);
		const BuildingInfo* buildingInfo = GetBuildingInfo(buildingType);
		cursorWidth *= pgm_read_byte(&buildingInfo->width);
		cursorHeight *= pgm_read_byte(&buildingInfo->height);
	}
	else
	{
		cursorX = UIState.selectX;
		cursorY = UIState.selectY;
	}

	int cursorDrawX, cursorDrawY;
	cursorDrawX = (cursorX * 8) - UIState.scrollX;
	cursorDrawY = (cursorY * 8) - UIState.scrollY;
	
	if(cursorDrawX >= 0 && cursorDrawY >= 0 && cursorDrawX + cursorWidth < DISPLAY_WIDTH && cursorDrawY + cursorHeight < DISPLAY_HEIGHT)
	{
    DrawCursorRect(cursorDrawX, cursorDrawY, cursorWidth, cursorHeight);
	}
}

void AnimatePowercuts()
{
  bool showPowercut = (AnimationFrame & 8) != 0;
  
  for(int n = 0; n < MAX_BUILDINGS; n++)
  {
    Building* building = &State.buildings[n];
    
    if(building->type && building->type != Park)
    {
      int screenX = building->x + 1 - CachedScrollX;
      int screenY = building->y + 1 - CachedScrollY;

      if(screenX >= 0 && screenY >= 0 && screenX < VISIBLE_TILES_X && screenY < VISIBLE_TILES_Y)
      {
        if(showPowercut && !building->hasPower)
        {
          VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = POWERCUT_TILE;
        }
        else
        {
          const BuildingInfo* info = GetBuildingInfo(building->type);
          uint8_t tile = pgm_read_byte(&info->drawTile);
          VisibleTileCache[screenY * VISIBLE_TILES_X + screenX] = tile + 17;
        }
      } 
    }

  }
}

void DrawTileAt(uint8_t tile, int x, int y)
{
  for(int col = 0; col < TILE_SIZE; col++)
  {
    uint8_t readBuf = pgm_read_byte(&GetTileData(tile)[col]);

    for(int row = 0; row < TILE_SIZE; row++)
    {
      uint8_t colour = readBuf & 1;
      readBuf >>= 1;
      PutPixel(x + col, y + row, colour);
    }
  }
}

#if _WIN32
void DrawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour)
{
  for(int j = 0; j < h; j++)
  {
    for(int i = 0; i < w; i++)
    {
      PutPixel(x + i, y + j, colour);
    }
  }
}

void DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour)
{
  for(int j = 0; j < w; j++)
  {
    PutPixel(x + j, y, colour);
    PutPixel(x + j, y + h - 1, colour);
  }

  for(int j = 1; j < h - 1; j++)
  {
    PutPixel(x, y + j, colour);
    PutPixel(x + w - 1, y + j, colour);
  }
}
#endif

void DrawUI()
{
  if(UIState.showingToolbar)
  {
    uint8_t buttonX = 1;

    DrawFilledRect(0, DISPLAY_HEIGHT - TILE_SIZE - 2, NUM_TOOLBAR_BUTTONS * (TILE_SIZE + 1) + 2, TILE_SIZE + 2, 1);
    DrawFilledRect(0, DISPLAY_HEIGHT - TILE_SIZE - 2 - FONT_HEIGHT - 1, DISPLAY_WIDTH / 2 + FONT_WIDTH + 1, FONT_HEIGHT + 2, 1);
    
    for(int n = 0; n < NUM_TOOLBAR_BUTTONS; n++)
    {
      DrawTileAt(FIRST_BRUSH_TILE + n, buttonX, DISPLAY_HEIGHT - TILE_SIZE - 1);
      buttonX += TILE_SIZE + 1;    
    }

    DrawCursorRect(UIState.toolbarSelection * (TILE_SIZE + 1), DISPLAY_HEIGHT - TILE_SIZE - 2, TILE_SIZE + 2, TILE_SIZE + 2);
    const char* currentSelection = GetToolbarString(UIState.toolbarSelection);
    DrawString(currentSelection, 1, DISPLAY_HEIGHT - FONT_HEIGHT - TILE_SIZE - 2);

    uint16_t cost = 0;
    
    switch(UIState.toolbarSelection)
    {
      case 0: cost = BULLDOZER_COST; break;
      case 1: cost = ROAD_COST; break;
      case 2: cost = POWERLINE_COST; break;
      default:
      {
        int buildingIndex = 1 + UIState.toolbarSelection - FirstBuildingBrush;
        if(buildingIndex < Num_BuildingTypes)
        {
          const BuildingInfo* buildingInfo = GetBuildingInfo(buildingIndex);
          cost = pgm_read_word(&buildingInfo->cost);
        }
      }
      break;  
    }
    if(cost > 0)
    {
      DrawCurrency(cost, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT - FONT_HEIGHT - TILE_SIZE - 2);
    }
  }
  else
  {
    // Current brush at bottom left
    const char* currentSelection = GetToolbarString(UIState.brush);
    DrawFilledRect(0, DISPLAY_HEIGHT - TILE_SIZE - 2, TILE_SIZE + 2 + strlen_P(currentSelection) * FONT_WIDTH + 2, TILE_SIZE + 2, 1);
    DrawTileAt(FIRST_BRUSH_TILE + UIState.brush, 1, DISPLAY_HEIGHT - TILE_SIZE - 1);
    DrawString(currentSelection, TILE_SIZE + 2, DISPLAY_HEIGHT - FONT_HEIGHT - 1);
  }
  
  // Date at top left
  DrawFilledRect(0, 0, FONT_WIDTH * 8 + 2, FONT_HEIGHT + 2, 1);
  DrawString(GetMonthString(State.month), 1, 1);
  DrawInt(State.year + 1900, FONT_WIDTH * 4 + 1, 1);

  // Funds at top right
  uint8_t currencyStrLen = DrawCurrency(State.money, DISPLAY_WIDTH - FONT_WIDTH - 1, 1);
  DrawRect(DISPLAY_WIDTH - 2 - currencyStrLen * FONT_WIDTH, 0, currencyStrLen * FONT_WIDTH + 2, FONT_HEIGHT + 2, 1);
}

void Draw()
{
	// Check to see if scrolled to a new location and need to update the visible tile cache
	int tileScrollX = UIState.scrollX >> TILE_SIZE_SHIFT;
	int tileScrollY = UIState.scrollY >> TILE_SIZE_SHIFT;
	int scrollDiffX = tileScrollX - CachedScrollX;
	int scrollDiffY = tileScrollY - CachedScrollY;
	
	if(scrollDiffX < 0)
	{
		if(scrollDiffX > -VISIBLE_TILES_X)
		{
			ScrollLeft(-scrollDiffX);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}
	else if(scrollDiffX > 0)
	{
		if(scrollDiffX < VISIBLE_TILES_X)
		{
			ScrollRight(scrollDiffX);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}

	if(scrollDiffY < 0)
	{
		if(scrollDiffY > -VISIBLE_TILES_Y)
		{
			ScrollUp(-scrollDiffY);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}
	else if(scrollDiffY > 0)
	{
		if(scrollDiffY < VISIBLE_TILES_Y)
		{
			ScrollDown(scrollDiffY);
		}
		else
		{
			ResetVisibleTileCache();
		}
	}

  AnimatePowercuts();

	DrawTiles();

  if(!UIState.showingToolbar)
  {
    DrawCursor();
  }

  DrawUI();

  AnimationFrame++;
}
