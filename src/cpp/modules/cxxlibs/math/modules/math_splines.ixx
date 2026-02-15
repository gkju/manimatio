export module math:splines;

export namespace math {
    inline float cubic_spline(float t, float p0, float p1, float p2, float p3) {
        float t2 = t*t;
        float t3 = t2*t;
        return 0.5f * ((2.0f*p1) +
                       (-p0 + p2) * t +
                       (2.0f*p0 - 5.0f*p1 + 4.0f*p2 - p3) * t2 +
                       (-p0 + 3.0f*p1 - 3.0f*p2 + p3) * t3);
    }
}
