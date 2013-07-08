
#include <BWAPI.h>
#include <vector>

// Defines the data structure and access for an InfluenceMap
// This map is based on build tiles for sizing. Even that level is 
// pushing it for the number of items in the map. Anything smaller
// is too expensive preformance wise

class InfluenceMap
{
public:

   // Create a new influence map
   // mapWidth - map width in build tiles
   // mapHeight - map height in build tiles
   InfluenceMap(int mapWidth, int mapHeight);
   ~InfluenceMap();

   // Store influence based on tile location
   void UpdateInfluence();

   // Draw influence based on tiles for the current screen position
   void DrawInfluenceScreen();

   // Draw influence based on tiles for the entire map
   // Also diplays a influnce tile grid and the influence value
   // for that position on the grid
   void DrawInfluenceAll();
   
   // Reset all values in the map to default of zero
   void ClearMap();

   // Sets the influence for a circular area on the map
   // Passed in values will be converted from pixel positions to tile positions
   // poistion - unit pixel position
   // radius - distance in pixels from the unit position that is influenced, must be > 0
   // value - the influence value to stamp
   void SetInfluence(BWAPI::Position position, int radius, double value);

private:

   // Influence values map based on tiles
   std::vector< std::vector<double> > mMap;

   // Used to validate values and make sure we don't exceed vector bounds
   const int maxWidth;
   const int maxHeight;

   // Stores a contributing influence value for a line of tiles
   // Uses StoreValue() to set individual values in the line
   // startX - the x value of the start tile of the line
   // y      - the y value of the line
   // endX   - the x value of the end tile of the line
   // value  - contributing influence value 
   void HorizontalLine(int startX, int y, int endX, double value);

   // Stores a contributing influence value at a location while doing
   // bounds checking to make sure the location is valid
   // x,y - build tile location
   // value - contributing influence value 
   void StoreValue(int x, int y, double value);

   // Return the blue/red level based on the influence value passed in
   // Only used for drawing
   BWAPI::Color GetInfluenceColor(double value);
};

