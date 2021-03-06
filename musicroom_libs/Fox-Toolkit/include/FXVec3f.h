/********************************************************************************
*                                                                               *
*       S i n g l e - P r e c i s i o n   3 - E l e m e n t   V e c t o r       *
*                                                                               *
*********************************************************************************
* Copyright (C) 1994,2010 by Jeroen van der Zijp.   All Rights Reserved.        *
*********************************************************************************
* This library is free software; you can redistribute it and/or modify          *
* it under the terms of the GNU Lesser General Public License as published by   *
* the Free Software Foundation; either version 3 of the License, or             *
* (at your option) any later version.                                           *
*                                                                               *
* This library is distributed in the hope that it will be useful,               *
* but WITHOUT ANY WARRANTY; without even the implied warranty of                *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 *
* GNU Lesser General Public License for more details.                           *
*                                                                               *
* You should have received a copy of the GNU Lesser General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>          *
********************************************************************************/
#ifndef FXVEC3F_H
#define FXVEC3F_H


namespace FX {


/// Single-precision 3-element vector
class FXAPI FXVec3f {
public:
  FXfloat x;
  FXfloat y;
  FXfloat z;
public:

  /// Default constructor; value is not initialized
  FXVec3f(){}

  /// Initialize from 2-vector
  FXVec3f(const FXVec2f& v,FXfloat s=0.0f):x(v.x),y(v.y),z(s){}

  /// Initialize from another vector
  FXVec3f(const FXVec3f& v):x(v.x),y(v.y),z(v.z){}

  /// Initialize from array of floats
  FXVec3f(const FXfloat v[]):x(v[0]),y(v[1]),z(v[2]){}

  /// Initialize from components
  FXVec3f(FXfloat xx,FXfloat yy,FXfloat zz):x(xx),y(yy),z(zz){}

  /// Return a non-const reference to the ith element
  FXfloat& operator[](FXint i){return (&x)[i];}

  /// Return a const reference to the ith element
  const FXfloat& operator[](FXint i) const {return (&x)[i];}

  /// Assignment
  FXVec3f& operator=(const FXVec3f& v){x=v.x;y=v.y;z=v.z;return *this;}

  /// Assignment from array of floats
  FXVec3f& operator=(const FXfloat v[]){x=v[0];y=v[1];z=v[2];return *this;}

  /// Set value from another vector
  FXVec3f& set(const FXVec3f& v){x=v.x;y=v.y;z=v.z;return *this;}

  /// Set value from array of floats
  FXVec3f& set(const FXfloat v[]){x=v[0];y=v[1];z=v[2];return *this;}

  /// Set value from components
  FXVec3f& set(FXfloat xx,FXfloat yy,FXfloat zz){x=xx;y=yy;z=zz;return *this;}

  /// Assigning operators
  FXVec3f& operator*=(FXfloat n){ return set(x*n,y*n,z*n); }
  FXVec3f& operator/=(FXfloat n){ return set(x/n,y/n,z/n); }
  FXVec3f& operator+=(const FXVec3f& v){ return set(x+v.x,y+v.y,z+v.z); }
  FXVec3f& operator-=(const FXVec3f& v){ return set(x-v.x,y-v.y,z-v.z); }
  FXVec3f& operator^=(const FXVec3f& v){ return set(y*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x); }

  /// Conversions
  operator FXfloat*(){return &x;}
  operator const FXfloat*() const {return &x;}
  operator FXVec2f&(){return *reinterpret_cast<FXVec2f*>(this);}
  operator const FXVec2f&() const {return *reinterpret_cast<const FXVec2f*>(this);}

  /// Test if zero
  FXbool operator!() const { return x==0.0f && y==0.0f && z==0.0f; }

  /// Unary
  FXVec3f operator+() const { return *this; }
  FXVec3f operator-() const { return FXVec3f(-x,-y,-z); }

  /// Length and square of length
  FXfloat length2() const { return x*x+y*y+z*z; }
  FXfloat length() const { return sqrtf(length2()); }

