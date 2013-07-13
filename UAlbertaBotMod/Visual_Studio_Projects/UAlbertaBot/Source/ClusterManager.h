
#include <BWAPI.h>
//#include <BWSAL.h>
#include <vector>


class ClusterManager
{
public:

   typedef struct {
      int left;
      int right;
      int distance;  //max = 2147483647
   } ClusterNode;

   struct NodeCompare {
       bool operator ()(ClusterNode const& a, ClusterNode const& b) const
       {
          return (a.distance < b.distance);
       }
   };

   static std::vector<std::vector<BWAPI::Unit*> > GetClustersByCount(const std::vector<BWAPI::Unit*>& group, int numClusters);
   static std::vector<std::vector<BWAPI::Unit*> > GetClustersByDistance(const std::vector<BWAPI::Unit*>& group, int distThreshold);

private:

   static std::vector<ClusterNode>      GenerateDendrogram(const std::vector<BWAPI::Unit*>& group);

   static std::vector<std::vector<BWAPI::Unit*>> SplitDendrogramIntoClusters(const std::vector<BWAPI::Unit*>& group, 
                                                                             std::vector<ClusterNode> dendrogram,
                                                                             int numClusters);

   static int                             mLastN;
   static std::vector< std::vector<int> > mDistMatrix;
};

