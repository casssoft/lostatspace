/*
 * OpenSimplex Noise in Java ported to C.
 * by Kurt Spencer, ported to C by Patrick Riordan
 *
 * Java version: 
 * v1.1 (October 5, 2014)
 * - Added 2D and 4D implementations. (removed all but 2d implementation for C port)
 * - Proper gradient sets for all dimensions, from a
 *   dimensionally-generalizable scheme with an actual
 *   rhyme and reason behind it.
 * - Removed default permutation array in favor of
 *   default seed.
 * - Changed seed-based constructor to be independent
 *   of any particular randomization library, so results
 *   will be the same when ported to other languages.
 */

/*
 * License from original code
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * The same license applies to the ported c and .h file because I'm nice.
 *
 * THIS LICENSE DOES NOT APPLY TO LOST AT SPACE CODE IN GENERAL
 */
#include <stdlib.h> 
#define STRETCH_CONSTANT_2D -0.211324865405187    //(1/Math.sqrt(2+1)-1)/2;
#define SQUISH_CONSTANT_2D 0.366025403784439      //(Math.sqrt(2+1)-1)/2;
#define STRETCH_CONSTANT_3D -1.0 / 6              //(1/Math.sqrt(3+1)-1)/3;
#define SQUISH_CONSTANT_3D 1.0 / 3                //(Math.sqrt(3+1)-1)/3;
#define STRETCH_CONSTANT_4D -0.138196601125011    //(1/Math.sqrt(4+1)-1)/4;
#define SQUISH_CONSTANT_4D 0.309016994374947      //(Math.sqrt(4+1)-1)/4;

#define NORM_CONSTANT_2D  47
#define NORM_CONSTANT_3D  103
#define NORM_CONSTANT_4D  30

#define DEFAULT_SEED  0
#include "OpenSimplexNoise.h"
	
	//Gradients for 2D. They approximate the directions to the
	//vertices of an octagon from the center.
static char gradients2D[] = {
		 5,  2,    2,  5,
		-5,  2,   -2,  5,
		 5, -2,    2, -5,
		-5, -2,   -2, -5,
	};
	
	//Gradients for 3D. They approximate the directions to the
	//vertices of a rhombicuboctahedron from the center, skewed so
	//that the triangular and square facets can be inscribed inside
	//circles of the same radius.
static char gradients3D[] = {
		-11,  4,  4,     -4,  11,  4,    -4,  4,  11,
		 11,  4,  4,      4,  11,  4,     4,  4,  11,
		-11, -4,  4,     -4, -11,  4,    -4, -4,  11,
		 11, -4,  4,      4, -11,  4,     4, -4,  11,
		-11,  4, -4,     -4,  11, -4,    -4,  4, -11,
		 11,  4, -4,      4,  11, -4,     4,  4, -11,
		-11, -4, -4,     -4, -11, -4,    -4, -4, -11,
		 11, -4, -4,      4, -11, -4,     4, -4, -11,
	};
	
	
static double extrapolate_4(OpenSimplexNoise* this, int xsb, int ysb, double dx, double dy)
	{
		int index = this->perm[(this->perm[xsb & 0xFF] + ysb) & 0xFF] & 0x0E;
		return gradients2D[index] * dx
			+ gradients2D[index + 1] * dy;
	}
	
	static double extrapolate_6(OpenSimplexNoise* this, int xsb, int ysb, int zsb, double dx, double dy, double dz)
	{
		int index = this->permGradIndex3D[(this->perm[(this->perm[xsb & 0xFF] + ysb) & 0xFF] + zsb) & 0xFF];
		return gradients3D[index] * dx
			+ gradients3D[index + 1] * dy
			+ gradients3D[index + 2] * dz;
	}
	
	
static int fastFloor(double x) {
		int xi = (int)x;
		return x < xi ? xi - 1 : xi;
	}
