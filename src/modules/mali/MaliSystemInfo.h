#ifndef __MALI_SYSTEM_INFO_H__
#define __MALI_SYSTEM_INFO_H__

#ifndef MALIOS_VERSION
#define MALIOS_VERSION "0.7"
#endif

namespace MaliSystemInfo {
void showSystemInfo();
void showHardwareStatus();
void showEnergy();
void showAbout();
} // namespace MaliSystemInfo

#endif
