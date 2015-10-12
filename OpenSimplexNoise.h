#ifndef OPEN_SIMPLEX_NOISE_H__
#define OPEN_SIMPLEX_NOISE_H__
typedef struct {
	short* perm;
	short* permGradIndex3D;
} OpenSimplexNoise;
void OSN_init(OpenSimplexNoise* this, long seed);
double OSN_eval(OpenSimplexNoise* this, double x, double y);
#endif