  /// Clamp values of vector between limits
  FXVec3f& clamp(FXfloat lo,FXfloat hi){ return set(FXCLAMP(lo,x,hi),FXCLAMP(lo,y,hi),FXCLAMP(lo,z,hi)); }
  };


/// Dot product
inline FXfloat operator*(const FXVec3f& a,const FXVec3f& b){ return a.x*b.x+a.y*b.y+a.z*b.z; }

/// Cross product
inline FXVec3f operator^(const FXVec3f& a,const FXVec3f& b){ return FXVec3f(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }

/// Scaling
inline FXVec3f operator*(const FXVec3f& a,FXfloat n){return FXVec3f(a.x*n,a.y*n,a.z*n);}
inline FXVec3f operator*(FXfloat n,const FXVec3f& a){return FXVec3f(n*a.x,n*a.y,n*a.z);}
inline FXVec3f operator/(const FXVec3f& a,FXfloat n){return FXVec3f(a.x/n,a.y/n,a.z/n);}
inline FXVec3f operator/(FXfloat n,const FXVec3f& a){return FXVec3f(n/a.x,n/a.y,n/a.z);}

/// Vector and vector addition
inline FXVec3f operator+(const FXVec3f& a,const FXVec3f& b){ return FXVec3f(a.x+b.x,a.y+b.y,a.z+b.z); }
inline FXVec3f operator-(const FXVec3f& a,const FXVec3f& b){ return FXVec3f(a.x-b.x,a.y-b.y,a.z-b.z); }

/// Equality tests
inline FXbool operator==(const FXVec3f& a,FXfloat n){return a.x==n && a.y==n && a.z==n;}
inline FXbool operator!=(const FXVec3f& a,FXfloat n){return a.x!=n || a.y!=n || a.z!=n;}
inline FXbool operator==(FXfloat n,const FXVec3f& a){return n==a.x && n==a.y && n==a.z;}
inline FXbool operator!=(FXfloat n,const FXVec3f& a){return n!=a.x || n!=a.y || n!=a.z;}

/// Equality tests
inline FXbool operator==(const FXVec3f& a,const FXVec3f& b){ return a.x==b.x && a.y==b.y && a.z==b.z; }
inline FXbool operator!=(const FXVec3f& a,const FXVec3f& b){ return a.x!=b.x || a.y!=b.y || a.z!=b.z; }

/// Inequality tests
inline FXbool operator<(const FXVec3f& a,FXfloat n){return a.x<n && a.y<n && a.z<n;}
inline FXbool operator<=(const FXVec3f& a,FXfloat n){return a.x<=n && a.y<=n && a.z<=n;}
inline FXbool operator>(const FXVec3f& a,FXfloat n){return a.x>n && a.y>n && a.z>n;}
inline FXbool operator>=(const FXVec3f& a,FXfloat n){return a.x>=n && a.y>=n && a.z>=n;}

/// Inequality tests
inline FXbool operator<(FXfloat n,const FXVec3f& a){return n<a.x && n<a.y && n<a.z;}
inline FXbool operator<=(FXfloat n,const FXVec3f& a){return n<=a.x && n<=a.y && n<=a.z;}
inline FXbool operator>(FXfloat n,const FXVec3f& a){return n>a.x && n>a.y && n>a.z;}
inline FXbool operator>=(FXfloat n,const FXVec3f& a){return n>=a.x && n>=a.y && n>=a.z;}

/// Inequality tests
inline FXbool operator<(const FXVec3f& a,const FXVec3f& b){ return a.x<b.x && a.y<b.y && a.z<b.z; }
inline FXbool operator<=(const FXVec3f& a,const FXVec3f& b){ return a.x<=b.x && a.y<=b.y && a.z<=b.z; }
inline FXbool operator>(const FXVec3f& a,const FXVec3f& b){ return a.x>b.x && a.y>b.y && a.z>b.z; }
inline FXbool operator>=(const FXVec3f& a,const FXVec3f& b){ return a.x>=b.x && a.y>=b.y && a.z>=b.z; }

/// Lowest or highest components
inline FXVec3f lo(const FXVec3f& a,const FXVec3f& b){return FXVec3f(FXMIN(a.x,b.x),FXMIN(a.y,b.y),FXMIN(a.z,b.z));}
inline FXVec3f hi(const FXVec3f& a,const FXVec3f& b){return FXVec3f(FXMAX(a.x,b.x),FXMAX(a.y,b.y),FXMAX(a.z,b.z));}

/// Convert vector to color
extern FXAPI FXColor colorFromVec3f(const FXVec3f& vec);

/// Convert color to vector
extern FXAPI FXVec3f colorToVec3f(FXColor clr);

/// Compute normal from three points a,b,c
extern FXAPI FXVec3f normal(const FXVec3f& a,const FXVec3f& b,const FXVec3f& c);

/// Compute approximate normal from four points a,b,c,d
extern FXAPI FXVec3f normal(const FXVec3f& a,const FXVec3f& b,const FXVec3f& c,const FXVec3f& d);

/// Normalize vector
extern FXAPI FXVec3f normalize(const FXVec3f& v);
extern FXAPI FXVec3f fastnormalize(const FXVec3f& v);

/// Save vector to a stream
extern FXAPI FXStream& operator<<(FXStream& store,const FXVec3f& v);

/// Load vector from a stream
extern FXAPI FXStream& operator>>(FXStream& store,FXVec3f& v);

}

#endif
