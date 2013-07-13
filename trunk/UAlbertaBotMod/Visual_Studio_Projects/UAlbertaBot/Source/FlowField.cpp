
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
   long end = 0;
   long beg = 0;
   long totalFileSize =  BWAPI::Broodwar->getStartLocations().size() * (3*sizeof(int)+size*sizeof(char));
   if (readFile.good())
   {
      readFile.seekg(0, std::ios::beg);
      beg = readFile.tellg();
      readFile.seekg(0, std::ios::end);
      end = readFile.tellg();
   }
   if (totalFileSize == (end-beg))
   {
      readFile.seekg(0, std::ios::beg);
      int count, x, y;
      while (readFile.peek() != EOF)
      {
         int ar[3];  //x, y, size
         readFile.read(reinterpret_cast<char*>(ar), 3*sizeof(int)); 
         x     = ar[0];
         y     = ar[1];
         count = ar[2];
         if (count != size)
         {
            //something seriously wrong, stop reading here
            break;
         }
         BWAPI::TilePosition tile(x, y);
         TargetField* flow = new TargetField(count);
         readFile.read(reinterpret_cast<char*>(&flow->cells[0]), flow->cells.size() * sizeof(flow->cells[0])); 
         //readFile.sync();
         mFields[tile] = flow;
      }
   }
   readFile.close();

   //cycle through all starting positions & create any flow fields that don't exist yet
   std::set<BWAPI::TilePosition> starts = BWAPI::Broodwar->getStartLocations();
   std::set<BWAPI::TilePosition>::iterator it = starts.begin();
   std::map<BWAPI::TilePosition, TargetField*>::iterator mit;
   //create objects for flow fields
   bool once = false;
   for (; it != starts.end(); it++)
   {
      mit = mFields.find(*it);
      if (mit == mFields.end())
      {
         mFields[*it] = new TargetField(size);
         if (!once)
         {
            //shrink walkable area a bit, fill in flow fields with vectors away from walls
            BlockOutMineralsAndGeysers();
            once = true;
         }
         GenerateFields(*it, mFields[*it]);
      }
   }

   if (once)
   {
      //at least one flow field was created, write the file (or re-write an incomplete file)
      //write out all the flow fields for this map to one file
      std::ofstream writeFile;
	   std::string writeFileName = "bwapi-data/testio/flow_fields/" + BWAPI::Broodwar->mapName() + ".bin";
      writeFile.open(writeFileName.c_str(), std::ios::out | std::ios::binary);
      for (mit = mFields.begin(); mit != mFields.end(); mit++)
      {
         BWAPI::TilePosition tpos = mit->first;
         TargetField* field = mit->second;
         if (!field->cells.empty())
         {
            int ar[3] = {tpos.x(), tpos.y(), int(field->cells.size())};  //x, y, size
            writeFile.write(reinterpret_cast<char*>(ar), 3*sizeof(int));
            writeFile.write(reinterpret_cast<char*>(&field->cells[0]), field->cells.size() * sizeof(field->cells[0]));
            writeFile.flush();
         }
      }
      writeFile.seekp(0, std::ios::beg);
      long beg = writeFile.tellp();
      writeFile.seekp(0, std::ios::end);
      long end = writeFile.tellp();
      long totalFileSize = mFields.size() * (3*sizeof(int)+size*sizeof(char));

      if (totalFileSize != (end-beg))
      {
         //write out a warning
         BWAPI::Broodwar->drawTextScreen(100, 600, "ERROR: flow field write output: %d / %d", (end-beg), totalFileSize);
      }
	   writeFile.close();
   }
   //}
}


