/*-------------------------------------------------------------
osk_sysdict.c -- the built-in dictionary of the console settings
-------------------------------------------------------------*/
#include "osk_internal.h"

const osk_dictionary *OSK_SystemDictionary(void)
{
	return OSK_LanguageDictionary(OSK_SystemLanguage());
}
