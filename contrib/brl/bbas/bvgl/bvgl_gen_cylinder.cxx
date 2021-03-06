#include "bvgl_gen_cylinder.h"
#include <bvrml/bvrml_write.h>
#include <vgl/vgl_intersection.h>
#include <vgl/vgl_distance.h>
#include <vgl/vgl_box_3d.h>
#include <vgl/vgl_bounding_box.h>
#include <vcl_cmath.h>
#include <vcl_limits.h>
bvgl_gen_cylinder::bvgl_gen_cylinder(vgl_cubic_spline_3d<double> const& axis, vcl_vector<bvgl_cross_section> const& cross_sects,
                                     double cross_section_interval):
  axis_(axis), cross_sections_(cross_sects), cross_section_interval_(cross_section_interval){
  for(vcl_vector<bvgl_cross_section>::const_iterator cit = cross_sects.begin();
      cit != cross_sects.end(); ++cit)
    bbox_.add(cit->bounding_box());
}
#if 0
void bvgl_gen_cylinder::load_cross_section_pointsets(vcl_ifstream& istr, double distance){
  cross_sections_.clear();
  // load the entire pointset
  vgl_pointset_3d<double> ptset;
  istr >> ptset;
  vcl_cout << "original pointset size " << ptset.npts() << '\n';
   // assign points to cross sections
  // scan through the axis knots
  unsigned nn = 0;
  double mt = axis_.max_t();
  for(double t = 0.0; t<=mt; t += cross_section_interval_){
    vgl_point_3d<double> p = axis_(t);
    vgl_point_3d<double> pc;
    if((t + cross_section_interval_) > mt)
      pc = axis_(t-cross_section_interval_);
    else
      pc = axis_(t+cross_section_interval_);
    double d = 0.6*((p-pc).length());
    vgl_plane_3d<double> norm_plane = axis_.normal_plane(t);
    vgl_pointset_3d<double> cpts = vgl_intersection(norm_plane, ptset, d), final_cpts;
    unsigned npts = cpts.npts();
    for(unsigned i = 0; i<npts; ++i){
      vgl_point_3d<double> pi = cpts.p(i);
      double r = (pi-p).length();
      if(r<=distance)
        if(cpts.has_normals())
          final_cpts.add_point_with_normal(pi, cpts.n(i));
        else
          final_cpts.add_point(pi);
    }
    bvgl_cross_section cs(t, p, norm_plane, final_cpts);
    nn +=final_cpts.npts();
    cross_sections_.push_back(cs);
    bbox_.add(cs.bounding_box());
  }
  vcl_cout << "cumulative cross section pointset size " << nn << '\n';
}
void bvgl_gen_cylinder::load_cross_section_pointsets(vcl_ifstream& istr, double distance){
  cross_sections_.clear();
  vgl_pointset_3d<double> ptset;
  // load the entire pointset
  istr >> ptset;
  if(!ptset.has_normals()){
    vcl_cout << "FATAL input pointset must have normals! \n";
    return;
  }
  vcl_vector<vgl_pointset_3d<double> > ptsets;
  vcl_vector<vgl_plane_3d<double> > cross_planes;
  vcl_vector<double> tset;
  vcl_vector<vgl_point_3d<double> > pset;
  // scan through the axis knots
  double mt = axis_.max_t();
  for(double t = 0.0; t<=mt; t += cross_section_interval_){
    vgl_point_3d<double> p = axis_(t);
    vgl_plane_3d<double> norm_plane = axis_.normal_plane(t);
    ptsets.push_back(vgl_pointset_3d<double>());
    cross_planes.push_back(norm_plane);
    tset.push_back(t);
    pset.push_back(p);
  }
  // scan through the pointset and assign each point to a cross section
  unsigned np = ptset.npts(), nc = static_cast<unsigned>(ptsets.size());
  for(unsigned i =0; i<np; ++i){
    vgl_point_3d<double> pi = ptset.p(i);
    vgl_vector_3d<double> ni = ptset.n(i);
    double min_dist = vcl_numeric_limits<double>::max();
    unsigned min_j = 0;
    for(unsigned j = 0; j<nc; ++j){
      double d = vgl_distance(pi, cross_planes[j]);
      if(d<min_dist){
        min_dist = d;
        min_j = j;
      }
    }
    ptsets[min_j].add_point_with_normal(pi, ni);
  }
  for(unsigned j = 0; j<nc; ++j){
    bvgl_cross_section cs(tset[j], pset[j], cross_planes[j], ptsets[j]);
    cross_sections_.push_back(cs);
    bbox_.add(cs.bounding_box());
  }
}
#endif

