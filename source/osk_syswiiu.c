/*-------------------------------------------------------------
osk_syswiiu.c -- the language of the Wii U's settings
-------------------------------------------------------------*/
#include "osk_internal.h"

#ifdef __WIIU__
#include <string.h>
#include <coreinit/mcp.h>
#include <coreinit/userconfig.h>

/* cafe.language */
enum {
	CAFE_JAPANESE, CAFE_ENGLISH, CAFE_FRENCH, CAFE_GERMAN, CAFE_ITALIAN,
	CAFE_SPANISH, CAFE_CHINESE, CAFE_KOREAN, CAFE_DUTCH
};

static int cafe_language(void)
{
	static UCSysConfig cfg __attribute__((aligned(0x40)));
	static uint32_t value __attribute__((aligned(0x40)));
	int handle = UCOpen();
	int lang = -1;

	if (handle < 0)
		return -1;
	memset(&cfg, 0, sizeof(cfg));
	strcpy(cfg.name, "cafe.language");
	cfg.dataType = UC_DATATYPE_UNSIGNED_INT;
	cfg.dataSize = sizeof(value);
	cfg.data = &value;
	if (UCReadSysConfig(handle, 1, &cfg) == UC_ERROR_OK)
		lang = (int)value;
	UCClose(handle);
	return lang;
}

static bool americas(void)
{
	static MCPSysProdSettings prod __attribute__((aligned(0x40)));
	int handle = MCP_Open();
	bool usa = false;

	if (handle < 0)
		return false;
	if (MCP_GetSysProdSettings(handle, &prod) == 0)
		usa = (prod.product_area & MCP_REGION_USA) != 0;
	MCP_Close(handle);
	return usa;
}

osk_language OSK_SystemLanguage(void)
{
	/* the Wii's CONF_LANG_* codes, as osk_conf_language() takes them */
	int lang;

	switch (cafe_language()) {
	case CAFE_ENGLISH:	lang = 1; break;
	case CAFE_GERMAN:	lang = 2; break;
	case CAFE_FRENCH:	lang = 3; break;
	case CAFE_SPANISH:	lang = 4; break;
	case CAFE_ITALIAN:	lang = 5; break;
	case CAFE_DUTCH:	lang = 6; break;
	default:		return OSK_LANG_NONE;
	}
	return osk_conf_language(lang, -1, americas() ? 1 : 0);
}

osk_layout OSK_SystemLayout(void)
{
	return OSK_LanguageLayout(OSK_SystemLanguage());
}
#endif
