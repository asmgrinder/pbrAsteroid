/*
 * Physically Based Rendering
 * Forked from Michał Siejak PBR project
 */

#if _WIN32
#define EXPORT_SYMBOL __declspec(dllexport)
#else
#define EXPORT_SYMBOL
#endif

// Enable usage of more performant GPUs on laptops.
extern "C" {
	EXPORT_SYMBOL int NvOptimusEnablement = 1;
	EXPORT_SYMBOL int AmdPowerXpressRequestHighPerformance = 1;
}