//	public OpenSimplexNoise(short* perm) {
//		this.perm = perm;
//		permGradIndex3D = (short*)calloc( 256, sizeof(short));
//		
//		for (int i = 0; i < 256; i++) {
//			//Since 3D has 24 gradients, simple bitmask won't work, so precompute modulo array.
//			permGradIndex3D[i] = (short)((perm[i] % (gradients3D.length / 3)) * 3);
//		}
//	}
	
	//Initializes the class using a permutation array generated from a 64-bit seed.
	//Generates a proper permutation (i.e. doesn't merely perform N successive pair swaps on a base array)
	//Uses a simple 64-bit LCG.
	void OSN_init(OpenSimplexNoise* this, long seed) {
		this->perm = (short*)calloc( 256, sizeof(short));
		this->permGradIndex3D = (short*)calloc( 256, sizeof(short));
		short* source = (short*)calloc( 256, sizeof(short));
    int i;
		for (i = 0; i < 256; i++)
			source[i] = i;
		seed = seed * 6364136223846793005l + 1442695040888963407l;
		seed = seed * 6364136223846793005l + 1442695040888963407l;
		seed = seed * 6364136223846793005l + 1442695040888963407l;
		for (i = 255; i >= 0; i--) {
			seed = seed * 6364136223846793005l + 1442695040888963407l;
			int r = (int)((seed + 31) % (i + 1));
			if (r < 0)
				r += (i + 1);
			this->perm[i] = source[r];
			this->permGradIndex3D[i] = (short)((this->perm[i] % (sizeof(gradients3D) / 3)) * 3);
			source[r] = source[i];
		}
	}

  double OSN_eval(OpenSimplexNoise* this, double x, double y) {
		//Place input coordinates onto grid.
		double stretchOffset = (x + y) * STRETCH_CONSTANT_2D;
		double xs = x + stretchOffset;
		double ys = y + stretchOffset;
		
		//Floor to get grid coordinates of rhombus (stretched square) super-cell origin.
		int xsb = fastFloor(xs);
		int ysb = fastFloor(ys);
		
		//Skew out to get actual coordinates of rhombus origin. We'll need these later.
		double squishOffset = (xsb + ysb) * SQUISH_CONSTANT_2D;
		double xb = xsb + squishOffset;
		double yb = ysb + squishOffset;
		
		//Compute grid coordinates relative to rhombus origin.
		double xins = xs - xsb;
		double yins = ys - ysb;
		
		//Sum those together to get a value that determines which region we're in.
		double inSum = xins + yins;

		//Positions relative to origin point.
		double dx0 = x - xb;
		double dy0 = y - yb;
		
		//We'll be defining these inside the next block and using them afterwards.
		double dx_ext, dy_ext;
		int xsv_ext, ysv_ext;
		
		double value = 0;

		//Contribution (1,0)
		double dx1 = dx0 - 1 - SQUISH_CONSTANT_2D;
		double dy1 = dy0 - 0 - SQUISH_CONSTANT_2D;
		double attn1 = 2 - dx1 * dx1 - dy1 * dy1;
		if (attn1 > 0) {
			attn1 *= attn1;
			value += attn1 * attn1 * extrapolate_4(this, xsb + 1, ysb + 0, dx1, dy1);
		}

		//Contribution (0,1)
		double dx2 = dx0 - 0 - SQUISH_CONSTANT_2D;
		double dy2 = dy0 - 1 - SQUISH_CONSTANT_2D;
		double attn2 = 2 - dx2 * dx2 - dy2 * dy2;
		if (attn2 > 0) {
			attn2 *= attn2;
			value += attn2 * attn2 * extrapolate_4(this, xsb + 0, ysb + 1, dx2, dy2);
		}
		
		if (inSum <= 1) { //We're inside the triangle (2-Simplex) at (0,0)
			double zins = 1 - inSum;
			if (zins > xins || zins > yins) { //(0,0) is one of the closest two triangular vertices
				if (xins > yins) {
					xsv_ext = xsb + 1;
					ysv_ext = ysb - 1;
					dx_ext = dx0 - 1;
					dy_ext = dy0 + 1;
				} else {
					xsv_ext = xsb - 1;
					ysv_ext = ysb + 1;
					dx_ext = dx0 + 1;
					dy_ext = dy0 - 1;
				}
			} else { //(1,0) and (0,1) are the closest two vertices.
				xsv_ext = xsb + 1;
				ysv_ext = ysb + 1;
				dx_ext = dx0 - 1 - 2 * SQUISH_CONSTANT_2D;
				dy_ext = dy0 - 1 - 2 * SQUISH_CONSTANT_2D;
			}
		} else { //We're inside the triangle (2-Simplex) at (1,1)
			double zins = 2 - inSum;
			if (zins < xins || zins < yins) { //(0,0) is one of the closest two triangular vertices
				if (xins > yins) {
					xsv_ext = xsb + 2;
					ysv_ext = ysb + 0;
					dx_ext = dx0 - 2 - 2 * SQUISH_CONSTANT_2D;
					dy_ext = dy0 + 0 - 2 * SQUISH_CONSTANT_2D;
				} else {
					xsv_ext = xsb + 0;
					ysv_ext = ysb + 2;
					dx_ext = dx0 + 0 - 2 * SQUISH_CONSTANT_2D;
					dy_ext = dy0 - 2 - 2 * SQUISH_CONSTANT_2D;
				}
			} else { //(1,0) and (0,1) are the closest two vertices.
				dx_ext = dx0;
				dy_ext = dy0;
				xsv_ext = xsb;
				ysv_ext = ysb;
			}
			xsb += 1;
			ysb += 1;
			dx0 = dx0 - 1 - 2 * SQUISH_CONSTANT_2D;
			dy0 = dy0 - 1 - 2 * SQUISH_CONSTANT_2D;
		}
		
		//Contribution (0,0) or (1,1)
		double attn0 = 2 - dx0 * dx0 - dy0 * dy0;
		if (attn0 > 0) {
			attn0 *= attn0;
			value += attn0 * attn0 * extrapolate_4(this, xsb, ysb, dx0, dy0);
		}
		
		//Extra Vertex
		double attn_ext = 2 - dx_ext * dx_ext - dy_ext * dy_ext;
		if (attn_ext > 0) {
			attn_ext *= attn_ext;
			value += attn_ext * attn_ext * extrapolate_4(this, xsv_ext, ysv_ext, dx_ext, dy_ext);
		}
		
		return value / NORM_CONSTANT_2D;
	}
	
