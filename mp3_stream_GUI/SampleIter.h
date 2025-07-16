#ifndef _SAMPLEITER
#define _SAMPLEITER

#include <stdlib.h>

enum WorkChannel  {
	WRKCHANNEL_LEFT = 0, 
	WRKCHANNEL_RIGHT = 1, 
	WRKCHANNEL_MONO = 2, 
	WRKCHANNEL_STEREO = 3
};

class SampleIter {
protected:
    WorkChannel c_curChannel;
    char*       c_pSamples;
    int         c_sampleLen;
    int         c_byteWidth;
    bool        c_stereoSamples;
    int         c_interRWIdx;

public:
    SampleIter () { 
        c_curChannel    = WRKCHANNEL_STEREO;
        c_pSamples      = NULL;
        c_sampleLen     = 0;
        c_byteWidth     = 0;
        c_interRWIdx    = 0;
        c_stereoSamples = false;
    };
	
	void SetWorkChannel (WorkChannel workChhannel) {
        c_curChannel = workChhannel;
    }

    bool SetSampleWorkSet (char* pSamples, int sampleLen, int byteWidth, bool stereo) {
        if (pSamples == NULL) return false;

        c_pSamples = pSamples;

        if (byteWidth <= 0) return false;

        c_sampleLen = sampleLen;
        c_byteWidth = byteWidth;

        if ((c_stereoSamples = stereo) == false) c_curChannel = WRKCHANNEL_MONO;
        else c_curChannel = WRKCHANNEL_STEREO;        
        
        SetPos (0);
        return true;
    }

    int Count () {
        if (c_pSamples == NULL) return 0;
        if (c_sampleLen <= 0) return 0;

        int c = c_sampleLen / c_byteWidth;
        if (c_stereoSamples != false) c /= 2;

        return c;
    }

    void SetPos (int idx) {
        c_interRWIdx = (idx * c_byteWidth);

        if (c_stereoSamples != false) c_interRWIdx *= 2;
    }

    int GetPos () {
        int pos = c_interRWIdx / c_byteWidth;

        if (c_stereoSamples != false) pos /= 2;
        return pos;
    }

    void Advance () {   
        c_interRWIdx += c_byteWidth;

        if (c_stereoSamples != false) c_interRWIdx += c_byteWidth;        
    }

    void BartlettWindow (void) {
        int fullRange = Count();
        short* pData  = (short*) c_pSamples;
        
        int    idx;
        float  pwr  = (float)(1.0 / (float)(fullRange / 2));

        switch (c_curChannel) {
            case WRKCHANNEL_LEFT: idx = 0;
                break;

            case WRKCHANNEL_RIGHT: idx = 2;
                break;
        }

        for (int c = 0; c < fullRange; c++) {
            if (c < (fullRange/2)) {
                pData[idx] = (int)((float)pData[idx] * (pwr * (float)idx));
            }
            else {
                pData[idx] = (int)((float)pData[idx] * (1.0 - (pwr * (float)((fullRange / 2) - idx))));
            }

            if (c_stereoSamples != false) idx += 4;
            else idx+= 2;
        }
    }

    short GetSample () {
        if (c_pSamples == NULL) return -1;

        switch (c_byteWidth) {
            case 0: return -1;
            case 1:
				// left channel
                if (c_curChannel != WRKCHANNEL_RIGHT) return c_pSamples[c_interRWIdx];
                return c_pSamples[c_interRWIdx+1];

            case 2:
				// left channel
                if (c_curChannel != WRKCHANNEL_RIGHT) return *((short*)(c_pSamples+c_interRWIdx)); 
                return *((short*)(c_pSamples+c_interRWIdx+2));                
        }
        return -1;
    }
};

#endif