void FlowField::BlockOutMineralsAndGeysers()
{
   std::set<BWAPI::Unit*> minerals = BWAPI::Broodwar->getStaticMinerals();
   std::set<BWAPI::Unit*>::iterator it;
   for (it = minerals.begin(); it!=minerals.end(); it++)
   {
      BWAPI::Unit* mineral = *it;
      int c1 = mineral->getLeft()/8;
      int r1 = mineral->getTop()/8;
      int c2 = mineral->getRight()/8;
      int r2 = mineral->getBottom()/8;

      for (int r=r1; r<=r2; ++r)
      {
         for (int c=c1; c<=c2; ++c)
         {
            walkable[index(c,r)] = false;
         }
      }
   }

   std::set<BWAPI::Unit*> geysers  = BWAPI::Broodwar->getStaticGeysers();
   for (it = geysers.begin(); it!=geysers.end(); it++)
   {
      BWAPI::Unit* mineral = *it;
      int c1 = mineral->getLeft()/8;
      int r1 = mineral->getTop()/8;
      int c2 = mineral->getRight()/8;
      int r2 = mineral->getBottom()/8;
      for (int r=r1; r<=r2; ++r)
      {
         for (int c=c1; c<=c2; ++c)
         {
            walkable[index(c,r)] = false;
         }
      }
   }
}

//for taking building footprints out of the flow field
void FlowField::onUnitShow(BWAPI::Unit* unit)
{
   BWAPI::UnitType type = unit->getType();
   if (type.isBuilding() && !type.isNeutral() && !type.isRefinery())
   {
      //BWAPI::Position pos = unit->getPosition();
      int c1 = unit->getLeft()/8;
      int r1 = unit->getTop()/8;
      int c2 = unit->getRight()/8;
      int r2 = unit->getBottom()/8;
      std::map<BWAPI::TilePosition, TargetField*>::iterator mit;
      //mark the actual area of the building as unwalkable
      for (int r=r1; r<=r2; ++r)
      {
         for (int c=c1; c<=c2; ++c)
         {
            int i = index(c,r);
            if (walkable[i])
            {
               walkable[i] = false;
               //if marking unwalkable spots, clear flow field too
               for (mit = mFields.begin(); mit != mFields.end(); mit++)
               {
                  mit->second->cells[i] = 0;
               }
            }
         }
      }

      //now adjust the tiles around the edges to help units make their way around the building
      int left   = c1-2; while(left    <    0) { left++;   }
      int right  = c2+2; while(right  >= cols) { right--;  }
      int top    = r1-2; while(top     <    0) { top++;    }
      int bottom = r2+2; while(bottom >= rows) { bottom--; }

      //when checking edges, make it fast &...
      //just use the char representation of the angle (1,255)=(0,2*pi)

      //check top edge
      for (int r=top; r<r1; ++r)
      {
         for (int c=left; c <= right; ++c)
         {
            int i = index(c,r);
            if (walkable[i])
            {
               for (mit = mFields.begin(); mit != mFields.end(); mit++)
               {
                  unsigned char cell = mit->second->cells[i];
                  if (cell <= unsigned char(64))   //facing down & right
                  {
                     //mit->second->cells[i] = unsigned char(1);   //make it face right
                     mit->second->cells[i] = unsigned char(252);   //make it face right & up a tad
                  }
                  else if (cell <= unsigned char(128))  //facing down & left
                  {
                     //mit->second->cells[i] = unsigned char(128);  //make it face left
                     mit->second->cells[i] = unsigned char(131);  //make it face left & up a tad
                  }
               }
            }
         }
      }

      //check bottom edge
      for (int r=bottom; r>r2; r--)
      {
         for (int c=left; c <= right; ++c)
         {
            int i = index(c,r);
            if (walkable[i])
            {
               for (mit = mFields.begin(); mit != mFields.end(); mit++)
               {
                  unsigned char cell = mit->second->cells[i];
                  if (cell >= unsigned char(191))   //facing up & right
                  {
                     //mit->second->cells[i] = unsigned char(1);   //make it face right
                     mit->second->cells[i] = unsigned char(4);   //make it face right & down a tad
                  }
                  else if (cell >= unsigned char(128))  //facing left
                  {
                     //mit->second->cells[i] = unsigned char(128);  //make it face left
                     mit->second->cells[i] = unsigned char(125);  //make it face left & down a tad
                  }
               }
            }
         }
      }

      //check left edge
      for (int r=top; r<=bottom; ++r)
      {
         for (int c=left; c < c1; ++c)
         {
            int i = index(c,r);
            if (walkable[i])
            {
               for (mit = mFields.begin(); mit != mFields.end(); mit++)
               {
                  unsigned char cell = mit->second->cells[i];
                  if (cell <= unsigned char(64))   //facing down & right
                  {
                     //mit->second->cells[i] = unsigned char(64);   //make it face down
                     mit->second->cells[i] = unsigned char(67);   //make it face down & left a tad
                  }
                  else if (cell >= unsigned char(191))  //facing up & right
                  {
                     //mit->second->cells[i] = unsigned char(191);  //make it face up
                     mit->second->cells[i] = unsigned char(188);  //make it face up & left a tad
                  }
               }
            }
         }
      }

      //check right edge
      for (int r=top; r<=bottom; ++r)
      {
         for (int c=right; c > c2; c--)
         {
            int i = index(c,r);
            if (walkable[i])
            {
               for (mit = mFields.begin(); mit != mFields.end(); mit++)
               {
                  unsigned char cell = mit->second->cells[i];
                  if (cell >= unsigned char(64))
                  {
                     if (cell <= unsigned char(128))   //facing down & left
                     {
                        //mit->second->cells[i] = unsigned char(64);   //make it face down
                        mit->second->cells[i] = unsigned char(61);   //make it face down & right a tad
                     }
                     else if (cell <= unsigned char(191))  //facing up & left
                     {
                        //mit->second->cells[i] = unsigned char(191);  //make it face up
                        mit->second->cells[i] = unsigned char(194);  //make it face up & right a tad
                     }
                  }
               }
            }
         }
      }
   }
}


