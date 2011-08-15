// This is brl/bbas/brad/brad_grad_hist_grad_hist_feature_vector.h
#ifndef brad_grad_hist_feature_vector_h
#define brad_grad_hist_feature_vector_h
//:
// \file
// \brief Extract a feature vector from image gradient magnitudes
// \author J.L. Mundy
// \date June 30, 2011
//
// \verbatim
//  Modifications
//   <none yet>
// \endverbatim
// 
//
//  This class acts as a functor to be used in eigenvector and 
//  classifier processes
//
#include <vcl_iostream.h>
#include <vnl/vnl_vector.h>
#include <vil/vil_image_view.h>
#include <vsl/vsl_binary_io.h>
class brad_grad_hist_feature_vector 
{
 public:
  brad_grad_hist_feature_vector(){}

  brad_grad_hist_feature_vector(float min, float max, unsigned nbins)
    : min_(min), max_(max), nbins_(nbins) {};

  ~brad_grad_hist_feature_vector(){};

  //: vector of histogram probabilities computed from the input view, plus entropy
  vnl_vector<double> operator() (vil_image_view<float> const& view) const;

  //: the number of elements in the vector
  unsigned size() const { return nbins_+1;}

  //: the type name
  vcl_string type() const {return "brad_grad_hist_feature_vector";}

  //: accessors, setters
  unsigned nbins() const { return nbins_;}
  float min() const {return min_;}
  float max() const {return max_;}
  void set_nbins(unsigned nbins) {nbins_ = nbins;}
  void set_min(float min) {min_ = min;}
  void set_max(float max) {max_ = max;}
  //: print
  void print(vcl_ostream& os = vcl_cout) const {
    os << "nbins = " << nbins_ << " min = " 
       << min_ << " max = " << max_ << '\n';
  }

 private:
  float min_, max_;
  unsigned nbins_;
};

void vsl_b_write(vsl_b_ostream &os, const brad_grad_hist_feature_vector& fv);

void vsl_b_read(vsl_b_istream &is, brad_grad_hist_feature_vector& fv);

void vsl_print_summary(vcl_ostream &os, brad_grad_hist_feature_vector& fv);


#endif // brad_grad_hist_feature_vector_h
