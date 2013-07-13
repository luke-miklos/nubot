
#include "FlockManager.h"
#include "FlowField.hpp"
#include "ClusterManager.h"
#include <map>

int                             FlockManager::mLastN = 0;
std::vector< std::vector<int> > FlockManager::mDistMatrix;


BWAPI::Position FlockManager::Neighbors::CenterOfMass()
{
   int count = 0;
   BWAPI::Position com(0,0);
   if (forward != 0) {
      com += forward->getPosition();
      count++;
   }
   if (back != 0) {
      com += back->getPosition();
      count++;
   }
   if (left != 0) {
      com += left->getPosition();
      count++;
   }
   if (right != 0) {
      com += right->getPosition();
      count++;
   }
   if (count>1)
   {
      com.x() /= count;
      com.y() /= count;
   }
   return com;
}



void FlockManager::executeMicro(const UnitVector & targets)
{
   //if (dirty)
   //{
   //   dirty = false; //row allocations made already
      InitializeFlock();
   //}

   //Position getPosition() const;  //(x,y) coordinates of unit (in pixels)
   //int getLeft() const;           //x position of left (right = increasing x)
   //int getTop() const;            //y position of top (down = increasing y)
   //int getRight() const;          //x position of right (right = increasing x)
   //int getBottom() const;         //y position of bottom (down = increasing y)
   //double getAngle() const;       //radians, 0=east
   //double getVelocityX() const;   //pixels/frame
   //double getVelocityY() const;   //pixels/frame

   int R = mFlockRows.size();
   for (int r=0; r < R; ++r)
   {
      int C = mFlockRows[r].size();
      for (int c=0; c < C; ++c)
      {
         BWAPI::Unit* unit = mFlockRows[r][c].unitPtr;

         if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount()) {
		      continue;
	      }

         BWAPI::Position pos = unit->getPosition();
         BWAPI::Broodwar->drawTextMap(pos.x(), pos.y(), "%d", (c+r*10));

         BWAPI::Position tgt = pos;
         double angle;
         double cosAngle = 0.0;
         double sinAngle = 0.0;
         double flowPixels = 13.0;
       //double unitDistance = 32.0;
       //double unitDistance = 48.0;
         double unitDistance = 40.0;

         ////get flow direction (main component of target location)
         //FlowField::Instance()->GetFlowFromTo(pos, BWAPI::TilePosition(order.getPosition()), tgt);
         if (! FlowField::Instance()->GetFlowFromTo(pos, BWAPI::TilePosition(order.getPosition()), angle))
         {
            angle = unit->getAngle();
         }
         cosAngle = cos(angle);
         sinAngle = sin(angle);
         tgt.x() = pos.x() + round(flowPixels * cosAngle);
         tgt.y() = pos.y() + round(flowPixels * sinAngle);

         if (drawLines) {
            BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), tgt.x(), tgt.y(), BWAPI::Colors::Black);
         }

       //  BWAPI::Position dest = tgt;
	      //if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move)
       //  {
       //     dest = unit->getLastCommand().getTargetPosition();
       //  }
       //  BWAPI::Position destVec = dest - pos;

         //get cohesion component
         int count = 0;
         BWAPI::Position vec(round(unitDistance*cosAngle), round(unitDistance*sinAngle));
         BWAPI::Position vec2(vec.y(), -vec.x());
         BWAPI::Position center(0,0);
         std::map<BWAPI::Unit*,int>::iterator rit;

         Neighbors n = mNeighbors[r][c];
         if (n.forward != 0)// && IsInRange(n.forward))  //only include in cohesion if its within reasonable range
         {
            //count++;
            //center += (n.forward->getPosition() - vec);
            double nAngle = 0;
            if (! FlowField::Instance()->GetFlowFromTo(n.forward->getPosition(), BWAPI::TilePosition(order.getPosition()), nAngle))
            {
               nAngle = n.forward->getAngle();
            }
            count++;
            center.x() += (n.forward->getPosition().x() - round(unitDistance*cos(nAngle)));
            center.y() += (n.forward->getPosition().y() - round(unitDistance*sin(nAngle)));
         }
         if (n.back != 0)// && IsInRange(n.back))  //only include in cohesion if its within reasonable range
         {
            //count++;
            //center += (n.back->getPosition() + vec);
            double nAngle = 0;
            if (! FlowField::Instance()->GetFlowFromTo(n.back->getPosition(), BWAPI::TilePosition(order.getPosition()), nAngle))
            {
               nAngle = n.back->getAngle();
            }
            count++;
            center.x() += (n.back->getPosition().x() + round(unitDistance*cos(nAngle)));
            center.y() += (n.back->getPosition().y() + round(unitDistance*sin(nAngle)));
         }
         if (n.left != 0)// && IsInRange(n.left))  //only include in cohesion if its within reasonable range
         {
            //count++;
            //center += (n.left->getPosition() - vec2);
            double nAngle = 0;
            if (! FlowField::Instance()->GetFlowFromTo(n.left->getPosition(), BWAPI::TilePosition(order.getPosition()), nAngle))
            {
               nAngle = n.left->getAngle();
            }
            count++;
            center.x() += (n.left->getPosition().x() - round(unitDistance*sin(nAngle)));
            center.y() += (n.left->getPosition().y() + round(unitDistance*cos(nAngle)));
         }
         if (n.right != 0)// && IsInRange(n.right))  //only include in cohesion if its within reasonable range
         {
            //count++;
            //center += (n.right->getPosition() + vec2);
            double nAngle = 0;
            if (! FlowField::Instance()->GetFlowFromTo(n.right->getPosition(), BWAPI::TilePosition(order.getPosition()), nAngle))
            {
               nAngle = n.right->getAngle();
            }
            count++;
            center.x() += (n.right->getPosition().x() + round(unitDistance*sin(nAngle)));
            center.y() += (n.right->getPosition().y() - round(unitDistance*cos(nAngle)));
         }
         if (count > 0)
         {
            center.x() /= count;
            center.y() /= count;
            //BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), pos.x()+force.x(), pos.y()+force.y(), BWAPI::Colors::Yellow);
            vec = (center - pos);

            if (drawLines) {
               BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), pos.x()+vec.x(), pos.y()+vec.y(), BWAPI::Colors::Yellow);
            }

            int x = vec.x();
            int y = vec.y();
            double mag = sqrt(double(x*x + y*y));
            //if (mag > 13)
            //{
            //   force.x() = round((double(force.x()*13)/mag) - 3.25*double(unit->getVelocityX())); //velocity = [0,4] -> *3.25 to translate to max 13 pixels
            //   force.y() = round((double(force.y()*13)/mag) - 3.25*double(unit->getVelocityY())); //velocity = [0,4] -> *3.25 to translate to max 13 pixels
            //}
            //else
            //{
            //   force.x() = round(double(force.x()) - 3.25*double(unit->getVelocityX())); //velocity = [0,4] -> *3.25 to translate to max 13 pixels
            //   force.y() = round(double(force.y()) - 3.25*double(unit->getVelocityY())); //velocity = [0,4] -> *3.25 to translate to max 13 pixels
            //}
            if (mag > 13)
            {
               vec.x() = round(double(vec.x())*6.5/mag);
               vec.y() = round(double(vec.y())*6.5/mag);
            }
            else
            {
               vec.x() = round(double(vec.x())/2.0);
               vec.y() = round(double(vec.y())/2.0);
            }

            //BWAPI::Position test(tgt);
            //test += force;
            //if (FlowField::Instance()->validDestination(test))
            //{
            //   tgt += force;
            //}
            tgt += vec;
         }
       //BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), tgt.x(), tgt.y(), BWAPI::Colors::White);


         //get separation component

         count = 0;
         float pushX = 0.0f;
         float pushY = 0.0f;
         //BWAPI::Position totalForce(0,0);
         //get all neighbors within neighbor radius
       //int neighborRadius = 42;
         int neighborRadius = int(unitDistance*3/2);
         int neighborRadiusSquared = neighborRadius*neighborRadius;
         //use: mDistMatrix[][];
         int index = mDistMatrixIndexMap[unit];
         const std::vector<BWAPI::Unit*> units = getUnits();
         for (int i=0; i<index; ++i)
         {
            if (mDistMatrix[index][i] < neighborRadiusSquared)
            {
               count++;
               //add to separation push force
               float x = float(pos.x() - units[i]->getPosition().x());
               float y = float(pos.y() - units[i]->getPosition().y());
               float mag = sqrt(x*x + y*y);
               float newMag = float(neighborRadius) - mag;
               pushX += x * newMag / mag;
               pushY += y * newMag / mag;
               //push += (pos - units[i]->getPosition());
            }
         }
         for (int i=(index+1); i<(int)units.size(); ++i)
         {
            if (mDistMatrix[i][index] < neighborRadiusSquared)
            {
               count++;
               //add to separation push force
               float x = float(pos.x() - units[i]->getPosition().x());
               float y = float(pos.y() - units[i]->getPosition().y());
               float mag = sqrt(x*x + y*y);
               float newMag = float(neighborRadius) - mag;
               pushX += x * newMag / mag;
               pushY += y * newMag / mag;
               //push += (pos - units[i]->getPosition());
            }
         }
         if (count > 0)
         {
            //pushForce.x() /= (2*count);   //max force is half of radius this way (14 pixels)
            //pushForce.y() /= (2*count);   //max force is half of radius this way (14 pixels)

            //push.x() = round((double(push.x()*13.0)/double(neighborRadius*count)));
            //push.y() = round((double(push.y()*13.0)/double(neighborRadius*count)));

            BWAPI::Position push(round(pushX/count), round(pushY/count));

            if (drawLines) {
               BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), pos.x()+push.x(), pos.y()+push.y(), BWAPI::Colors::Purple);
            }
            //BWAPI::Position test(tgt);
            //test += pushForce;
            //if (FlowField::Instance()->validDestination(test))
            //{
            //   tgt += pushForce;
            //}
            tgt += push;
         }
        
         ////calculate push force for separation
         //BWAPI::Position pushForce = pos - neighbor->getPosition();
         //totalForce += (1 - pushForce / pushForce.getLength());
         //totalForce /= neighbor->count();
         //totalForce += maxForce;

         BWAPI::Position wallForce = FlowField::Instance()->GetVectorFromWalls(unit, 7.0);
         if (drawLines) {
            BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), pos.x()+wallForce.x(), pos.y() + wallForce.y(), BWAPI::Colors::Red);
         }
         tgt += wallForce;


         if (drawLines) {
            BWAPI::Broodwar->drawLineMap(pos.x(), pos.y(), tgt.x(), tgt.y(), BWAPI::Colors::White);
         }

         unit->move(tgt);

      }
   }
}


