#include <volm/conf/volm_conf_object.h>
//:
// \file
#include <vcl_cmath.h>
#include <vnl/vnl_math.h>
#include <vcl_limits.h>
#include <vpgl/vpgl_lvcs.h>
#include <bkml/bkml_write.h>
#include <volm/volm_category_io.h>

#define EPSILON 1E-5

  //The angular value is from 0 to 2*pi

volm_conf_object::volm_conf_object(float const& theta, float const& dist, float const& height, unsigned char const& land)
 : dist_(dist), land_(land), height_(height)
{
  theta_ = theta;
  while (theta_ > vnl_math::twopi)
    theta_ -= (float)vnl_math::twopi;
  while (theta_ < 0)
    theta_ += (float)vnl_math::twopi;
}

volm_conf_object::volm_conf_object(double const& theta, double const& dist, double const& height, unsigned char const& land)
  : land_(land)
{
  dist_ = (float)dist; height_ = (float)height;
  theta_ = (float)theta;
  while (theta_ > vnl_math::twopi)
    theta_ -= (float)vnl_math::twopi;
  while (theta_ < 0)
    theta_ += (float)vnl_math::twopi;
}

// construct by the location point represented as (x,y)
volm_conf_object::volm_conf_object(vgl_point_2d<float> const& pt, float const& height, unsigned char const& land)
  : land_(land), height_(height)
{
  dist_ = vcl_sqrt(pt.x()*pt.x()+pt.y()*pt.y());
  float theta = vcl_atan2(pt.y(),pt.x());
  theta_ = (theta < 0) ? theta + (float)vnl_math::twopi : theta;
}

volm_conf_object::volm_conf_object(vgl_point_2d<double> const& pt, double const& height, unsigned char const& land)
  : land_(land)
{
  height_ = (float)height;
  dist_ = (float)vcl_sqrt(pt.x()*pt.x()+pt.y()*pt.y());
  float theta = (float)vcl_atan2(pt.y(), pt.x());
  theta_ = (theta < 0) ? theta + (float)vnl_math::twopi : theta;
}

volm_conf_object::volm_conf_object(vgl_point_3d<float> const& pt, unsigned char const& land)
  : land_(land), height_(pt.z())
{
  dist_ = vcl_sqrt(pt.x()*pt.x()+pt.y()*pt.y());
  float theta = (float)vcl_atan2(pt.y(), pt.x());
  theta_ = (theta < 0) ? theta + (float)vnl_math::twopi : theta;
}

volm_conf_object::volm_conf_object(vgl_point_3d<double> const& pt, unsigned char const& land)
  : land_(land)
{
  height_ = (float)pt.z();
  dist_ = (float)vcl_sqrt(pt.x()*pt.x()+pt.y()*pt.y());
  float theta = (float)vcl_atan2(pt.y(), pt.x());
  theta_ = (theta < 0) ? theta + (float)vnl_math::twopi : theta;
}

float volm_conf_object::theta_in_deg() const  {  return theta_ / (float)vnl_math::pi_over_180;  }
float volm_conf_object::x() const  {  return dist_ * vcl_cos(theta_);  }
float volm_conf_object::y() const  {  return dist_ * vcl_sin(theta_);  }
vgl_point_2d<float> volm_conf_object::loc() const { return vgl_point_2d<float>(this->x(), this->y()); }

bool volm_conf_object::is_same(volm_conf_object const& other)
{
  return (vcl_fabs(this->theta_ - other.theta() ) < EPSILON) && (vcl_fabs(this->dist_ - other.dist() ) < EPSILON)
          && (vcl_fabs(this->height_ - other.height() ) < EPSILON)
          && this->land_ == other.land();
}
bool volm_conf_object::is_same(volm_conf_object_sptr other_sptr)
{
  return this->is_same(*other_sptr);
}
bool volm_conf_object::is_same(volm_conf_object const* other_ptr)
{
  return this->is_same(*other_ptr);
}

