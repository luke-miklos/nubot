
#include "InfluenceMap.h"

InfluenceMap::InfluenceMap(int mapWidth, int mapHeight)
   : maxWidth(mapWidth - 1),
     maxHeight(mapHeight - 1)
{
   mMap.resize(mapWidth, std::vector<double> (mapHeight, 0.0));
}

InfluenceMap::~InfluenceMap()
{
}

void InfluenceMap::UpdateInfluence()
{
   ClearMap();

   std::set< BWAPI::Unit* > units = BWAPI::Broodwar->self()->getUnits();
   for (std::set< BWAPI::Unit* >::iterator it = units.begin(); it != units.end(); it++)
   {
      BWAPI::Unit* unit = *it;
      if (unit->exists())
      {
         // Use pixel position here as it is more accurate, gives unit center
         BWAPI::Position pixelPos = unit->getPosition();
         // Set the influence for the unit based on unis sight range
         SetInfluence(pixelPos, unit->getType().sightRange(), 1.0);

         // DEBUG - drawing to help visualization
         //BWAPI::Color color = BWAPI::Colors::Orange;
         // Draw a circle showing the unit's vision range
         //int radius = unit->getType().sightRange() + unit->getType().dimensionUp();
         //BWAPI::Broodwar->drawCircleMap(pixelPos.x(), pixelPos.y(), radius, color);

         // Box unit
         //int left = (pixelPos.x() - unit->getType().dimensionLeft());
         //int top = (pixelPos.y() - unit->getType().dimensionUp());
         //int right = (pixelPos.x() + unit->getType().dimensionRight());
         //int bottom = (pixelPos.y() + unit->getType().dimensionDown());
         //BWAPI::Broodwar->drawBoxMap(left, top, right, bottom, color);
         // DEBUG
      }
   } // End my units

   std::set< BWAPI::Unit* > enemyUnits = BWAPI::Broodwar->enemy()->getUnits();
   for (std::set< BWAPI::Unit* >::iterator it = enemyUnits.begin(); it != enemyUnits.end(); it++)
   {
      BWAPI::Unit* unit = *it;
      if (unit->exists())
      {
         // Use pixel position here as it is more accurate, gives unit center
         BWAPI::Position pixelPos = unit->getPosition();
         // Set the influence for the unit based on unis sight range
         SetInfluence(pixelPos, unit->getType().sightRange(), -1.0);
      }
   } // End enemy units
}

void InfluenceMap::DrawInfluenceScreen()
{
   const int screenWidth = (640 / TILE_SIZE);  // pixels / pixels per tile
   const int screenHeight = (480 / TILE_SIZE); // pixels / pixels per tile

   // Only draw influence on the visible part of the screen
   // Get the screen top left X,Y and draw down to the bottom right
   // A screen size is 640 x 480 pixels
   BWAPI::Position screenPosition = BWAPI::Broodwar->getScreenPosition();
   
   if (screenPosition != BWAPI::Positions::Unknown)
   {
      //for each location in the vector
      // if the influence value is > 0.0
      // Draw a blue square for the tile
      int screenLeft = (screenPosition.x() / TILE_SIZE);
      int screenTop = (screenPosition.y() / TILE_SIZE);
      int screenRight = std::min(maxWidth, (screenLeft + screenWidth));
      int screenBottom = std::min(maxHeight, (screenTop + screenHeight));
      BWAPI::Broodwar->drawTextScreen( 0, 50, "Drawing influence map from %d, %d to %d, %d", screenLeft, screenTop, screenRight, screenBottom);

      for (int x = screenLeft; x <= screenRight; ++x) 
      {
         for (int y = screenTop; y <= screenBottom; ++y)
         {
            if (mMap[x][y] > 0.01 || mMap[x][y] < -0.01)
            {
               //convert tile location to top left/bottom right pixel coordinates
               int boxLeft = (x * TILE_SIZE);
               int boxTop = (y * TILE_SIZE);
               int boxRight = boxLeft + TILE_SIZE;
               int boxBottom = boxTop + TILE_SIZE;
               BWAPI::Color color = GetInfluenceColor(mMap[x][y]);
               BWAPI::Broodwar->drawBoxMap(boxLeft, boxTop, boxRight, boxBottom, color, true);
            }
         }
      }
   } // End screen position check
}