bool FlowField::validDestination(BWAPI::Position pos)
{
   return this->isWalkable(pos.x()/8,pos.y()/8);
}


BWAPI::Position FlowField::GetVectorFromWalls(BWAPI::Unit* unit, double scale)
{
   int top = -1 + unit->getTop()/8;
   int bot =  1 + unit->getBottom()/8;
   int lef = -1 + unit->getLeft()/8;
   int rit =  1 + unit->getRight()/8;
   int x = 0;
   int y = 0;
   //do corners last
   //top edge
   for (int c = lef+1; c<rit; ++c) {
      if (!isWalkable(c, top)) {
         y++;
      }
   }
   //bot edge
   for (int c = lef+1; c<rit; ++c) {
      if (!isWalkable(c, bot)) {
         y--;
      }
   }
   //lef edge
   for (int r = top+1; r<bot; ++r) {
      if (!isWalkable(lef, r)) {
         x++;
      }
   }
   //rit edge
   for (int r = top+1; r<bot; ++r) {
      if (!isWalkable(rit, r)) {
         x--;
      }
   }
   //top lef corner
   if (!isWalkable(lef, top)) {
      x++; y++;
   }
   //top rit corner
   if (!isWalkable(rit, top)) {
      x--; y++;
   }
   //bot rit corner
   if (!isWalkable(rit, bot)) {
      x--; y--;
   }
   //bot lef corner
   if (!isWalkable(rit, bot)) {
      x++; y--;
   }
   if (x!=0 || y!= 0)
   {
      double mag = sqrt(double(x*x + y*y));
      return BWAPI::Position(round(double(x*scale/mag)), round(double(y*scale/mag)));
   }
   else
   {
      return BWAPI::Position(0,0);
   }
}


