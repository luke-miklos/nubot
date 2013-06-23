
#include "FlowField.hpp"

#include <string>
#include <fstream>
#include <set>



FlowField* FlowField::mInstance = 0;


FlowField* FlowField::Instance() 
{
   if (mInstance == 0)
   {
      mInstance = new FlowField(BWAPI::Broodwar->mapWidth()*4, BWAPI::Broodwar->mapHeight()*4);
      mInstance->Initialize();   //saves off unchanging map data & get or create the fields
   }
   return mInstance;
}


FlowField::FlowField(int c, int r) 
   : cols(c)
   , rows(r)
{
   walkable  = std::vector<bool>(rows*cols, false);   //global map data
   fringe    = std::vector<int> (rows*cols, 0);       //utility
}

void FlowField::Initialize()
{
   int size = rows * cols;

   for (int r=0; r<rows; ++r)
   {
      for (int c=0; c<cols; ++c)
      {
         setWalkable(c, r, BWAPI::Broodwar->isWalkable(c, r));
      }
   }

   //read in flow fields from a file if possible
	std::string readFileName = "bwapi-data/testio/flow_fields/" + BWAPI::Broodwar->mapName() + ".bin";
   std::ifstream readFile;
   readFile.open(readFileName.c_str(), std::ios::in | std::ios::binary);
   if (readFile.good())
   {
      int count, x, y;
      while (!readFile.eof())
      {
         int ar[3];  //x, y, size
         readFile.read(reinterpret_cast<char*>(ar), 3*sizeof(int)); 
         x     = ar[0];
         y     = ar[1];
         count = ar[2];
         BWAPI::TilePosition tile(x, y);
         TargetField* flow = new TargetField(count);
         readFile.read(reinterpret_cast<char*>(&flow->cells[0]), flow->cells.size() * sizeof(flow->cells[0])); 
         mFields[tile] = flow;
      }
		readFile.close();
   }
   else
   {
      readFile.close();

      //otherwise create them & write them out
      //create a flow field for each starting location
      std::set<BWAPI::TilePosition> starts = BWAPI::Broodwar->getStartLocations();
      std::set<BWAPI::TilePosition>::iterator it = starts.begin();
      for (; it != starts.end(); it++)
      {
         mFields[*it] = GenerateFields(*it);
      }

      //write out all the flow fields for this map to one file
      std::ofstream writeFile;
	   std::string writeFileName = "bwapi-data/testio/flow_fields/" + BWAPI::Broodwar->mapName() + ".bin";
      writeFile.open(writeFileName.c_str(), std::ios::out | std::ios::binary);
      std::map<BWAPI::TilePosition, TargetField*>::iterator mit = mFields.begin();
      for (; mit != mFields.end(); mit++)
      {
         BWAPI::TilePosition tpos = mit->first;
         TargetField* field = mit->second;
         if (!field->cells.empty())
         {
            int ar[3] = {tpos.x(), tpos.y(), int(field->cells.size())};  //x, y, size
            writeFile.write(reinterpret_cast<char*>(ar), 3*sizeof(int));
            writeFile.write(reinterpret_cast<char*>(&field->cells[0]), field->cells.size() * sizeof(field->cells[0]));
         }
      }
	   writeFile.close();
   }
}


