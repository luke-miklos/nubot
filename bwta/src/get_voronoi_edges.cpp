#include "functions.h"
#include "Color.h"
#include "VertexData.h"
namespace BWTA
{
  bool inside_a_polygon(PointD pt, const std::vector<Polygon> &polygons)
  {
    for(unsigned int p=0;p<polygons.size();p++)
      if (polygons[p].isInside(BWAPI::Position(int(pt.x()),int(pt.y())))==CGAL::ON_BOUNDED_SIDE)
        return true;
    return false;
  }
  void get_voronoi_edges(SDG2 &sdg,std::vector<Segment> &voronoi_diagram_edges, std::map<Point, std::set<Point>, ptcmp > &nearest,std::map<Point,double, ptcmp> &distance, const std::vector<Polygon> &polygons)
  {
    Point_set_2 pointset;
    SDG2::Construct_sdg_bisector_segment_2 get_edge=sdg.construct_sdg_bisector_segment_2_object();

    SDG2::Finite_edges_iterator eit = sdg.finite_edges_begin();
    for (int k = 1; eit != sdg.finite_edges_end(); ++eit, ++k) {
      SDG2::Edge e = *eit;
      // get the vertices defining the Voronoi edge
      SDG2::Vertex_handle v[] = { e.first->vertex( sdg.ccw(e.second) ),
                                  e.first->vertex( sdg.cw(e.second) ),
                                  e.first->vertex( e.second ),
                                  sdg.tds().mirror_vertex(e.first, e.second) };

      if (!sdg.is_infinite(v[0])  && !sdg.is_infinite(v[1]) && !sdg.is_infinite(v[2])  && !sdg.is_infinite(v[3]))
      {
        CGAL::Object edge=get_edge(v[0]->site(),v[1]->site(),v[2]->site(),v[3]->site());
        SegmentD s;
        if (CGAL::assign(s,edge))
        {
          if (s.vertex(0).x()!=s.vertex(1).x() || s.vertex(0).y()!=s.vertex(1).y())
          {
            if (is_real(s.vertex(0).x())!=0 && is_real(s.vertex(0).y())!=0
             && is_real(s.vertex(1).x())!=0 && is_real(s.vertex(1).y())!=0)
            {
              PointD p1(cast_to_double(s.vertex(0).x()),cast_to_double(s.vertex(0).y()));
              PointD p2(cast_to_double(s.vertex(1).x()),cast_to_double(s.vertex(1).y()));
              if (inside_a_polygon(p1,polygons)==false && inside_a_polygon(p2,polygons)==false)
              {
                bool added=false;
                Point v0(s.vertex(0).x(),s.vertex(0).y());
                Point v1(s.vertex(1).x(),s.vertex(1).y());
                Vertex_handle n0=pointset.nearest_neighbor(CGAL::Point_2< Kernel >(v0.x(),v0.y()));
                if (n0!=NULL && get_distance(Point(n0->point().x(),n0->point().y()),v0)<0.1)
                {
                  v0=Point(n0->point().x(),n0->point().y());
                }
                else
                {
                  pointset.insert(CGAL::Point_2< Kernel >(v0.x(),v0.y()));
                }
                Vertex_handle n1=pointset.nearest_neighbor(CGAL::Point_2< Kernel >(v1.x(),v1.y()));
                if (n1!=NULL && get_distance(Point(n1->point().x(),n1->point().y()),v1)<0.1)
                {
                  v1=Point(n1->point().x(),n1->point().y());
                }
                else
                {
                  pointset.insert(CGAL::Point_2< Kernel >(v1.x(),v1.y()));
                }
                if (v[0]->site().is_point() && v[1]->site().is_point())
                {
                  PointD nearest0_D(v[0]->site().point());
                  PointD nearest1_D(v[1]->site().point());
                  Point nearest0(nearest0_D.x(),nearest0_D.y());
                  Point nearest1(nearest1_D.x(),nearest1_D.y());
                  Point mid=CGAL::midpoint(nearest0,nearest1);
                  if ((v0.x()-mid.x())*(v1.x()-mid.x())+(v0.y()-mid.y())*(v1.y()-mid.y())<0)
                  {
                    added=true;
                    voronoi_diagram_edges.push_back(Segment(v0,mid));
                    voronoi_diagram_edges.push_back(Segment(mid,v1));
                    distance[v0]=sqrt(to_double((v0.x()-nearest0.x())*(v0.x()-nearest0.x())+(v0.y()-nearest0.y())*(v0.y()-nearest0.y())));
                    distance[mid]=sqrt(to_double( (mid.x()-nearest0.x())*(mid.x()-nearest0.x())+(mid.y()-nearest0.y())*(mid.y()-nearest0.y())));
                    distance[v1]=sqrt(to_double( (v1.x()-nearest0.x())*(v1.x()-nearest0.x())+(v1.y()-nearest0.y())*(v1.y()-nearest0.y())));
                    nearest[v0].insert(nearest0);  nearest[v0].insert(nearest1);
                    nearest[mid].insert(nearest0); nearest[mid].insert(nearest1);
                    nearest[v1].insert(nearest0);  nearest[v1].insert(nearest1);
                  }
                }
                if (!added)
                {
                  voronoi_diagram_edges.push_back(Segment(v0,v1));
                  Point nearest0;
                  Point nearest1;
                  if (v[0]->site().is_point())
                  {
                    nearest0=Point(v[0]->site().point().x(),v[0]->site().point().y());
                  }
                  else
                  {
                    nearest0=Line(Point(v[0]->site().segment().vertex(0).x(),v[0]->site().segment().vertex(0).y()),
                                  Point(v[0]->site().segment().vertex(1).x(),v[0]->site().segment().vertex(1).y())).projection(v0);
                  }
                  if (v[1]->site().is_point())
                  {
                    nearest1=Point(v[1]->site().point().x(),v[1]->site().point().y());
                  }
                  else
                  {
                    nearest1=Line(Point(v[1]->site().segment().vertex(0).x(),v[1]->site().segment().vertex(0).y()),
                                  Point(v[1]->site().segment().vertex(1).x(),v[1]->site().segment().vertex(1).y())).projection(v0);
                  }
                  distance[v0]=sqrt(cast_to_double( (v0.x()-nearest0.x())*(v0.x()-nearest0.x())+(v0.y()-nearest0.y())*(v0.y()-nearest0.y())));
                  nearest[v0].insert(nearest0); nearest[v0].insert(nearest1);
                  if (v[0]->site().is_point())
                  {
                    nearest0=Point(v[0]->site().point().x(),v[0]->site().point().y());
                  }
                  else
                  {
                    nearest0=Line(Point(v[0]->site().segment().vertex(0).x(),v[0]->site().segment().vertex(0).y()),
                                  Point(v[0]->site().segment().vertex(1).x(),v[0]->site().segment().vertex(1).y())).projection(v1);
                  }
                  if (v[1]->site().is_point())
                  {
                    nearest1=Point(v[1]->site().point().x(),v[1]->site().point().y());
                  }
                  else
                  {
                    nearest1=Line(Point(v[1]->site().segment().vertex(0).x(),v[1]->site().segment().vertex(0).y()),
                                  Point(v[1]->site().segment().vertex(1).x(),v[1]->site().segment().vertex(1).y())).projection(v1);
                  }
                  distance[v1]=sqrt(cast_to_double( (v1.x()-nearest0.x())*(v1.x()-nearest0.x())+(v1.y()-nearest0.y())*(v1.y()-nearest0.y())));
                  nearest[v1].insert(nearest0); nearest[v1].insert(nearest1);
                }
              }
            }
          }
        }
        CGAL::Parabola_segment_2<GtD> parabola;

        if (CGAL::assign(parabola,edge))
        {
          std::vector<Point> points;
          std::vector<PointD> pointsD;
          parabola.generate_points(pointsD);
          for(unsigned int i=0;i<pointsD.size();i++)
          {
            points.push_back(Point(pointsD[i].x(),pointsD[i].y()));
          }
          for(unsigned int i=0;i<points.size();i++)
          {
            Vertex_handle n=pointset.nearest_neighbor(CGAL::Point_2< Kernel >(points[i].x(),points[i].y()));
            if (n!=NULL && get_distance(Point(n->point().x(),n->point().y()),points[i])<0.1)
            {
              points[i]=Point(n->point().x(),n->point().y());
            }
            else
            {
              pointset.insert(CGAL::Point_2< Kernel >(points[i].x(),points[i].y()));
            }
          }
          for(unsigned int i=0;i+1<points.size();i++)
          {
            int j=i+1;
            if (points[i].x()!=points[j].x() || points[i].y()!=points[j].y())
            {
              if (is_real(points[i].x())!=0 && is_real(points[i].y())!=0
               && is_real(points[j].x())!=0 && is_real(points[j].y())!=0)
              {
                PointD p1(cast_to_double(points[i].x()),cast_to_double(points[i].y()));
                PointD p2(cast_to_double(points[j].x()),cast_to_double(points[j].y()));
                if (inside_a_polygon(p1,polygons)==false && inside_a_polygon(p2,polygons)==false)
                {
                  voronoi_diagram_edges.push_back(Segment(points[i],points[j]));
                }
              }
            }
          }
          for(unsigned int i=0;i<points.size();i++)
          {
            if (is_real(points[i].x())!=0 && is_real(points[i].y())!=0)
            {
              PointD p1(cast_to_double(points[i].x()),cast_to_double(points[i].y()));
              if (inside_a_polygon(p1,polygons)==false)
              {
                Point nearest0;
                Point nearest1;
                if (v[0]->site().is_point())
                {
                  nearest0=Point(v[0]->site().point().x(),v[0]->site().point().y());
                }
                else
                {
                  nearest0=Line(Point(v[0]->site().segment().vertex(0).x(),v[0]->site().segment().vertex(0).y()),
                                Point(v[0]->site().segment().vertex(1).x(),v[0]->site().segment().vertex(1).y())).projection(points[i]);
                }
                if (v[1]->site().is_point())
                {
                  nearest1=Point(v[1]->site().point().x(),v[1]->site().point().y());
                }
                else
                {
                  nearest1=Line(Point(v[1]->site().segment().vertex(0).x(),v[1]->site().segment().vertex(0).y()),
                                Point(v[1]->site().segment().vertex(1).x(),v[1]->site().segment().vertex(1).y())).projection(points[i]);
                }
                distance[points[i]]=get_distance(points[i],nearest0);
                nearest[points[i]].insert(nearest0);
                nearest[points[i]].insert(nearest1);
              }
            }
          }
        }
      }
    }
  }
}