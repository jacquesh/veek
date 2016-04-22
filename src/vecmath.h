#ifndef _MATH_H
#define _MATH_H

struct Vector2
{
    float x;
    float y;

    Vector2() : x(0.0f), y(0.0f) {}
    Vector2(float xVal, float yVal) : x(xVal), y(yVal) {}

    float dot(Vector2& rhs);
    float sqrMagnitude();
    float magnitude();
    void normalize();
    Vector2 normalized();
};

Vector2 operator +(Vector2 lhs, Vector2 rhs);
Vector2 operator -(Vector2 lhs, Vector2 rhs);
Vector2 operator -(Vector2 rhs);
Vector2 operator *(Vector2 lhs, float rhs);
Vector2 operator *(float lhs, Vector2 rhs);
Vector2 operator *(Vector2 lhs, Vector2 rhs);

Vector2& operator +=(Vector2& lhs, Vector2 rhs);
Vector2& operator -=(Vector2& lhs, Vector2 rhs);
Vector2& operator *=(Vector2& lhs, float rhs);


int sign(int x);

int clamp(int x, int min, int max);
float clampf(float x, float min, float max);
float lerp(float from, float to, float t);
float signf(float x);

#endif
