
#include "ClusterManager.h"
#include <algorithm>

/*
This code based on the SLINK algorithm, described in:
Sibson, R (1973), SLINK: An optimally efficient algorithm for the single-link cluster method. 
*/

int                             ClusterManager::mLastN = 0;
std::vector< std::vector<int> > ClusterManager::mDistMatrix;

//static
std::vector<ClusterManager::ClusterNode> ClusterManager::GenerateDendrogram(const std::vector<BWAPI::Unit*>& group)
{
   //distance matrix (holds distance squared between each unit in the group)
   //int distmatrix[512][512];
   int N = group.size();
   if (N > mLastN)
   {
      mDistMatrix.resize(N);
      for (int i=mLastN; i<N; ++i)
      {
         //mDistMatrix[i].resize(i+1);
         mDistMatrix[i].resize(i);  //should never use last item (zero difference from self)
      }
      mLastN = N;
   }

   std::vector<BWAPI::Unit*>::const_iterator it = group.begin();
   std::vector<BWAPI::Unit*>::const_iterator jt;
   BWAPI::Position pi, pj;
   for (int i=0; i<N; ++i, it++)
   {
      //mDistMatrix[i][i] = 2147483647;   //this even needed?  //not part of matrix anymore
      pi = (*it)->getPosition();
      jt = group.begin();
      for (int j=0; j<i; ++j, jt++)
      {
         pj = (*jt)->getPosition();
         int x = pi.x() - pj.x();
         int y = pi.y() - pj.y();
         int d2 = x*x + y*y;
         mDistMatrix[i][j] = d2;
      }
   }
   int i, j, k;
   const int nnodes = N - 1;
   std::vector<int> temp(N-1);
   std::vector<int> index(N);
   std::vector<int> vec(N-1);
   std::vector<ClusterNode> tree(N);

   for (i = 0; i < nnodes; i++)
   {
      vec[i] = i;
   }

   for (i = 0; i < N; i++)
   {
      tree[i].distance = 2147483647;
      for (j = 0; j < i; j++)
      {
         temp[j] = mDistMatrix[i][j];
      }
      for (j = 0; j < i; j++)
      {
         k = vec[j];
         if (tree[j].distance >= temp[j])
         { 
            if (tree[j].distance < temp[k])
            {
               temp[k] = tree[j].distance;
            }
            tree[j].distance = temp[j];
            vec[j] = i;
         }
         else if (temp[j] < temp[k])
         {
            temp[k] = temp[j];
         }
      }
      for (j = 0; j < i; j++)
      {
        if (tree[j].distance >= tree[vec[j]].distance)
        {
           vec[j] = i;
        }
      }
   }

   for (i = 0; i < nnodes; i++)
   {
      tree[i].left = i;
   }
   std::sort(tree.begin(), tree.end(), NodeCompare());

   for (i = 0; i < N; i++)
   {
      index[i] = i;
   }
   for (i = 0; i < nnodes; i++)
   {
      j = tree[i].left;
      k = vec[j];
      tree[i].left = index[j];
      tree[i].right = index[k];
      index[k] = -i-1;
   }
   tree.resize(nnodes);  //drops last element
   return tree;
}



//static
std::vector<std::vector<BWAPI::Unit*> > ClusterManager::SplitDendrogramIntoClusters(const std::vector<BWAPI::Unit*>& group, 
                                                                                   std::vector<ClusterNode>   dendrogram,
                                                                                   int                        numClusters )
{
   std::vector<BWAPI::Unit*> groupArray;
   std::vector<BWAPI::Unit*>::const_iterator it;
   for (it = group.begin(); it != group.end(); it++)
   {
      groupArray.push_back(*it);
   }

   int i, j, k;
   std::vector<std::vector<BWAPI::Unit*> > clusters = std::vector<std::vector<BWAPI::Unit*> >(numClusters);
   int icluster = 0;
   int N = group.size();
   const int n = N-numClusters; //number of nodes to join
   //this loop looks for any leaf nodes that are clusters by themselves
   for (i = N-2; i >= n; i--)
   { 
      k = dendrogram[i].left;
      if (k>=0)
      {
         //clusters[icluster].insert(groupArray[k]);
         clusters[icluster].push_back(groupArray[k]);
         icluster++;
      }
      k = dendrogram[i].right;
      if (k>=0)
      { 
         //clusters[icluster].insert(groupArray[k]);
         clusters[icluster].push_back(groupArray[k]);
         icluster++;
      }
   }
   std::vector<int> nodeid = std::vector<int>(n, -1);
   for (i = n-1; i >= 0; i--)
   {
      if(nodeid[i]<0)
      {
         j = icluster;
         nodeid[i] = j;
         icluster++;
      }
      else
      {
         j = nodeid[i];
      }
      k = dendrogram[i].left;
      if (k<0)
      {
         nodeid[-k-1] = j;
      }
      else
      {
         //clusters[j].insert(groupArray[k]);
         clusters[j].push_back(groupArray[k]);
      }
      k = dendrogram[i].right;
      if (k<0)
      {
         nodeid[-k-1] = j;
      }
      else
      {
         //clusters[j].insert(groupArray[k]);
         clusters[j].push_back(groupArray[k]);
      }
   }
   return clusters;
}



//static 
std::vector<std::vector<BWAPI::Unit*> > ClusterManager::GetClustersByCount(const std::vector<BWAPI::Unit*>& group, int numClusters)
{
   std::vector<ClusterNode> dendrogram = GenerateDendrogram(group);
   return SplitDendrogramIntoClusters(group, dendrogram, numClusters);
}



//static 
std::vector<std::vector<BWAPI::Unit*> > ClusterManager::GetClustersByDistance(const std::vector<BWAPI::Unit*>& group, int distThreshold)
{
   std::vector<ClusterNode> dendrogram = GenerateDendrogram(group);
   //count how many tree splits are necessary because of distance threshold
   int numClusters = 1;
   int N = group.size();
 //for (int i = dendrogram.size()-1; i >= 0; i--)
   for (int i = N-2;                 i >= 0; i--)
   { 
      if (dendrogram[i].distance > (distThreshold*distThreshold))
      {
         numClusters++;
      }
      else
      {
         break;
      }
   }
   return SplitDendrogramIntoClusters(group, dendrogram, numClusters);
}
