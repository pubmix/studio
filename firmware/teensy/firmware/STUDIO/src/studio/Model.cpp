#include "Model.h"
namespace studio {float Mapping::at(float x)const {x=unit(x); switch(curve){case Curve::Square:x*=x;break;case Curve::Root:x=std::sqrt(x);break;case Curve::Smooth:x=x*x*(3-2*x);break;default:break;}return low+(high-low)*x;}}
