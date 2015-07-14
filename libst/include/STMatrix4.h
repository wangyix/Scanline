// STMatrix4.h
#ifndef __STMATRIX4_H__
#define __STMATRIX4_H__

#include "stForward.h"

#include <math.h>

/**
* STMatrix4 represents a 4x4 matrix
*/
struct STMatrix4
{
    inline STMatrix4();
    inline void EncodeI();
    inline void EncodeT(float tx,float ty,float tz);
    inline void EncodeS(float sx,float sy,float sz);
    inline void EncodeR(float degrees,const STVector3& axis);

    inline STVector3 operator*(const STVector3& v);
    //
    // Local members
    //
    float table[4][4];

    /*CS148 -- implement other member functions here as necessary*/

};

#include "STMatrix4.inl"

#endif  // __STMATRIX4_H__
