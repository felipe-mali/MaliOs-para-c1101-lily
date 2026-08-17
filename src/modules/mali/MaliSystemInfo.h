#ifndef __MALI_SYSTEM_INFO_H__
#define __MALI_SYSTEM_INFO_H__

#ifndef MALIOS_VERSION
#define MALIOS_VERSION "dev"
#endif

namespace MaliSystemInfo {
void showSystemInfo();
void showHardwareStatus();
void showEnergy();
void showAbout();
} // namespace MaliSystemInfo

#endif