//makes a pass over walkable tiles & shrinks it by size tiles
void FlowField::ShrinkWalkableArea(TargetField* fields, int tiles)
{
   std::vector<bool> localWalkable(walkable);

   //shrinks walkable area by size
   int rowsM1 = rows-1;
   int colsM1 = cols-1;
   int i;
   int x, y;
   bool up, dn, lf, rt, ur, ul, dr, dl;
   std::vector<int> indices;
   //std::map<BWAPI::TilePosition, TargetField*>::iterator mit;

   for (int k=0; k < tiles; ++k)
   {
      //mark the indicated tiles from the previous loop as unwalkable now
      while (indices.size() > 0)
      {
         localWalkable[indices.back()] = false;
         indices.pop_back();
      }

      //do not search outside borders, make them all unwalkable at end
      //this also has the nice advantage of making our up/down/left/right indices all valid
      for (int r=1; r<rowsM1; ++r)
      {
         for (int c=1; c<colsM1; ++c)
         {
            i = index(c,r);
            if (localWalkable[i])
            {
               up = localWalkable[i-cols];
               dn = localWalkable[i+cols];
               lf = localWalkable[i-1];
               rt = localWalkable[i+1];
               ul = localWalkable[i-cols-1];   //up left
               dr = localWalkable[i+cols+1];   //down right
               dl = localWalkable[i+cols-1];   //down left
               ur = localWalkable[i-cols+1];   //up right
               if (!(up && dn && lf && rt && ul && ur && dl && dr))
               {
                  //mark this index for later, cant make it unwalkable now, it would incorrectly affect other tiles
                  indices.push_back(i);
                  //figure out which direction is best away from walls
                  x = 0; y = 0;
                  if (ul) {
                     x++; y++; }
                  if (up) {
                     y++; }
                  if (ur) {
                     y++; x--; }
                  if (rt) {
                     x--; }
                  if (dr) {
                     x--; y--; }
                  if (dn) {
                     y--; }
                  if (dl) {
                     x++; y--; }
                  if (lf) {
                     x++; }
                  double angle = atan2(double(-y),double(-x));

                  ////if marking unwalkable spots, make flow field vector point away from walls
                  //for (mit = mFields.begin(); mit != mFields.end(); mit++)
                  //{
                  //   mit->second->setAngle(i, angle);
                  //}
                  //make flow field skew away from walls, but not completely
                  fields->skewAngle(i, angle, 1.0);   //try a scale of 2.0 for now, a larger scale skews the original angle less
               }
            }
         }
      }
   }
}


void FlowField::MakeBordersUnWalkable(TargetField* fields, int tiles)
{
   for (int k=0; k < tiles; ++k)
   {
      int lastRow = rows - 1 - k;
      int lastCol = cols - 1 - k;
      //std::map<BWAPI::TilePosition, TargetField*>::iterator mit;

      //for speed, make all border area unwalkable at the end
      //top row
      for (int c=0; c<cols; ++c)
      {
         //walkable[index(c,0)] = false;
         //for (mit = mFields.begin(); mit != mFields.end(); mit++)
         //{
         //   mit->second->setAngle(index(c,0), M_PI/2.0);
         //}
         fields->skewAngle(index(c,k), M_PI/2.0, 1.0); 
      }
      //bottom row
      for (int c=0; c<cols; ++c)
      {
         //walkable[index(c,rowsM1)] = false;
         //for (mit = mFields.begin(); mit != mFields.end(); mit++)
         //{
         //   mit->second->setAngle(index(c,rowsM1), 3.0*M_PI/2.0);
         //}
         fields->skewAngle(index(c,lastRow), 3.0*M_PI/2.0, 1.0); 
      }
      //left edge (except corners, already done)
      for (int r=(1+k); r<lastRow; ++r)
      {
         //walkable[index(0,r)] = false;
         //for (mit = mFields.begin(); mit != mFields.end(); mit++)
         //{
         //   mit->second->setAngle(index(0,r), 0.0);
         //}
         fields->skewAngle(index(0,r), 0.0, 1.0); 
      }
      //right edge (except corners, already done)
      for (int r=(1+k); r<lastRow; ++r)
      {
         //walkable[index(colsM1,r)] = false;
         //for (mit = mFields.begin(); mit != mFields.end(); mit++)
         //{
         //   mit->second->setAngle(index(colsM1,r), M_PI);
         //}
         fields->skewAngle(index(lastCol,r), M_PI, 1.0); 
      }
   }
}


