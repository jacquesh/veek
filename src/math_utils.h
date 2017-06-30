#ifndef _MATH_UTILS_H
#define _MATH_UTILS_H

inline int min(int x, int y)
{
    if(x <= y)
        return x;
    return y;
}

inline float minf(float x, float y)
{
    if(x <= y)
        return x;
    return y;
}

inline int max(int x, int y)
{
    if(x >= y)
        return x;
    return y;
}

inline float maxf(float x, float y)
{
    if(x >= y)
        return x;
    return y;
}

inline int clamp(int x, int min, int max)
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

inline float clampf(float x, float min, float max)
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

inline float lerp(float from, float to, float t)
{
    return (1.0f-t)*from + t*to;
}

inline int sign(int x)
{
    int result;
    if(x > 0) result = 1;
    else if(x < 0) result = -1;
    else result = 0;
    return result;
}

inline float signf(float x)
{
    float result;
    if(x > 0.0f) result = 1.0f;
    else if(x < 0.0f) result = -1.0f;
    else result = 0.0f;
    return result;
}

#endif // _MATH_UTILS_H