bool volm_conf_object::write_to_kml(double const& lon, double const& lat, vcl_vector<volm_conf_object>& values, vcl_string const& kml_file)
{
  // construct a local lvcs with origin (lon, lat)
  vpgl_lvcs lvcs(lat, lon, 0.0, vpgl_lvcs::wgs84, vpgl_lvcs::DEG, vpgl_lvcs::METERS);

  // write values into kml file
  vcl_ofstream ofs(kml_file.c_str());
  bkml_write::open_document(ofs);
  // write the geo location into kml
  vcl_stringstream name;
  name << "hypo_" << vcl_setprecision(6) << lon << '_' << vcl_setprecision(6) << lat;
  bkml_write::write_location(ofs, vgl_point_2d<double>(lon, lat), name.str(), "");
  unsigned n = values.size();
  for (unsigned i = 0; i < n; i++)
  {
    // calculate the actual location from local coordinate theta/dist
    double lx = values[i].dist() * vcl_cos(values[i].theta());
    double ly = values[i].dist() * vcl_sin(values[i].theta());
    double loc_lon, loc_lat, loc_gz;
    lvcs.local_to_global(lx, ly, 0.0, vpgl_lvcs::wgs84, loc_lon, loc_lat, loc_gz);
    // write the location into kml
    vcl_stringstream loc_name;
    loc_name << i << "_" << vcl_setprecision(4) << loc_lon << '_' << vcl_setprecision(4) << loc_lat << '_' << vcl_setprecision(6) << values[i].height()
             << "_" << volm_osm_category_io::volm_land_table[values[i].land()].name_;
    vnl_double_2 ul, ll, lr, ur;
    double size = 5E-5;
    ll[0] = loc_lat;      ll[1] = loc_lon;
    ul[0] = loc_lat+size; ul[1] = loc_lon;
    lr[0] = loc_lat;      lr[1] = loc_lon+size;
    ur[0] = loc_lat+size; ur[1] = loc_lon+size;
    vil_rgb<vxl_byte> color = volm_osm_category_io::volm_land_table[values[i].land()].color_;
    bkml_write::write_box(ofs, loc_name.str(), "", ul, ur, ll, lr, color.r, color.g, color.b);
  }
  bkml_write::close_document(ofs);
  return true;
}

// binary IO
void volm_conf_object::b_write(vsl_b_ostream& os) const
{
  unsigned char ver = this->version();
  vsl_b_write(os, ver);
  vsl_b_write(os, dist_);
  vsl_b_write(os, height_);
  vsl_b_write(os, theta_);
  vsl_b_write(os, land_);
}

void volm_conf_object::b_read(vsl_b_istream& is)
{
  unsigned char ver;
  vsl_b_read(is, ver);
  if (ver == 1)
  {
    vsl_b_read(is, dist_);
    vsl_b_read(is, theta_);
    vsl_b_read(is, land_);
  }
  else if ( ver == this->version())
  {
    vsl_b_read(is, dist_);
    vsl_b_read(is, height_);
    vsl_b_read(is, theta_);
    vsl_b_read(is, land_);
  }
  else
  {
    vcl_cout << "volm_conf_object: binary read -- unknown binary io version: " << (int)ver << ", most updated version is " << (int)this->version()
             << vcl_flush << vcl_endl;
    return;
  }
}

void vsl_b_write(vsl_b_ostream& os, volm_conf_object const& obj)
{
  obj.b_write(os);
}

void vsl_b_write(vsl_b_ostream& os, volm_conf_object const* obj_ptr)
{
  if (obj_ptr == 0)
    vsl_b_write(os, false);
  else {
    vsl_b_write(os, true);
    vsl_b_write(os, *obj_ptr);
  }
}

void vsl_b_write(vsl_b_ostream& os, volm_conf_object_sptr const& obj_sptr)
{
  vsl_b_write(os, obj_sptr.ptr());
}

void vsl_b_read(vsl_b_istream& is, volm_conf_object& obj)
{
  obj.b_read(is);
}

void vsl_b_read(vsl_b_istream& is, volm_conf_object*& obj_ptr)
{
  delete obj_ptr;  obj_ptr = 0;
  bool not_null_ptr;
  vsl_b_read(is, not_null_ptr);
  if (not_null_ptr)
  {
    obj_ptr = new volm_conf_object();
    obj_ptr->b_read(is);
  }
}

void vsl_b_read(vsl_b_istream& is, volm_conf_object_sptr& obj_sptr)
{
  volm_conf_object* obj_ptr = 0;
  vsl_b_read(is, obj_ptr);
  obj_sptr = obj_ptr;
}

void vsl_print_summary(vcl_ostream& os, volm_conf_object const& obj)
{
  obj.print(os);
}