void FlockManager::InitializeFlock()
{
   const std::vector<BWAPI::Unit*> units = getUnits();

   //expand distance matrix if necessary
   int N = units.size();
   if (N > mLastN)
   {
      mDistMatrix.resize(N);
      for (int i=mLastN; i<N; ++i)
      {
         mDistMatrix[i].resize(i);  //should never use last item (zero difference from self)
      }
      mLastN = N;
   }

   //populate distance matrix
   BWAPI::Position pi, pj;
   for (int i=0; i<N; ++i)
   {
      mDistMatrixIndexMap[units[i]] = i;
      pi = units[i]->getPosition();
      for (int j=0; j<i; ++j)
      {
         pj = units[j]->getPosition();
         int x = pi.x() - pj.x();
         int y = pi.y() - pj.y();
         int d2 = x*x + y*y;
         mDistMatrix[i][j] = d2;
      }
   }

   //calculate unit cluster for this squad
   ClearOutOfRange();
 //std::vector<std::vector<BWAPI::Unit*> > clusters = ClusterManager::GetClustersByDistance(units, 128); //4 tiles away
   std::vector<std::vector<BWAPI::Unit*> > clusters = ClusterManager::GetClustersByDistance(units, 96);  //3 build-tiles away
   int max = 0;
   int maxIndex = 0;
   if (clusters.size() <= 0)
   {
      return;
   }
   if (clusters.size() > 1)
   {
      //find largest cluster (use it as main squad)
      for (int i=0; i<(int)clusters.size(); ++i)
      {
         if ((int)clusters[i].size() > max)
         {
            max = (int)clusters[i].size();
            maxIndex = i;
         }
      }

      //get centroid of largest cluster
      BWAPI::Position center(0,0);
      for (int i=0; i<(int)clusters[maxIndex].size(); ++i)
      {
         center += clusters[maxIndex][i]->getPosition();
      }
      center.x() /= clusters[maxIndex].size();
      center.y() /= clusters[maxIndex].size();
      //send all units of non-largest cluster to the centroid
      for (int i=0; i<(int)clusters.size(); ++i)
      {
         if (i != maxIndex)
         {
            for (int j=0; j<(int)clusters[i].size(); ++j)
            {
               clusters[i][j]->move(center);
               SetOutOfRange(clusters[i][j]);
            }
         }
      }
   }

   std::sort(clusters[maxIndex].begin(), clusters[maxIndex].end()); //for comparison later to see if main cluster has changed
   if (clusters[maxIndex].size() == mCluster.size())
   {
      if (std::equal(clusters[maxIndex].begin(), clusters[maxIndex].end(), mCluster.begin()))
      {
         //the units in the main cluster have not changed, so do not change the flock rows or neighbors
         return;
      }
   }

   //otherwise, update the flock rows & neighbors
   mCluster = clusters[maxIndex];

   mNeighbors.clear();
   mFlockRows.clear();

   BWAPI::Position origin;
   double angle = 0.0;

   //figure out who is in what row (8 units wide for small, 6 wide for medium, 4 wide for large)
   if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Protoss)
   {
      //zealots in front rows, 8 wide

      //dragoons in back rows, 4 wide

   }
   else if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Terran)
   {
      std::vector<SortedUnit> firebats;
      std::vector<SortedUnit> medics;
      std::vector<SortedUnit> marines;

      BWAPI::TilePosition tgt(order.getPosition());
      double avgCos = 0.0;
      double avgSin = 0.0;
      int count = 0;

    //std::vector<BWAPI::Unit*>::const_iterator it = getUnits().begin();
      std::vector<BWAPI::Unit*>::iterator it = mCluster.begin();
      for (; it!=mCluster.end(); it++)
      {
         BWAPI::UnitType type = (*it)->getType();
         switch(type.getID())
         {
         case 32: //firebats in front  (8 wide)
            {
               firebats.push_back(SortedUnit(*it));
            }break;
         case 34: //medics next        (8 wide)
            {
               medics.push_back(SortedUnit(*it));
            }break;
         case 0:  //rines next         (8 wide)
            {
               marines.push_back(SortedUnit(*it));
            }break;
         default: //unknown unit, nowhere
            {

            }break;
         };

         if (FlowField::Instance()->GetFlowFromTo((*it)->getPosition(), tgt, angle))
         {
            avgCos += cos(angle);
            avgSin += sin(angle);
            count++;
         }
      }
      if (count>0)
      {
         avgCos /= count;
         avgSin /= count;
         angle = atan2(avgSin, avgCos);
      }

      //sort units of each type front to back
      origin = mCluster[0]->getPosition();
      //FlowField::Instance()->GetFlowFromTo(origin, BWAPI::TilePosition(order.getPosition()), angle);
      //angle += M_PI; //face angle backwards so smaller numbers are up "front"
      float normLineX = float(cos(angle+M_PI));
      float normLineY = float(sin(angle+M_PI));

      if (drawLines) {
         BWAPI::Broodwar->drawLineMap(origin.x(), origin.y(), origin.x()+int(200.0f*normLineX), origin.y()+int(200.0f*normLineY), BWAPI::Colors::Blue);
      }

      //sort firebats
      for (int i=0; i<(int)firebats.size(); ++i)
      {
         BWAPI::Unit* unit = firebats[i].unitPtr;
         float pointX = float(unit->getPosition().x() - origin.x());
         float pointY = float(unit->getPosition().y() - origin.y());
         firebats[i].key = (normLineX*pointX + normLineY*pointY);  //dot product
      }
      std::sort(firebats.begin(),firebats.end());
      //sort medics
      for (int i=0; i<(int)medics.size(); ++i)
      {
         BWAPI::Unit* unit = medics[i].unitPtr;
         float pointX = float(unit->getPosition().x() - origin.x());
         float pointY = float(unit->getPosition().y() - origin.y());
         medics[i].key = (normLineX*pointX + normLineY*pointY);  //dot product
      }
      std::sort(medics.begin(),medics.end());
      //sort marines
      for (int i=0; i<(int)marines.size(); ++i)
      {
         BWAPI::Unit* unit = marines[i].unitPtr;
         float pointX = float(unit->getPosition().x() - origin.x());
         float pointY = float(unit->getPosition().y() - origin.y());
         marines[i].key = (normLineX*pointX + normLineY*pointY);  //dot product
      }
      std::sort(marines.begin(),marines.end());

      //TODO: intersperse medics among all units instead of putting them at middle???
      firebats.insert(firebats.end(), medics.begin(), medics.end());
      firebats.insert(firebats.end(), marines.begin(), marines.end());
      int F = (int)firebats.size();
      while (F > 0)
      {
         if (F>8)
         {
            std::vector<SortedUnit> row(firebats.begin(), firebats.begin()+8);
            firebats.erase(firebats.begin(), firebats.begin()+8);
            mFlockRows.push_back(row);
            mNeighbors.push_back(std::vector<Neighbors>(8));
         }
         else
         {
            mFlockRows.push_back(firebats);
            mNeighbors.push_back(std::vector<Neighbors>(F));
         }
         F -= 8;
      }
      //tanks in back ???
   }
   else //if (BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg)
   {
      //lurkers in front ???
      //lings in front
      //lurkers in mid ???
      //hydra in back
   }


   float normLineX = float(cos(angle+(M_PI/2.0)));
   float normLineY = float(sin(angle+(M_PI/2.0)));
   if (drawLines) {
      BWAPI::Broodwar->drawLineMap(origin.x(), origin.y(), origin.x()+int(200.0f*normLineX), origin.y()+int(200.0f*normLineY), BWAPI::Colors::Teal);
   }

   //sort rows side to side
   for (int r=0; r < (int)mFlockRows.size(); ++r)
   {
      //BWAPI::Position origin = mFlockRows[r][0].unitPtr->getPosition();
      //mFlockRows[r][0].key = 0.0f;
      //double angle = 0.0;
      //FlowField::Instance()->GetFlowFromTo(origin, BWAPI::TilePosition(order.getPosition()), angle);
      for (int c=0; c < (int)mFlockRows[r].size(); ++c)
      {
         BWAPI::Unit* unit = mFlockRows[r][c].unitPtr;
         float pointX = float(unit->getPosition().x() - origin.x());
         float pointY = float(unit->getPosition().y() - origin.y());
         mFlockRows[r][c].key = (normLineX*pointX + normLineY*pointY);  //dot product
      }
      std::sort(mFlockRows[r].begin(), mFlockRows[r].end());
   }

   //generate neighbors
   int R = mFlockRows.size();
   for (int r=0; r < R; ++r)
   {
      int C = mFlockRows[r].size();
      int fro = (r>0)?(mFlockRows[r-1].size()):(0);
      int bac = ((r+1)<R)?(mFlockRows[r+1].size()):(0);
      for (int c=0; c < C; ++c)
      {
         //BWAPI::Unit* unit = mFlockRows[r][c];
         //try to set forward neighbor
         if (r>0 && c < fro) {
            mNeighbors[r][c].forward = mFlockRows[r-1][c].unitPtr;
         }
         //try to set backwards neighbor
         if ((r+1)<R && c < bac) {
            mNeighbors[r][c].back    = mFlockRows[r+1][c].unitPtr;
         }
         //try to set left neighbor
         if (c>0) {
            mNeighbors[r][c].left    = mFlockRows[r][c-1].unitPtr;
         }
         //try to set right neighbor
         if ((c+1) < C) {
            mNeighbors[r][c].right   = mFlockRows[r][c+1].unitPtr;
         }

         ////try to set left neighbor
         //if (c==1) {
         //   mNeighbors[r][c].left    = mFlockRows[r][0].unitPtr;
         //} else if (c==3 || c==5 || c==7) {
         //   mNeighbors[r][c].left    = mFlockRows[r][c-2].unitPtr;
         //} else if (c!=6 && (c+2) < C) {
         //   mNeighbors[r][c].left    = mFlockRows[r][c+2].unitPtr;
         //}
         ////try to set right neighbor
         //if (c==0 && C>1) {
         //   mNeighbors[r][c].right   = mFlockRows[r][1].unitPtr;
         //} else if (c==2 || c==4 || c==6) {
         //   mNeighbors[r][c].right   = mFlockRows[r][c-2].unitPtr;
         //} else if (c!=7 && (c+2) < C) {
         //   mNeighbors[r][c].right   = mFlockRows[r][c+2].unitPtr;
         //}
      }
   }
}