void InfluenceMap::DrawInfluenceAll()
{
   //for each location in the vector
     // if the influence value is > 0.0
       // Draw a blue square starting at the build tile position * TILE_SIZE (32) and TILE_SIZE in x and y directions
   int width = 0;
   int height = 0;
   for (std::vector< std::vector<double> >::const_iterator w = mMap.begin(); w != mMap.end();++w) 
   {
      for (std::vector<double>::const_iterator h = w->begin(); h != w->end();++h)
      {
         if (*h > 0.01 || *h < -0.01)
         {
            int left = (width * TILE_SIZE);
            int top = (height * TILE_SIZE);
            int right = left + TILE_SIZE;
            int bottom = top + TILE_SIZE;
            BWAPI::Color color = GetInfluenceColor(*h);
            BWAPI::Broodwar->drawBoxMap(left, top, right, bottom, color, true);

            // Display influence value, center of tile
            BWAPI::Broodwar->drawTextMap(left + 16, top + 16, "%d", (int)*h);
         }
         height++;
      }
      // Move to next column and start again at the top row.
      width++;
      height = 0;
   }

   // Draw Grid
   width = mMap.size();
   height = mMap[0].size();
   BWAPI::Color gridColor = BWAPI::Colors::Green;
   for (int x = 0; x < width; ++x)
   {
      BWAPI::Broodwar->drawLineMap((x * TILE_SIZE), 0, (x * TILE_SIZE), (height * TILE_SIZE), gridColor);
   }
   for (int y = 0; y < height; ++y)
   {
      BWAPI::Broodwar->drawLineMap(0, (y * TILE_SIZE), (width * TILE_SIZE), (y * TILE_SIZE), gridColor);
   }

   BWAPI::Broodwar->drawTextScreen( 0, 50, "Drawing influence map, size is %d x %d", width, height);
}

void InfluenceMap::ClearMap()
{
   // There has to be a better way to do this
   // mMap.clear(), mMap.resize()? seems expensive
   // Change how we represent the map to just an array so we can do a memset?
   for (std::vector< std::vector<double> >::iterator x = mMap.begin(); x != mMap.end();++x) 
   {
      for (std::vector<double>::iterator y = x->begin(); y != x->end();++y)
      {
         *y = 0.0;
      }
   }   
}

// Adapted from the optimized example of the midpoint circle algorithm
// See http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
// Modifications are to prevent double counting influence
void InfluenceMap::SetInfluence(BWAPI::Position position, int radius, double value)
{
   // Convert from pixels to build tiles
   int cX = position.x()/TILE_SIZE;
   int cY = position.y()/TILE_SIZE;
   radius = radius/TILE_SIZE + 1;

   int x = radius;
   int y = 0;
   int error = -radius;
   int previousY = y;

   while(x >= y)
   {
      previousY = y;
      error += y;
      ++y;
      error += y;

      HorizontalLine(cX - x, cY + previousY, cX + x, value);
      if (x != 0 && previousY != 0) // prevent double counting center line
      {
         HorizontalLine(cX - x, cY - previousY, cX + x, value);
      }

      if (error >= 0)
      {
         if (x != previousY) // prevents couning top/bottom multiple times
         {
            HorizontalLine(cX - previousY, cY + x, cX + previousY, value);
            HorizontalLine(cX - previousY, cY - x, cX + previousY, value);
         }

         error -= x;
         --x;
         error -= x;
      }
   }
}

// private
void InfluenceMap::HorizontalLine(int startX, int y, int endX, double value)
{
   for (int x = startX; x <= endX; ++x)
   {
      StoreValue(x, y, value);
   }
}

// private
void InfluenceMap::StoreValue(int x, int y, double value)
{
   if (y >= 0 && y <= maxHeight && x >= 0 && x <= maxWidth)
   {
      mMap[x][y] += value;
   }
}

// Blue color IDs, darkest to blue:
//  120, 121, 122, 123, 165
// Red color IDs, darkest to red:
//   171, 172, 173, 174, 111
BWAPI::Color InfluenceMap::GetInfluenceColor(double value)
{
   BWAPI::Color color;
   if (value > 0.0)
   {
      if (value >= 12.0)
      {
         color = BWAPI::Color(165); // Blue
      }
      else if (value >= 9.0)
      {
         color = BWAPI::Color(123);
      }
      else if (value >= 6.0)
      {
         color = BWAPI::Color(122);
      }
      else if (value >= 3.0)
      {
         color = BWAPI::Color(121);
      }
      else if (value > 0.0)
      {
         color = BWAPI::Color(120);
      }
      else
      {
         color = BWAPI::Colors::Black;
      }
   }
   else
   {
      if (value <= -12.0)
      {
         color = BWAPI::Color(111); //Red
      }
      else if (value <= 9.0)
      {
         color = BWAPI::Color(174);
      }
      else if (value <= 6.0)
      {
         color = BWAPI::Color(173);
      }
      else if (value <= 3.0)
      {
         color = BWAPI::Color(172);
      }
      else if (value < 0.0)
      {
         color = BWAPI::Color(171);  
      }
      else
      {
         color = BWAPI::Colors::Black;
      }
   }

   return color;
}