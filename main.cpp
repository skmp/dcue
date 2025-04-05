#include <dc/pvr.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>

#if !defined(DC_SIM)
#include <kos.h>
KOS_INIT_FLAGS(INIT_IRQ | INIT_CONTROLLER | INIT_CDROM | INIT_VMU);
#endif

static pvr_init_params_t pvr_params = {
	.opb_sizes = {
				PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_8, PVR_BINSIZE_0,
				PVR_BINSIZE_8
	},
	.autosort_disabled = true
};

int main() {

    if (pvr_params.fsaa_enabled) {
		pvr_params.vertex_buf_size = (1024 + 768) * 1024;
		pvr_params.opb_overflow_count = 4; // 307200 bytes
	} else {
		pvr_params.vertex_buf_size = (1024 + 1024) * 1024;
		pvr_params.opb_overflow_count = 7; // 268800 bytes
	}

	// if (videoModes[VIDEO_MODE].depth == 24) {
	// 	pvr_params.vertex_buf_size -= 128 * 1024;
	// 	pvr_params.opb_overflow_count -= pvr_params.fsaa_enabled ? 1 : 2;
	// }

    #if !defined(DC_SIM)
	// vid_set_mode(DM_640x480, videoModes[VIDEO_MODE].depth == 24 ? PM_RGB888P : PM_RGB565);
	#endif
    pvr_init(&pvr_params);

    for(;;) {
        // get input
        auto contMaple = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (contMaple) {
            auto state = (cont_state_t *)maple_dev_status(contMaple);

            if (state) {
                if (state->start) {
                    break;
                }
            }
        }
        // render frame
        pvr_set_zclip(0.0f);
		pvr_wait_ready();

        pvr_scene_begin();
        pvr_scene_finish();
    }

    pvr_shutdown();
}