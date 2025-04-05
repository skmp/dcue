#include <stdio.h>
#include <assert.h>
#include "pvr_texture_encoder.h"
#include "file_common.h"
#include "file_pvr.h"

int fPvrSmallVQCodebookSize(int texsize_pixels, int mip) {
    // 16x16 = 256 bytes Small VQ
	if (texsize_pixels <= 32)
		return 24;
    // 16x32 or 32x16 = 512 bytes Small VQ
	if(texsize_pixels == 48)
	    return 48;
	// 32x32 = 512 bytes Small VQ
    if(texsize_pixels == 64)
        return 32;
	// 64x32 or 32x64 = 1024 bytes Small VQ
    if(texsize_pixels == 96)
        return 64;
	// 64xx64 = 1536 bytes Small VQ
    if(texsize_pixels == 128)
        return 64;
    // 128x64 or 64x128 = 3584 bytes Small VQ
	if(texsize_pixels == 192)
	    return 192;

	return 256;
}

void fPvrWrite(const PvrTexEncoder *pte, const char *outfname) {
	assert(pte);
	assert(pte->pvr_tex);
	assert(outfname);
	
	FILE *f = fopen(outfname, "wb");
	assert(f);
	
	//Write header
	unsigned chunksize = 16;
	
	unsigned pvrfmt = FILE_PVR_SQUARE;
	if (pteIsCompressed(pte)) {
		pvrfmt = FILE_PVR_VQ;
		unsigned cb_size = 2048;
		unsigned int idxcnt = pte->w * pte->h / 4;
		if (pteHasMips(pte))
			idxcnt = idxcnt * 4/3 + 1;
		
		if (pte->auto_small_vq) {
			//We only generate real small VQ textures when small_vq is set
			pvrfmt = FILE_PVR_SMALL_VQ;
			cb_size = pte->codebook_size * 8;
		}
		
		if (pteIsPalettized(pte))
			ErrorExit(".PVR format does not support compressed palettized textures\n");
		// JP - Rectangle VQ certainly does work on real hardware
		//if (pte->w != pte->h)
		//	ErrorExit(".PVR format does not support non-square compressed textures\n");
		
		chunksize += idxcnt+cb_size;
	} else {
		chunksize += CalcTextureSize(pte->w, pte->h, pte->pixel_format, pteHasMips(pte), 0, 0);
		
		if (pte->pixel_format == PTE_PALETTE_8B) {
			pvrfmt = FILE_PVR_8BPP;
		} else if (pte->pixel_format == PTE_PALETTE_4B) {
			pvrfmt = FILE_PVR_4BPP;
		}
		
		//.PVR does not store first 4 padding bytes of uncompressed mipmapped texture
		if (pteHasMips(pte))
			chunksize -= 4;
		
		if (pte->w != pte->h) {
			pvrfmt = FILE_PVR_RECT;
			assert(!pteHasMips(pte));
		}
	}
	
	if (pteHasMips(pte))
		pvrfmt += FILE_PVR_MIP_ADD;

	if(pte->codebook_size != 256) {
	    // Write codebook size to GBIX (hack)
	    WriteFourCC("GBIX", f);
	    Write32LE(0, f);
	    Write32LE(pte->codebook_size, f);
	}

	WriteFourCC("PVRT", f);
	Write32LE(chunksize, f);	//chunk size
	Write32LE(pvrfmt | pte->pixel_format, f);	//pixel format, type
	Write16LE(pte->w, f);
	Write16LE(pte->h, f);
	
	WritePvrTexEncoder(pte, f, pte->auto_small_vq ? PTEW_FILE_PVR_SMALL_VQ : PTEW_NO_SMALL_VQ, 4);
	
	fclose(f);
	assert(chunksize + (pte->codebook_size != 256 ? 12 : 0) == FileSize(outfname));
}