void FlowField::GenerateFields(BWAPI::TilePosition pos, TargetField* fields)
{
   int size = rows*cols;
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

   //populate the flood fill, while we still have things left to expand
	while (fringeIndex < fringeSize)
	{
		// grab the current index to expand from the fringe
		currentIndex = fringe[fringeIndex++];
      char curD = fields->moveTo[currentIndex];

      switch(curD)
      {
      case 'U':
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
		   // search down
		   nextIndex = ((currentIndex + cols) < size) ? (currentIndex + cols) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'U';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
      }break;

      case 'D':
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
      }break;

      case 'R':
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
      }break;

      case 'L':
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
		   // search right
		   nextIndex = ((currentIndex % cols) < (cols-1)) ? (currentIndex + 1) : -1;
		   if (fields->unExpanded(nextIndex) && isWalkable(nextIndex)) {
            fields->moveTo[nextIndex] = 'L';
			   fringe[fringeSize++] = nextIndex;   // put it in the fringe
		   }
      }break;

      default:
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
      };
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
            fields->setAngle(i, angle);
         }
      }
   }

   ShrinkWalkableArea(fields, 2);
   MakeBordersUnWalkable(fields, 2);
}


FlowField::TargetField* FlowField::GenerateFields(BWAPI::TilePosition pos)
{
   TargetField* fields = new TargetField(rows*cols);  //return this
   GenerateFields(pos, fields);
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
   //float xAdj = (x>=0)?(0.5f):(-0.5f);
   //float yAdj = (y>=0)?(0.5f):(-0.5f);
   bool pathable = true;
   while (cur < L && pathable)
   {
    //pathable = isWalkable(c1+int(x*cur+xAdj), r1+int(y*cur+yAdj));
      pathable = isWalkable(c1+round(x*cur), r1+round(y*cur));
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
               int x2 = x1 + round(7.0*cos(angle));
               int y2 = y1 + round(7.0*sin(angle));
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
   
   int i = index(pos.x()/8,pos.y()/8);
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


bool FlowField::GetFlowFromTo(BWAPI::Position pos, BWAPI::TilePosition tgt, BWAPI::Position& moveHere, int pixelsAhead)
{
   double angle;
   if (GetFlowFromTo(pos, tgt, angle))
   {
      moveHere.x() = pos.x() + round(double(pixelsAhead) * cos(angle));
      moveHere.y() = pos.y() + round(double(pixelsAhead) * sin(angle));
      return true;
   }
   return false;
}


//assumes input in range [0,2*pi]
void FlowField::TargetField::setAngle(int i, double angle)
{
   //angle is in radians
   //cell == 0 is reserved to represent "not movable here"
   //so real angle values start at 1
   //scale angle to [0,254] and add in 1
   while (angle < 0) {
      angle += (2.0*M_PI);
   }
   cells[i] = unsigned char(1)+unsigned char(254.0*angle/(2.0*M_PI));
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


void FlowField::TargetField::skewAngle(int i, double angle, double scale)
{
   if (cells[i] != 0)   //will only skew if a valid angle already exists
   {
      double myAngle = getAngle(i);
      double cosA = ((cos(myAngle) * scale) + cos(angle))/(1.0 + scale);
      double sinA = ((sin(myAngle) * scale) + sin(angle))/(1.0 + scale);
      setAngle(i, atan2(sinA, cosA));
   }
}



bool FlowField::TargetField::isMovable(int i)
{
   //exploit the fact that cells==0 means 'not movable'
   return (cells[i] != 0);
}

