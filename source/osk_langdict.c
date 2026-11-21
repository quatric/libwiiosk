/*-------------------------------------------------------------
osk_langdict.c -- built-in dictionary by language

Its own object: it references every dictionary.
-------------------------------------------------------------*/
#include "osk_internal.h"

const osk_dictionary *const *OSK_SystemDictionaries(unsigned *count)
{
	static const osk_dictionary *const americas[] = { &osk_dict_en_us, &osk_dict_fr_ca, &osk_dict_es_la };
	static const osk_dictionary *const europe[] = {
		&osk_dict_en_gb, &osk_dict_de, &osk_dict_fr, &osk_dict_es, &osk_dict_it, &osk_dict_nl,
	};
	osk_language lang = OSK_SystemLanguage();

	if (lang == OSK_LANG_EN_US || lang == OSK_LANG_FR_CA || lang == OSK_LANG_ES_LA) {
		*count = sizeof(americas) / sizeof(americas[0]);
		return americas;
	}
	*count = sizeof(europe) / sizeof(europe[0]);
	return europe;
}

const osk_dictionary *OSK_LanguageDictionary(osk_language lang)
{
	switch (lang) {
	case OSK_LANG_EN_US:	return &osk_dict_en_us;
	case OSK_LANG_EN_GB:	return &osk_dict_en_gb;
	case OSK_LANG_DE:	return &osk_dict_de;
	case OSK_LANG_FR:	return &osk_dict_fr;
	case OSK_LANG_FR_CA:	return &osk_dict_fr_ca;
	case OSK_LANG_ES:	return &osk_dict_es;
	case OSK_LANG_ES_LA:	return &osk_dict_es_la;
	case OSK_LANG_IT:	return &osk_dict_it;
	case OSK_LANG_NL:	return &osk_dict_nl;
	default:		return NULL;
	}
}
