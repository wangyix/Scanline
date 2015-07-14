// STMatrix4.inl
#ifndef __STMATRIX4_INL__
#define __STMATRIX4_INL__

/**
* Inline file for STMatrix4.h
*/

#include "STVector3.h"

inline STMatrix4::STMatrix4()
{
    EncodeI();
}

inline void STMatrix4::EncodeI()
{
    for(int i=0;i<4;i++)for(int j=0;j<4;j++)
        table[i][j]=(i==j?1.f:0.f);
}

inline void STMatrix4::EncodeT(float tx,float ty,float tz)
{
    EncodeI();
    table[0][3]=tx;
    table[1][3]=ty;
    table[2][3]=tz;
}

inline void STMatrix4::EncodeS(float sx,float sy,float sz)
{
    EncodeI();
    table[0][0]=sx;
    table[1][1]=sy;
    table[2][2]=sz;
}

inline void STMatrix4::EncodeR(float degrees,const STVector3& axis)
{
    EncodeI();
    //http://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
    float cos_t=cos(degrees * 3.1415926536f / 180.f);
    float sin_t=sin(degrees * 3.1415926535f / 180.f);

    STVector3 unit_vector=axis;
    unit_vector.Normalize();

    const float& x=unit_vector.x;
    const float& y=unit_vector.y;
    const float& z=unit_vector.z;

    table[0][0] = cos_t + x*x*(1.f-cos_t);
    table[0][1] = x*y*(1-cos_t) - z*sin_t;
    table[0][2] = x*z*(1-cos_t) + y*sin_t;

    table[1][0] = y*x*(1-cos_t) + z*sin_t;
    table[1][1] = cos_t + y*y*(1-cos_t);
    table[1][2] = y*z*(1-cos_t) - x*sin_t;

    table[2][0] = z*x*(1-cos_t) - y*sin_t;
    table[2][1] = z*y*(1-cos_t) + x*sin_t;
    table[2][2] = cos_t + z*z*(1-cos_t);
}

inline STVector3 STMatrix4::operator*(const STVector3& v)
{
    STVector3 result;
    result.x = table[0][0]*v.x + table[0][1]*v.y + table[0][2]*v.z;
    result.y = table[1][0]*v.x + table[1][1]*v.y + table[1][2]*v.z;
    result.z = table[2][0]*v.x + table[2][1]*v.y + table[2][2]*v.z;
    return result;
}

#endif  // __STMATRIX4_INL__
