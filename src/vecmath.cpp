#include "vecmath.h"
#include <math.h>

// ==============================
// Vector2 method implementations
// ==============================
float Vector2::dot(Vector2& rhs)
{
    return x*rhs.x + y*rhs.y;
}

float Vector2::sqrMagnitude()
{
    return x*x + y*y;
}

float Vector2::magnitude()
{
    return sqrtf(sqrMagnitude());
}

void Vector2::normalize()
{
    float mag = magnitude();
    if(mag > 0.0f)
    {
        float inverseMagnitude = 1.0f/mag;
        x *= inverseMagnitude;
        y *= inverseMagnitude;
    }
}

Vector2 Vector2::normalized()
{
    Vector2 result(x, y);
    result.normalize();
    return result;
}

// ==========================
// Vector2 operator overloads
// ==========================
Vector2 operator +(Vector2 lhs, Vector2 rhs)
{
    Vector2 result;
    result.x = lhs.x + rhs.x;
    result.y = lhs.y + rhs.y;
    return result;
}

Vector2 operator -(Vector2 lhs, Vector2 rhs)
{
    Vector2 result;
    result.x = lhs.x - rhs.x;
    result.y = lhs.y - rhs.y;
    return result;
}

Vector2 operator -(Vector2 rhs)
{
    Vector2 result;
    result.x = -rhs.x;
    result.y = -rhs.y;
    return result;
}

Vector2 operator *(Vector2 lhs, float rhs)
{
    Vector2 result;
    result.x = lhs.x * rhs;
    result.y = lhs.y * rhs;
    return result;
}

Vector2 operator *(float lhs, Vector2 rhs)
{
    Vector2 result;
    result.x = lhs * rhs.x;
    result.y = lhs * rhs.y;
    return result;
}

Vector2 operator *(Vector2 lhs, Vector2 rhs)
{
    Vector2 result;
    result.x = lhs.x * rhs.x;
    result.y = lhs.x * rhs.y;
    return result;
}

Vector2& operator +=(Vector2& lhs, Vector2 rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

Vector2& operator -=(Vector2& lhs, Vector2 rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

Vector2& operator *=(Vector2& lhs, float rhs)
{
    lhs.x *= rhs;
    lhs.y *= rhs;
    return lhs;
}

// ========================
// Function implementations
// ========================
int min(int x, int y)
{
    if(x <= y)
        return x;
    return y;
}

int max(int x, int y)
{
    if(x >= y)
        return x;
    return y;
}

int clamp(int x, int min, int max)
{
    if(x < min)
    {
        x = min;
    }
    else if(x > max)
    {
        x = max;
    }
    return x;
}

float clampf(float x, float min, float max)
{
    if(x < min)
    {
        x = min;
    }
    else if(x > max)
    {
        x = max;
    }
    return x;
}

float lerp(float from, float to, float t)
{
    return (1.0f-t)*from + t*to;
}

Vector2 lerp(Vector2 from, Vector2 to, float t)
{
    return (1.0f - t)*from + t*to;
}

int sign(int x)
{
    int result;
    if(x > 0) result = 1;
    else if(x < 0) result = -1;
    else result = 0;
    return result;
}

float signf(float x)
{
    float result;
    if(x > 0.0f) result = 1.0f;
    else if(x < 0.0f) result = -1.0f;
    else result = 0.0f;
    return result;
}
