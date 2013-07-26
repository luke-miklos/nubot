
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
   // scale - tile multiplier factor to increase the grid size and reduce overall map size
   InfluenceMap(int mapWidth, int mapHeight, int scale = 1);
   ~InfluenceMap();

   // Store influence based on build tile location
   // Implemented in child classes
   virtual void UpdateInfluence() {}

   // Draw influence based on tiles only for the currently visible screen
   void DrawInfluenceScreen();

   // Draw influence based on tiles for the entire map
   // Also displays an influnce tile grid and the influence value
   // for that position on the grid
   void DrawInfluenceAll();
   
   // Reset all values in the map to default of zero
   void ClearMap();

   // Sets a uniform influence for a circular area on the map
   // Passed in values will be converted from pixels to build tiles
   // poistion - unit pixel position
   // radius - distance in pixels from the unit position that is influenced, must be > 0
   // value - the influence value to set, added to any existing influence value
   void SetInfluence(BWAPI::Position position, int radius, double value);

   // TODO
   // Find the nearest position to me with a influnce greater then or equal to requested value
   // BWAPI::Position FindNearestPosition(BWAPI::Position myPosition, double value);
   // Find strongest influence position
   // Find weakest influence position 

private:

   // Influence values grid based on build tiles
   std::vector< std::vector<double> > mMap;

   // Used to validate values and make sure we don't exceed vector bounds
   const int maxWidth;
   const int maxHeight;

   // Used a a divisor to make the tiles on the grid larger
   // e.g. '2' would make a grid tile of 2x2 build tiles
   const int scale;

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

