
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

   static std::vector<BWSAL::UnitGroup> GetClustersByCount(BWSAL::UnitGroup& group, int numClusters);
   static std::vector<BWSAL::UnitGroup> GetClustersByDistance(BWSAL::UnitGroup& group, int distThreshold);

private:

   static std::vector<ClusterNode>      GenerateDendrogram(BWSAL::UnitGroup& group);

   static std::vector<BWSAL::UnitGroup> SplitDendrogramIntoClusters(BWSAL::UnitGroup& group, 
                                                                    std::vector<ClusterNode> dendrogram,
                                                                    int numClusters);

   static int                             mLastN;
   static std::vector< std::vector<int> > mDistMatrix;
};

