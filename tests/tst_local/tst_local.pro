# Legacy test — written against the old BusServer/Catalog/Router API.
# Kept for reference only; excluded from the build.
TEMPLATE = aux

OTHER_FILES += \
    ../tst_local.cpp \
    ../../bus/BusServer.h  ../../bus/BusServer.cpp \
    ../../bus/Catalog.h    ../../bus/Catalog.cpp