void bvgl_gen_cylinder::load_cross_section_pointsets(vcl_ifstream& istr){
  cross_sections_.clear();
  vgl_pointset_3d<double> ptset;
  // load the entire pointset
  istr >> ptset;
  if(!ptset.has_normals()){
    vcl_cout << "FATAL input pointset must have normals! \n";
    return;
  }
  vcl_vector<vgl_pointset_3d<double> > ptsets;
  vcl_vector<vgl_plane_3d<double> > cross_planes;
  vcl_vector<double> tset;
  vcl_vector<vgl_point_3d<double> > pset;
  // scan through the axis knots and set up elements of the cross section data
  double mt = axis_.max_t();
  for(double t = 0.0; t<=mt; t += cross_section_interval_){
    vgl_point_3d<double> p = axis_(t);
    vgl_plane_3d<double> norm_plane = axis_.normal_plane(t);
    ptsets.push_back(vgl_pointset_3d<double>());
    cross_planes.push_back(norm_plane);
    tset.push_back(t);
    pset.push_back(p);
  }

  // scan through the pointset and assign each point to a cross section
  unsigned np = ptset.npts(), nc = static_cast<unsigned>(ptsets.size());
  for(unsigned i =0; i<np; ++i){
    vgl_point_3d<double> pi = ptset.p(i);
    vgl_vector_3d<double> ni = ptset.n(i);
    double min_dist = vcl_numeric_limits<double>::max();
    unsigned min_j = 0;
    double dp, dk, d;
    for(unsigned j = 0; j<nc; ++j){
      dp = vgl_distance(pi, cross_planes[j]);
      dk = vgl_distance(pi, pset[j]);
      d = dp+ dk;
      if(d<min_dist){
        min_dist = d;
        min_j = j;
      }
    }
    ptsets[min_j].add_point_with_normal(pi, ni);
  }
  for(unsigned j = 0; j<nc; ++j){
    bvgl_cross_section cs(tset[j], pset[j], cross_planes[j], ptsets[j]);
    cross_sections_.push_back(cs);
    bbox_.add(cs.bounding_box());
  }
}
vcl_vector<unsigned> bvgl_gen_cylinder::cross_section_contains(vgl_point_3d<double> const& p) const{
  vcl_vector<unsigned> ret;
  unsigned nc = static_cast<unsigned>(cross_sections_.size());
  for(unsigned i = 0; i<nc; ++i)
    if(cross_sections_[i].contains(p))
      ret.push_back(i);
  return ret;
}

  bool bvgl_gen_cylinder::closest_point(vgl_point_3d<double> const& p, vgl_point_3d<double>& pc, double dist_thresh) const{
  double big = vcl_numeric_limits<double>::max();
  pc.set(big, big, big);
   vcl_vector<unsigned> crx_indices = this->cross_section_contains(p);
   if(!crx_indices.size())
     return false;
   double d_close = big;
   vgl_point_3d<double> closest_pt;
   for(vcl_vector<unsigned>::iterator cit = crx_indices.begin();
       cit != crx_indices.end(); ++cit){
     vgl_point_3d<double> cp = cross_sections_[*cit].closest_point(p, dist_thresh);
     double d = (p-cp).length();
     if(d<d_close){
       d_close = d;
       closest_pt = cp;
     }
   }
   pc = closest_pt;
   return true;
}

double bvgl_gen_cylinder::surface_distance(vgl_point_3d<double> const& p, double dist_thresh) const{
  vgl_point_3d<double> cp;
  if(!this->closest_point(p, cp, dist_thresh))
    return vcl_numeric_limits<double>::max();
  return (p-cp).length();
}

vgl_pointset_3d<double> bvgl_gen_cylinder::aggregate_pointset() const{
  unsigned n = static_cast<unsigned>(cross_sections_.size());
  vgl_pointset_3d<double> aggregate_ptset;
  for(unsigned i = 0; i<n; i++){
    vgl_pointset_3d<double> ps = cross_sections_[i].pts();
    unsigned np = ps.npts();
    for(unsigned j = 0; j<np; ++j)
      aggregate_ptset.add_point_with_normal(ps.p(j), ps.n(j));
  }
  return aggregate_ptset;
}

void bvgl_gen_cylinder::display_axis_spline(vcl_ofstream& ostr) const{
  bvrml_write::write_vrml_header(ostr);
  // display the knots
  vcl_vector<vgl_point_3d<double> > knots = axis_.knots();
  unsigned n = static_cast<unsigned>(knots.size());
  double nd = static_cast<double>(n-1);
  float r = 1.0f;
  for(unsigned i =0; i<n; ++i){
    vgl_point_3d<double> p = knots[i];
    vgl_point_3d<float> pf(static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()));
    vgl_sphere_3d<float> sp(pf, r);
    bvrml_write::write_vrml_sphere(ostr, sp, 0.0f, 1.0f, 0.0f);
  }
  // display the spline points
  for(double t = 0; t<=nd; t+=0.05){
    vgl_point_3d<double> p = axis_(t);
    vgl_point_3d<float> pf(static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()));
    vgl_sphere_3d<float> sp(pf, 0.25f);
    bvrml_write::write_vrml_sphere(ostr, sp, 1.0f, 1.0f, 0.0f);
  }
  ostr.close();
}
void bvgl_gen_cylinder::display_cross_section_planes(vcl_ofstream& ostr) const{
  bvrml_write::write_vrml_header(ostr);
  unsigned n = static_cast<unsigned>(cross_sections_.size());
  for(unsigned i = 0; i<n; i++)
    cross_sections_[i].display_cross_section_plane(ostr);
  ostr.close();
}

void bvgl_gen_cylinder::display_cross_section_pointsets(vcl_ofstream& ostr) const{
  bvrml_write::write_vrml_header(ostr);
  unsigned n = static_cast<unsigned>(cross_sections_.size());
  for(unsigned i = 0; i<n; i++)
    cross_sections_[i].display_cross_section_pts(ostr);
  ostr.close();
}

void bvgl_gen_cylinder::display_surface_disks(vcl_ofstream& ostr) const{
  bvrml_write::write_vrml_header(ostr);
  unsigned n = static_cast<unsigned>(cross_sections_.size());
  for(unsigned i = 0; i<n; i++)
    cross_sections_[i].display_cross_section_normal_disks(ostr);
  ostr.close();
}
