SET(SOURCES ui_MiscGauge.c
    ui_OilWaterTemperatureGauge.c
    ui_RpmSpeedGauge.c
    ui_EngineManagementGauge.c
    ui_ErrorScreen.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_724913057.c)

add_library(ui ${SOURCES})
