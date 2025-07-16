#ifndef _FFTransform
#define _FFTransform

#include "SampleIter.h"
#include <math.h>

#define PI      3.1415926535897931
#define HALFPI 1.5707963267948966f
#define sinCosRng 2048

#define BITREV_F(j, nu, fp)                           \
{ int j2, j1 = j, k = 0;                              \
  for (int i = 1; i <= nu; i++)                       \
  { j2 = j1/2; k  = ((2*k) + j1) - (2*j2); j1 = j2; } \
  *fp = (float) k;                                    \
} 

#define BITREV_I(j, nu, ip)                           \
{ int j2, j1 = j, k = 0;                              \
  for (int i = 1; i <= nu; i++)                       \
  { j2 = j1/2; k  = ((2*k) + j1) - (2*j2); j1 = j2; } \
  *ip =  k;                                           \
} 

float FastSqrt(float x) { 
	float xhalf = 0.5f*x;
	int i = *(int*)&x;

	i = 0x5f375a84 - (i>>1);
	x = *(float*)&i;
	x = x*(1.5f-xhalf*x*x);

	return 1.0f/x;
}

class FFTransform {
private:
    int    sampleRate, sampleCount; // assume both var are a power of 2
    float* pSpecMag;
    float* pXReal;
    float* pXImaginary;
    float* pSamples;

    float     sinTab[sinCosRng], cosTab[sinCosRng]; 

    int bitrev (int j, int nu) {        
        int j2;
        int j1 = j;
        int k = 0;
        for (int i = 1; i <= nu; i++) {
            j2 = j1/2;
            k  = ((2*k) + j1) - (2*j2);
            j1 = j2;
        }

        return k;
    }

    void initTables (void) {
        double step = HALFPI / (double)sinCosRng;
        double rad  = 0.0;
        for (int idx = 0; idx < sinCosRng; idx++) {
            sinTab[idx] = (float)sin (rad);
            cosTab[idx] = (float)cos (rad);
            rad         += step;
        }
    }

public:

    FFTransform(int sampleRate, int sampleCount) {
        initTables();

        this->sampleRate  = sampleRate;
        this->sampleCount = sampleCount;
        pSpecMag    = new float [sampleCount];
        pXReal      = new float [sampleCount];
        pXImaginary = new float [sampleCount];
    }

    ~FFTransform() {
        if (pSpecMag != NULL) delete[] pSpecMag;
        if (pXReal != NULL) delete[] pXReal;
        if (pXImaginary != NULL) delete[] pXImaginary;

        pXReal = pXImaginary = pSpecMag = NULL;
    }


    int GetFrequency(int point) const {                
        return sampleRate / sampleCount ;
    }

    int HzToPoint(int freq) const { 
        return (sampleCount * freq) / sampleRate; 
    }

    float* fftGetMagArray() {
        return pSpecMag;
    }

    float fftGetMagFromHertz(int freq) {
        return pSpecMag[HzToPoint(freq)];
    }

    bool fftTransformMag(SampleIter& samples, bool powerSpectrum) {
        if ((pSpecMag == NULL) || (pXImaginary == NULL) || (pXReal == NULL)) return false;
        if (samples.Count() > sampleCount) return false;

        int nu            = (int)(log(sampleCount)/log(2));
        int halfSampCount = sampleCount/2;
        int nu1           = nu - 1;

        float tr, ti, p, arg, c, s;        

        // Load the sample data
        int idx;
        for (idx = 0; idx < sampleCount; idx++) {            
            pXReal[idx]  = samples.GetSample();            

            if (powerSpectrum != false) pSpecMag[idx] = pXReal[idx];
        
            pXImaginary[idx] = 0.0f;
            samples.Advance();
        }
        int k = 0;
        int scidx;

        for (int l = 1; l <= nu; l++) {
            while (k < sampleCount) {
                for (int i = 1; (i <= halfSampCount) && ((k+halfSampCount) < sampleCount); i++) {                    
                    // p   = (float)bitrev (k >> nu1, nu);
                    BITREV_F(k >> nu1, nu, &p)
                    arg = 2 * ((float)PI) * p / sampleCount;
                  
                    // c   = (float) cos (arg);
                    // s   = (float) sin (arg);                    

                    while (arg >= HALFPI) arg = arg - HALFPI;
                    scidx = (int)((arg / HALFPI) * ((float) sinCosRng));
                    c     = cosTab [scidx];
                    s     = sinTab [scidx];

                    tr  = pXReal[k+halfSampCount]*c + pXImaginary[k+halfSampCount]*s;
                    ti  = pXReal[k+halfSampCount]*c - pXReal[k+halfSampCount]*s;

                    pXReal[k+halfSampCount]      = pXReal[k] - tr;
                    pXImaginary[k+halfSampCount] = pXImaginary[k] - ti;
                    pXReal[k]         += tr;
                    pXImaginary[k]    += ti;

                    k++;
                }
                k += halfSampCount;
            }

            k = 0;
            nu1--;

            halfSampCount = halfSampCount/2;
        }

        k = 0;
        int r;        

        while (k < sampleCount) {
            // r = bitrev (k, nu);
            BITREV_I(k, nu, &r)

            if (r > k) {
                tr             = pXReal[k];
                ti             = pXImaginary[k];
                pXReal[k]      = pXReal[r];
                pXImaginary[k] = pXImaginary[r];
                pXReal[r]      = tr;
                pXImaginary[r] = ti;
            }

            k++;
        }

        halfSampCount = sampleCount/2;

        if (powerSpectrum != false) {
	        for (idx = 0; idx < halfSampCount; idx++)
                pSpecMag[idx] = 2.0f * ((float) pXReal[idx] / (float)sampleCount);	            
        }
        else {
            pSpecMag[0]   = (float) (FastSqrt(pXReal[0]*pXReal[0] + pXImaginary[0]*pXImaginary[0])) / (float)sampleCount;
            for (idx = 1; idx < halfSampCount; idx++)
                  pSpecMag[idx] = 2.0f * (float) (FastSqrt((pXReal[idx]*pXReal[idx]) + (pXImaginary[idx]*pXImaginary[idx]))) / (float)sampleCount;
        }

        return true;
    }
};

#endif