FlowField::TargetField* FlowField::GenerateFields(BWAPI::TilePosition pos)
{
	// the size of the map
	int size = rows*cols;
   TargetField* fields = new TargetField(size);  //return this

   int c = pos.x()*4;
   int r = pos.y()*4;
	// reset the internal variables
 	resetFringe();

   fields->moveTo[index(c,r)] = 'X';  //this is the target
	// set the fringe variables accordingly
	int fringeSize(1);
	int fringeIndex(0);
	fringe[0] = index(c,r);
	// temporary variables used in search loop
	int currentIndex;
   int nextIndex;

   int rotation = 0; //used to change around which direction is searched first for each node explored
	
   //populate the flood fill, while we still have things left to expand
	while (fringeIndex < fringeSize)
	{
		// grab the current index to expand from the fringe
		currentIndex = fringe[fringeIndex++];

      switch(rotation)
      {
      case 0:
      {
		   // search up
		   nextIndex = (currentIndex >= cols) ? (currentIndex - cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'D';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search down
		   nextIndex = ((currentIndex + cols) < size) ? (currentIndex + cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'U';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search left
		   nextIndex = ((currentIndex % cols) > 0) ? (currentIndex - 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'R';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search right
		   nextIndex = ((currentIndex % cols) < (cols-1)) ? (currentIndex + 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'L';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
      }break;

      case 1:
      {
		   // search left
		   nextIndex = ((currentIndex % cols) > 0) ? (currentIndex - 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'R';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search right
		   nextIndex = ((currentIndex % cols) < (cols-1)) ? (currentIndex + 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'L';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search up
		   nextIndex = (currentIndex >= cols) ? (currentIndex - cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'D';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search down
		   nextIndex = ((currentIndex + cols) < size) ? (currentIndex + cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'U';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
      }break;

      case 2:
      {
		   // search down
		   nextIndex = ((currentIndex + cols) < size) ? (currentIndex + cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'U';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search up
		   nextIndex = (currentIndex >= cols) ? (currentIndex - cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'D';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search right
		   nextIndex = ((currentIndex % cols) < (cols-1)) ? (currentIndex + 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'L';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search left
		   nextIndex = ((currentIndex % cols) > 0) ? (currentIndex - 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'R';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
      }break;

      case 3:
      {
		   // search right
		   nextIndex = ((currentIndex % cols) < (cols-1)) ? (currentIndex + 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'L';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search left
		   nextIndex = ((currentIndex % cols) > 0) ? (currentIndex - 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'R';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search down
		   nextIndex = ((currentIndex + cols) < size) ? (currentIndex + cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'U';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
		   // search up
		   nextIndex = (currentIndex >= cols) ? (currentIndex - cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'D';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
      }break;
      };

      rotation++;
      if (rotation>3)
         rotation = 0;
	}

   int c2, r2, x, y;
   //populate the flow field
   for (int r=0; r<rows; ++r)
   {
      for (int c=0; c<cols; ++c)
      {
         int i = index(c,r);
         char d = fields->moveTo[i];
         if (d != 0) //this checks if the current tile at (c,r) is reachable from the target location
         {
            //we know the first one is pathable, it got us here, start search at 2nd one
            c2 = c;
            r2 = r;
            if (d=='D') {
               r2++;
            } else if (d=='U') {
               r2--;
            } else if (d=='R') {
               c2++;
            } else {
               c2--;
            }
            //step along flood fill one tile at a time, checking path along the way
            bool pathable = true;
            while (pathable)
            {
               x = c2;
               y = r2;
               int i2 = index(c2,r2);
               d = fields->moveTo[i2];
               if (d=='D') {
                  r2++;
               } else if (d=='U') {
                  r2--;
               } else if (d=='R') {
                  c2++;
               } else if (d=='L') {
                  c2--;
               } else if (d == 'X') {  //reached destination
                  x = c2;
                  y = r2;
                  break;
               }
               //only check the path if we've turned at all
               if (c2!=c && r2!=r)
               {
                  pathable = isPathable(c,r,c2,r2);
               }
            }
            //done, use last known good point as final destination
            double angle = atan2((double)(y-r),(double)(x-c));
            if (angle < 0)
            {
               angle += (2.0*M_PI);
            }
            fields->setAngle(i, angle);
         }
      }
   }
   return fields;
}


//steps along the line between these two walkable tiles to see if its not blocked
bool FlowField::isPathable(int c1, int r1, int c2, int r2)
{
   float step = 0.2f;
   //vector
   float x = float(c2-c1);
   float y = float(r2-r1);
   float L = sqrt(x*x+y*y);   //length
   x /= L;  //normalize
   y /= L;  //normalize
   float cur = step;
   float xAdj = (x>=0)?(0.5f):(-0.5f);
   float yAdj = (y>=0)?(0.5f):(-0.5f);
   bool pathable = true;
   while (cur < L && pathable)
   {
      pathable = isWalkable(c1+int(x*cur+xAdj), r1+int(y*cur+yAdj));
      cur += step;
   }
   return pathable;
}


void FlowField::DrawFlowFieldOnMap(BWAPI::TilePosition pos) 
{
   std::map<BWAPI::TilePosition, TargetField*>::iterator it = mFields.find(pos);
   if (it != mFields.end())
   {
      TargetField* flow = it->second;
      for (int r=0; r<rows; ++r)
      {
         for (int c=0; c<cols; ++c)
         {
            if (flow->isMovable(index(c,r)))
            {
               double angle = flow->getAngle(index(c,r));
               int x1 = c*8;
               int y1 = r*8;
               int x2 = x1 + (int)(7.0*cos(angle));
               int y2 = y1 + (int)(7.0*sin(angle));
               if (x2>x1) {
                  if (y2>y1) {   //to the right & down
                     BWAPI::Broodwar->drawLineMap(x1,y1,x2,y2, BWAPI::Colors::Green);
                  } else {       //to the right & up
                     BWAPI::Broodwar->drawLineMap(x1,y1,x2,y2, BWAPI::Colors::Red);
                  }
               } else {
                  if (y2>y1) {   //to the left & down
                     BWAPI::Broodwar->drawLineMap(x1,y1,x2,y2, BWAPI::Colors::Blue);
                  } else {       //to the left & up
                     BWAPI::Broodwar->drawLineMap(x1,y1,x2,y2, BWAPI::Colors::Yellow);
                  }
               }
            }
         }
      }
   }
}

//this wont be available if we've loaded from a file
//the flood fill is only used as an intermediate step to find the flow field
void FlowField::DrawFloodFillOnMap(BWAPI::TilePosition pos)
{
   std::map<BWAPI::TilePosition, TargetField*>::iterator it = mFields.find(pos);
   if (it != mFields.end())
   {
      TargetField* flow = it->second;
      for (int r=0; r<rows; ++r)
      {
         for (int c=0; c<cols; ++c)
         {
            if (flow->isMovable(index(c,r)))
            {
               int x = c*8;
               int y = r*8;
               char direction = flow->direction(index(c,r));
               switch(direction)
               {
               case 'U': { BWAPI::Broodwar->drawLineMap(x,y,x  ,y-6, BWAPI::Colors::Yellow); }break;
               case 'D': { BWAPI::Broodwar->drawLineMap(x,y,x  ,y+6, BWAPI::Colors::Green ); }break;
               case 'L': { BWAPI::Broodwar->drawLineMap(x,y,x-6,y  , BWAPI::Colors::Teal  ); }break;
               case 'R': { BWAPI::Broodwar->drawLineMap(x,y,x+6,y  , BWAPI::Colors::Red   ); }break;
               default:  { BWAPI::Broodwar->drawBoxMap( x,y,x+6,y+6, BWAPI::Colors::Purple, true); }break;
               };
            }
         }
      }
   }
}


void FlowField::DrawWalkable() 
{
   for (int r=0; r<rows; ++r)
   {
      for (int c=0; c<cols; ++c)
      {
         if (isWalkable(c,r))
         {
            int x = c*8 + 3;
            int y = r*8 + 3;
            BWAPI::Broodwar->drawDotMap(x,y, BWAPI::Colors::White);
         }
      }
   }
}



//returns false if flow field not found for tgt location, or if tgt is not reachable
bool FlowField::GetFlowFromTo(BWAPI::Position pos, BWAPI::TilePosition tgt, double& angle)
{
   std::map<BWAPI::TilePosition, TargetField*>::iterator mit = mFields.find(tgt);
   if (mit==mFields.end())
   {
      return false;
   }
   
   int i = index(pos.x()/4,pos.y()/4);
   TargetField* field = mit->second;
   if (field->isMovable(i))
   {
      angle = field->getAngle(i);
      return true;
   }
   else
   {
      return false;
   }
}




//assumes input in range [0,2*pi]
void FlowField::TargetField::setAngle(int i, double angle)
{
   //angle is in radians
   //cell == 0 is reserved to represent "not movable here"
   //so real angle values start at 1
   //scale angle to [0,254] and add in 1
   cells[i] = char(1)+char(254.0*angle/(2.0*M_PI));
}

//returns angle in range [0, 2*pi]
double FlowField::TargetField::getAngle(int i)
{
   //angle is in radians
   //cell == 0 is reserved to represent "not movable here"
   //so real angle values start at 1
   //take off 1, then scale from [0,254] to angle
   return (double(cells[i]-1)*2.0*M_PI/254.0);
}

bool FlowField::TargetField::isMovable(int i)
{
   //exploit the fact that cells==0 means 'not movable'
   return (cells[i] != 0);
}

