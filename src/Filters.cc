// ==========================================================
// Upsampling / downsampling classes
//
// Design and implementation by
// - Hervé Drolon (drolon@infonie.fr)
// - Detlev Vendt (detlev.vendt@brillit.de)
// - Carsten Klein (cklein05@users.sourceforge.net)
//
// This file is part of FreeImage 3
//
// COVERED CODE IS PROVIDED UNDER THIS LICENSE ON AN "AS IS" BASIS, WITHOUT WARRANTY
// OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING, WITHOUT LIMITATION, WARRANTIES
// THAT THE COVERED CODE IS FREE OF DEFECTS, MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE
// OR NON-INFRINGING. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE COVERED
// CODE IS WITH YOU. SHOULD ANY COVERED CODE PROVE DEFECTIVE IN ANY RESPECT, YOU (NOT
// THE INITIAL DEVELOPER OR ANY OTHER CONTRIBUTOR) ASSUME THE COST OF ANY NECESSARY
// SERVICING, REPAIR OR CORRECTION. THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL
// PART OF THIS LICENSE. NO USE OF ANY COVERED CODE IS AUTHORIZED HEREUNDER EXCEPT UNDER
// THIS DISCLAIMER.
//
// Use at your own risk!
// ==========================================================

// Note: a good portion of this file was removed for use by the IIP Server project
// and other portions were modified and / or fixed - @beaudet

#include "Filters.h"

CWeightsTable::CWeightsTable(CGenericFilter *pFilter, unsigned uDstSize, unsigned uSrcSize) {
    double dWidth; // should probably be called dRadius instead of width
    double dFScale;
    const double dFilterWidth = pFilter->GetWidth();

    // scale factor
    const double dScale = double(uDstSize) / double(uSrcSize);

    if(dScale < 1.0) {
        // minification
        dWidth = dFilterWidth / dScale; 
        dFScale = dScale; 
    } else {
        // magnification
	dWidth = dFilterWidth; 
	dFScale = 1.0; 
    }

    // allocate a new line contributions structure
    // window size is the number of sampled pixels -- only used for memory allocation
    m_WindowSize = 2 * (int)ceil(dWidth) + 1; 

    // length of dst line (no. of rows / cols) 
    m_LineLength = uDstSize; 

    // allocate list of contributions 
    m_WeightTable = (Contribution*)malloc(m_LineLength * sizeof(Contribution));
    for(unsigned u = 0; u < m_LineLength; u++) {
        // allocate contributions for every pixel
        m_WeightTable[u].Weights = (double*)malloc(m_WindowSize * sizeof(double));
        m_WeightTable[u].intWeights = (int*)malloc(m_WindowSize * sizeof(int));
    }

    /*****************************************************************************************
     offset for discrete to continuous coordinate conversion - this isn't correct - it needs 
     to slide with the position X - it's not FIXED!!!!
     // const double dOffset = (0.5 / dScale);

     iterate the line in the destination - e.g., for horizontal scaling it would be a scan 
     line in the destination and for each pixel location in the destination, compute the left 
     and right indexes to use in the sample as well as the weights to assign to each of them 
     based on the weighting function of the filter, e.g. Filter() which accepts a distance 
     between the center of the source pixel and the center in the sample source that is 
     equivalent to the exact center in the destination
    ******************************************************************************************/
    for (unsigned u = 0; u < m_LineLength; u++) {
        /*****************************************************************************************
         scan through line of contributions
         dCenter is the exact center in the source sample corresponding to the exact center in the 
         destination (which is why it's a double) we need to add a half pixel to the destination 
         location before dividing by the scale in order to get the true center in the sample
        *****************************************************************************************/
        const double dCenter = ( (double) u ) + 0.5;
	const double sCenter = dCenter / dScale; 

        // ensure the left index into the source sample is never < 0 even if the filter extends farther to the right
        const int iLeft  = MAX( 0, (int) ( sCenter - dWidth ));              
            
        /*****************************************************************************************
         ensure the right index into the source sample never exceeds the source width -- for 
         extremely small samples, iLeft-iRight could be < m_WindowSize 
         DPB the index of the right side should never exceed the max index of the source image which is width - 1
        *****************************************************************************************/
	const int iRight = MIN( (int) (iLeft + dWidth*2), int(uSrcSize)-1);  

	m_WeightTable[u].Left  = iLeft;     // the start index of the sample for this section of the source image
	m_WeightTable[u].Right = iRight;    // the end index of the sample for this section of the source image

	double dTotalWeight = 0;            // sum of weights (initialized to zero but will sum exactly to 1 to 
                                            // accurately distribute the sample pixels to the destination pixel)
	for (int iSrc = iLeft; iSrc <= iRight; iSrc++) { // @beaudet corrected to <= iRight
           /*****************************************************************************************
             calculate weights; DPB - dFScale adjusts the distance passed to Filter() from source pixels to destination pixels 
             but why is it multiplied twice instead of once?  should only be once I think - we can experiment to find out - DPB
             a half pixel is added here in order to calculate the center of the reference source pixel exactly
	     //const double weight = dFScale * pFilter->Filter(dFScale * ( (double) iSrc + 0.5 - sCenter));  
            *****************************************************************************************/
	    const double weight = pFilter->Filter(dFScale * ( (double) iSrc + 0.5 - sCenter));  
	    // assert((iSrc-iLeft) < m_WindowSize);

            // assign the weight to the proper array index
	    m_WeightTable[u].Weights[iSrc-iLeft] = weight;
	    m_WeightTable[u].intWeights[iSrc-iLeft] = weight * INTSCALER;
	    dTotalWeight += weight;
	}

        // normalize the weights = they MUST total exactly one in order to property distribute the pixel values
	if ( (dTotalWeight > 0) && (dTotalWeight != 1) ) {
	    // normalize weight of neighbouring points
	    for (int iSrc = iLeft; iSrc <= iRight; iSrc++) {
	        // normalize point
	        m_WeightTable[u].Weights[iSrc-iLeft] /= dTotalWeight; 
	        m_WeightTable[u].intWeights[iSrc-iLeft] =  m_WeightTable[u].Weights[iSrc-iLeft] * INTSCALER;
            }
	}

        /*****************************************************************************************
         removed since the IIP resize function discards zero weights instead
        *****************************************************************************************/
        /*
	int iTrailing = iRight - iLeft - 1;
	while ( m_WeightTable[u].Weights[iTrailing] == 0) {
	    m_WeightTable[u].Right--;
	    iTrailing--;
	    if ( m_WeightTable[u].Right == m_WeightTable[u].Left ) {
	        break;
	    }
        }
        */
    } // next dst pixel
}

CWeightsTable::~CWeightsTable() {
    for(unsigned u = 0; u < m_LineLength; u++) {
        // free contributions for every pixel
        free(m_WeightTable[u].Weights);
        free(m_WeightTable[u].intWeights);
    }
    // free list of pixels contributions
    free(m_WeightTable);
}

/********************************************************************************
 remainder of file removed since IIP uses an optimized resizing engine - @beaudet
*********************************************************************************/
