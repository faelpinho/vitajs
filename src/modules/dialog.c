#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <psp2/apputil.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/libime.h>
#include <psp2/message_dialog.h>
#include <psp2/sysmodule.h>
#include <psp2/system_param.h>
#include <vita2d.h>

#include "../env.h"

typedef enum VitaJSDialogKind
{
	VITAJS_DIALOG_NONE = 0,
	VITAJS_DIALOG_MESSAGE,
	VITAJS_DIALOG_IME
} VitaJSDialogKind;

static VitaJSDialogKind current_dialog = VITAJS_DIALOG_NONE;
static int common_dialog_initialized = 0;
static int apputil_module_loaded_by_us = 0;
static int ime_module_loaded_by_us = 0;

static SceMsgDialogParam message_param;
static SceMsgDialogUserMessageParam message_user_param;
static char message_text[SCE_MSG_DIALOG_USER_MSG_SIZE + 1];
static int message_button_type = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

static SceImeDialogParam ime_param;
static SceWChar16 ime_title[SCE_IME_DIALOG_MAX_TITLE_LENGTH + 1];
static SceWChar16 ime_initial[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
static SceWChar16 ime_input[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];

static JSValue throw_sce_error(JSContext *ctx, const char *name, int error)
{
	return JS_ThrowInternalError(
		ctx,
		"%s failed: 0x%08X",
		name,
		(unsigned int)error);
}

static int set_property(JSContext *ctx, JSValue object, const char *name, JSValue value)
{
	if (JS_SetPropertyStr(ctx, object, name, value) < 0)
		return -1;

	return 0;
}

static size_t utf8_to_utf16(
	const char *src,
	SceWChar16 *dst,
	size_t dst_capacity)
{
	if (!dst_capacity)
		return 0;

	const unsigned char *s = (const unsigned char *)src;
	const unsigned char *end = s + strlen(src);
	size_t out = 0;

	while (s < end && out + 1 < dst_capacity)
	{
		uint32_t cp;
		size_t consumed;
		size_t remaining = (size_t)(end - s);

		if (s[0] < 0x80)
		{
			cp = s[0];
			consumed = 1;
		}
		else if (
			remaining >= 2 &&
			(s[0] & 0xE0) == 0xC0 &&
			(s[1] & 0xC0) == 0x80)
		{
			cp =
				((uint32_t)(s[0] & 0x1F) << 6) |
				(uint32_t)(s[1] & 0x3F);
			consumed = 2;

			if (cp < 0x80)
			{
				cp = 0xFFFD;
				consumed = 1;
			}
		}
		else if (
			remaining >= 3 &&
			(s[0] & 0xF0) == 0xE0 &&
			(s[1] & 0xC0) == 0x80 &&
			(s[2] & 0xC0) == 0x80)
		{
			cp =
				((uint32_t)(s[0] & 0x0F) << 12) |
				((uint32_t)(s[1] & 0x3F) << 6) |
				(uint32_t)(s[2] & 0x3F);
			consumed = 3;

			if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
			{
				cp = 0xFFFD;
				consumed = 1;
			}
		}
		else if (
			remaining >= 4 &&
			(s[0] & 0xF8) == 0xF0 &&
			(s[1] & 0xC0) == 0x80 &&
			(s[2] & 0xC0) == 0x80 &&
			(s[3] & 0xC0) == 0x80)
		{
			cp =
				((uint32_t)(s[0] & 0x07) << 18) |
				((uint32_t)(s[1] & 0x3F) << 12) |
				((uint32_t)(s[2] & 0x3F) << 6) |
				(uint32_t)(s[3] & 0x3F);
			consumed = 4;

			if (cp < 0x10000 || cp > 0x10FFFF)
			{
				cp = 0xFFFD;
				consumed = 1;
			}
		}
		else
		{
			cp = 0xFFFD;
			consumed = 1;
		}

		s += consumed;

		if (cp <= 0xFFFF)
		{
			dst[out++] = (SceWChar16)cp;
		}
		else
		{
			if (out + 2 >= dst_capacity)
				break;

			cp -= 0x10000;
			dst[out++] = (SceWChar16)(0xD800 + (cp >> 10));
			dst[out++] = (SceWChar16)(0xDC00 + (cp & 0x3FF));
		}
	}

	dst[out] = 0;
	return out;
}

static size_t utf16_to_utf8(
	const SceWChar16 *src,
	char *dst,
	size_t dst_capacity)
{
	if (!dst_capacity)
		return 0;

	size_t in = 0;
	size_t out = 0;

	while (src[in] != 0)
	{
		uint32_t cp = src[in++];

		if (cp >= 0xD800 && cp <= 0xDBFF)
		{
			uint32_t low = src[in];
			if (low >= 0xDC00 && low <= 0xDFFF)
			{
				in++;
				cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
			}
			else
			{
				cp = 0xFFFD;
			}
		}
		else if (cp >= 0xDC00 && cp <= 0xDFFF)
		{
			cp = 0xFFFD;
		}

		if (cp < 0x80)
		{
			if (out + 1 >= dst_capacity)
				break;
			dst[out++] = (char)cp;
		}
		else if (cp < 0x800)
		{
			if (out + 2 >= dst_capacity)
				break;
			dst[out++] = (char)(0xC0 | (cp >> 6));
			dst[out++] = (char)(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000)
		{
			if (out + 3 >= dst_capacity)
				break;
			dst[out++] = (char)(0xE0 | (cp >> 12));
			dst[out++] = (char)(0x80 | ((cp >> 6) & 0x3F));
			dst[out++] = (char)(0x80 | (cp & 0x3F));
		}
		else
		{
			if (out + 4 >= dst_capacity)
				break;
			dst[out++] = (char)(0xF0 | (cp >> 18));
			dst[out++] = (char)(0x80 | ((cp >> 12) & 0x3F));
			dst[out++] = (char)(0x80 | ((cp >> 6) & 0x3F));
			dst[out++] = (char)(0x80 | (cp & 0x3F));
		}
	}

	dst[out] = '\0';
	return out;
}

/*
 * IMPORTANT: call this before vita2d_init().
 * Real hardware is stricter than Vita3K about CommonDialog/AppUtil/GXM order.
 */
int vitajs_dialog_system_init(void)
{
	if (common_dialog_initialized)
		return 0;

	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_APPUTIL) < 0)
	{
		int load_result = sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);
		if (load_result < 0)
			return load_result;

		apputil_module_loaded_by_us = 1;
	}

	SceAppUtilInitParam app_init;
	SceAppUtilBootParam boot_param;
	memset(&app_init, 0, sizeof(app_init));
	memset(&boot_param, 0, sizeof(boot_param));

	int result = sceAppUtilInit(&app_init, &boot_param);
	if (result < 0)
		return result;

	SceCommonDialogConfigParam config;
	sceCommonDialogConfigParamInit(&config);

	/* Match established Vita homebrew initialization patterns. */
	(void)sceAppUtilSystemParamGetInt(
		SCE_SYSTEM_PARAM_ID_LANG,
		(int *)&config.language);

	(void)sceAppUtilSystemParamGetInt(
		SCE_SYSTEM_PARAM_ID_ENTER_BUTTON,
		(int *)&config.enterButtonAssign);

	result = sceCommonDialogSetConfigParam(&config);
	if (result < 0)
	{
		sceAppUtilShutdown();
		return result;
	}

	common_dialog_initialized = 1;
	return 0;
}

void vitajs_dialog_system_shutdown(void)
{
	if (current_dialog == VITAJS_DIALOG_MESSAGE)
	{
		(void)sceMsgDialogAbort();
		(void)sceMsgDialogTerm();
	}
	else if (current_dialog == VITAJS_DIALOG_IME)
	{
		(void)sceImeDialogAbort();
		(void)sceImeDialogTerm();
	}

	current_dialog = VITAJS_DIALOG_NONE;

	if (ime_module_loaded_by_us)
	{
		(void)sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);
		ime_module_loaded_by_us = 0;
	}

	if (common_dialog_initialized)
	{
		(void)sceAppUtilShutdown();
		common_dialog_initialized = 0;
	}

	if (apputil_module_loaded_by_us)
	{
		(void)sceSysmoduleUnloadModule(SCE_SYSMODULE_APPUTIL);
		apputil_module_loaded_by_us = 0;
	}
}

static int ensure_common_dialog_initialized(void)
{
	/*
	 * Do NOT lazily initialize here after vita2d_init().
	 * main.c must call vitajs_dialog_system_init() first.
	 */
	return common_dialog_initialized ? 0 : -1;
}

static int ensure_ime_module_loaded(void)
{
	if (sceSysmoduleIsLoaded(SCE_SYSMODULE_IME) >= 0)
		return 0;

	int result = sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
	if (result < 0)
		return result;

	ime_module_loaded_by_us = 1;
	return 0;
}

static SceCommonDialogStatus get_native_status(void)
{
	switch (current_dialog)
	{
	case VITAJS_DIALOG_MESSAGE:
		return sceMsgDialogGetStatus();
	case VITAJS_DIALOG_IME:
		return sceImeDialogGetStatus();
	default:
		return SCE_COMMON_DIALOG_STATUS_NONE;
	}
}

static const char *status_name(SceCommonDialogStatus status)
{
	switch (status)
	{
	case SCE_COMMON_DIALOG_STATUS_RUNNING:
		return "running";
	case SCE_COMMON_DIALOG_STATUS_FINISHED:
		return "finished";
	case SCE_COMMON_DIALOG_STATUS_NONE:
	default:
		return "none";
	}
}

static int parse_message_button_type(const char *buttons)
{
	if (strcmp(buttons, "ok") == 0)
		return SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	if (strcmp(buttons, "yesNo") == 0)
		return SCE_MSG_DIALOG_BUTTON_TYPE_YESNO;
	if (strcmp(buttons, "none") == 0)
		return SCE_MSG_DIALOG_BUTTON_TYPE_NONE;
	if (strcmp(buttons, "okCancel") == 0)
		return SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL;
	if (strcmp(buttons, "cancel") == 0)
		return SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL;

	return -1;
}

static JSValue vitajs_dialog_message(
	JSContext *ctx,
	JSValue this_val,
	int argc,
	JSValueConst *argv)
{
	(void)this_val;

	if (argc != 1 || !JS_IsObject(argv[0]))
		return JS_ThrowSyntaxError(ctx, "message(options) expects one object argument");

	if (current_dialog != VITAJS_DIALOG_NONE)
		return JS_ThrowInternalError(ctx, "A native dialog is already active");

	if (ensure_common_dialog_initialized() < 0)
		return JS_ThrowInternalError(
			ctx,
			"CommonDialog is not initialized; vitajs_dialog_system_init() must run before vita2d_init()");

	JSValue text_value = JS_GetPropertyStr(ctx, argv[0], "text");
	if (JS_IsUndefined(text_value) || JS_IsNull(text_value))
	{
		JS_FreeValue(ctx, text_value);
		return JS_ThrowTypeError(ctx, "message(options) requires options.text");
	}

	const char *text = JS_ToCString(ctx, text_value);
	JS_FreeValue(ctx, text_value);
	if (!text)
		return JS_EXCEPTION;

	size_t text_length = strlen(text);
	if (text_length > SCE_MSG_DIALOG_USER_MSG_SIZE)
	{
		JS_FreeCString(ctx, text);
		return JS_ThrowRangeError(
			ctx,
			"Dialog message is too long (maximum %d bytes)",
			SCE_MSG_DIALOG_USER_MSG_SIZE);
	}

	memcpy(message_text, text, text_length + 1);
	JS_FreeCString(ctx, text);

	message_button_type = SCE_MSG_DIALOG_BUTTON_TYPE_OK;

	JSValue buttons_value = JS_GetPropertyStr(ctx, argv[0], "buttons");
	if (!JS_IsUndefined(buttons_value) && !JS_IsNull(buttons_value))
	{
		const char *buttons = JS_ToCString(ctx, buttons_value);
		JS_FreeValue(ctx, buttons_value);
		if (!buttons)
			return JS_EXCEPTION;

		message_button_type = parse_message_button_type(buttons);
		JS_FreeCString(ctx, buttons);

		if (message_button_type < 0)
		{
			return JS_ThrowRangeError(
				ctx,
				"buttons must be 'ok', 'yesNo', 'okCancel', 'cancel' or 'none'");
		}
	}
	else
	{
		JS_FreeValue(ctx, buttons_value);
	}

	memset(&message_user_param, 0, sizeof(message_user_param));
	message_user_param.buttonType = message_button_type;
	message_user_param.msg = (const SceChar8 *)message_text;

	sceMsgDialogParamInit(&message_param);
	message_param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	message_param.userMsgParam = &message_user_param;

	int result = sceMsgDialogInit(&message_param);
	if (result < 0)
		return throw_sce_error(ctx, "sceMsgDialogInit", result);
	SceCommonDialogStatus status =
		sceMsgDialogGetStatus();

	if (status != SCE_COMMON_DIALOG_STATUS_RUNNING)
	{
		return JS_ThrowInternalError(
			ctx,
			"sceMsgDialogInit returned 0x%08X but status is %d",
			(unsigned int)result,
			(int)status);
	}

	current_dialog = VITAJS_DIALOG_MESSAGE;
	return JS_TRUE;
}

static uint64_t supported_ime_languages(void)
{
	return SCE_IME_LANGUAGE_DANISH |
		   SCE_IME_LANGUAGE_GERMAN |
		   SCE_IME_LANGUAGE_ENGLISH |
		   SCE_IME_LANGUAGE_SPANISH |
		   SCE_IME_LANGUAGE_FRENCH |
		   SCE_IME_LANGUAGE_ITALIAN |
		   SCE_IME_LANGUAGE_DUTCH |
		   SCE_IME_LANGUAGE_NORWEGIAN |
		   SCE_IME_LANGUAGE_POLISH |
		   SCE_IME_LANGUAGE_PORTUGUESE |
		   SCE_IME_LANGUAGE_RUSSIAN |
		   SCE_IME_LANGUAGE_FINNISH |
		   SCE_IME_LANGUAGE_SWEDISH |
		   SCE_IME_LANGUAGE_JAPANESE |
		   SCE_IME_LANGUAGE_KOREAN |
		   SCE_IME_LANGUAGE_SIMPLIFIED_CHINESE |
		   SCE_IME_LANGUAGE_TRADITIONAL_CHINESE |
		   SCE_IME_LANGUAGE_PORTUGUESE_BR |
		   SCE_IME_LANGUAGE_ENGLISH_GB |
		   SCE_IME_LANGUAGE_TURKISH;
}

static int ime_type_from_string(const char *type)
{
	if (strcmp(type, "text") == 0)
		return SCE_IME_TYPE_DEFAULT;
	if (strcmp(type, "latin") == 0)
		return SCE_IME_TYPE_BASIC_LATIN;
	if (strcmp(type, "number") == 0)
		return SCE_IME_TYPE_NUMBER;
	if (strcmp(type, "extendedNumber") == 0)
		return SCE_IME_TYPE_EXTENDED_NUMBER;
	if (strcmp(type, "url") == 0)
		return SCE_IME_TYPE_URL;
	if (strcmp(type, "email") == 0)
		return SCE_IME_TYPE_MAIL;

	return -1;
}

static int ime_enter_label_from_string(const char *label)
{
	if (strcmp(label, "default") == 0)
		return SCE_IME_ENTER_LABEL_DEFAULT;
	if (strcmp(label, "send") == 0)
		return SCE_IME_ENTER_LABEL_SEND;
	if (strcmp(label, "search") == 0)
		return SCE_IME_ENTER_LABEL_SEARCH;
	if (strcmp(label, "go") == 0)
		return SCE_IME_ENTER_LABEL_GO;

	return -1;
}

static int read_optional_bool(
	JSContext *ctx,
	JSValue object,
	const char *name,
	int default_value,
	int *out)
{
	JSValue value = JS_GetPropertyStr(ctx, object, name);

	if (JS_IsUndefined(value) || JS_IsNull(value))
	{
		JS_FreeValue(ctx, value);
		*out = default_value;
		return 0;
	}

	int result = JS_ToBool(ctx, value);
	JS_FreeValue(ctx, value);

	if (result < 0)
		return -1;

	*out = result;
	return 0;
}

static JSValue vitajs_dialog_keyboard(
	JSContext *ctx,
	JSValue this_val,
	int argc,
	JSValueConst *argv)
{
	(void)this_val;

	if (argc != 1 || !JS_IsObject(argv[0]))
		return JS_ThrowSyntaxError(ctx, "keyboard(options) expects one object argument");

	if (current_dialog != VITAJS_DIALOG_NONE)
		return JS_ThrowInternalError(ctx, "A native dialog is already active");

	if (ensure_common_dialog_initialized() < 0)
		return JS_ThrowInternalError(
			ctx,
			"CommonDialog is not initialized; vitajs_dialog_system_init() must run before vita2d_init()");

	int result = ensure_ime_module_loaded();
	if (result < 0)
		return throw_sce_error(ctx, "sceSysmoduleLoadModule(SCE_SYSMODULE_IME)", result);

	memset(ime_title, 0, sizeof(ime_title));
	memset(ime_initial, 0, sizeof(ime_initial));
	memset(ime_input, 0, sizeof(ime_input));

	JSValue value = JS_GetPropertyStr(ctx, argv[0], "title");
	if (!JS_IsUndefined(value) && !JS_IsNull(value))
	{
		const char *title = JS_ToCString(ctx, value);
		JS_FreeValue(ctx, value);
		if (!title)
			return JS_EXCEPTION;

		utf8_to_utf16(title, ime_title, countof(ime_title));
		JS_FreeCString(ctx, title);
	}
	else
	{
		JS_FreeValue(ctx, value);
	}

	uint32_t max_length = 128;
	value = JS_GetPropertyStr(ctx, argv[0], "maxLength");
	if (!JS_IsUndefined(value) && !JS_IsNull(value))
	{
		if (JS_ToUint32(ctx, &max_length, value))
		{
			JS_FreeValue(ctx, value);
			return JS_EXCEPTION;
		}
		JS_FreeValue(ctx, value);
	}
	else
	{
		JS_FreeValue(ctx, value);
	}

	if (max_length < 1 || max_length > SCE_IME_DIALOG_MAX_TEXT_LENGTH)
	{
		return JS_ThrowRangeError(
			ctx,
			"maxLength must be between 1 and %d",
			SCE_IME_DIALOG_MAX_TEXT_LENGTH);
	}

	value = JS_GetPropertyStr(ctx, argv[0], "initialText");
	if (!JS_IsUndefined(value) && !JS_IsNull(value))
	{
		const char *initial = JS_ToCString(ctx, value);
		JS_FreeValue(ctx, value);
		if (!initial)
			return JS_EXCEPTION;

		size_t initial_length = utf8_to_utf16(
			initial,
			ime_initial,
			countof(ime_initial));
		JS_FreeCString(ctx, initial);

		if (initial_length > max_length)
			ime_initial[max_length] = 0;
	}
	else
	{
		JS_FreeValue(ctx, value);
	}

	memcpy(ime_input, ime_initial, sizeof(ime_initial));

	int type = SCE_IME_TYPE_DEFAULT;
	value = JS_GetPropertyStr(ctx, argv[0], "type");
	if (!JS_IsUndefined(value) && !JS_IsNull(value))
	{
		const char *type_string = JS_ToCString(ctx, value);
		JS_FreeValue(ctx, value);
		if (!type_string)
			return JS_EXCEPTION;

		type = ime_type_from_string(type_string);
		JS_FreeCString(ctx, type_string);

		if (type < 0)
		{
			return JS_ThrowRangeError(
				ctx,
				"type must be 'text', 'latin', 'number', 'extendedNumber', 'url' or 'email'");
		}
	}
	else
	{
		JS_FreeValue(ctx, value);
	}

	int password;
	int with_clear;
	int with_cancel;
	int multiline;
	int no_auto_capitalization;
	int no_assistance;

	if (read_optional_bool(ctx, argv[0], "password", 0, &password) < 0 ||
		read_optional_bool(ctx, argv[0], "withClear", 0, &with_clear) < 0 ||
		read_optional_bool(ctx, argv[0], "withCancel", 1, &with_cancel) < 0 ||
		read_optional_bool(ctx, argv[0], "multiline", 0, &multiline) < 0 ||
		read_optional_bool(ctx, argv[0], "noAutoCapitalization", 0, &no_auto_capitalization) < 0 ||
		read_optional_bool(ctx, argv[0], "noAssistance", 0, &no_assistance) < 0)
	{
		return JS_EXCEPTION;
	}

	int enter_label = SCE_IME_ENTER_LABEL_DEFAULT;
	value = JS_GetPropertyStr(ctx, argv[0], "enterLabel");
	if (!JS_IsUndefined(value) && !JS_IsNull(value))
	{
		const char *label = JS_ToCString(ctx, value);
		JS_FreeValue(ctx, value);
		if (!label)
			return JS_EXCEPTION;

		enter_label = ime_enter_label_from_string(label);
		JS_FreeCString(ctx, label);

		if (enter_label < 0)
		{
			return JS_ThrowRangeError(
				ctx,
				"enterLabel must be 'default', 'send', 'search' or 'go'");
		}
	}
	else
	{
		JS_FreeValue(ctx, value);
	}

	sceImeDialogParamInit(&ime_param);
	ime_param.supportedLanguages = supported_ime_languages();
	ime_param.languagesForced = SCE_FALSE;
	ime_param.type = (SceUInt32)type;
	ime_param.dialogMode = with_cancel
							   ? SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL
							   : SCE_IME_DIALOG_DIALOG_MODE_DEFAULT;

	if (password)
		ime_param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_PASSWORD;
	else if (with_clear)
		ime_param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
	else
		ime_param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_DEFAULT;

	ime_param.option = 0;
	if (multiline)
		ime_param.option |= SCE_IME_OPTION_MULTILINE;
	if (no_auto_capitalization)
		ime_param.option |= SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
	if (no_assistance)
		ime_param.option |= SCE_IME_OPTION_NO_ASSISTANCE;

	ime_param.title = ime_title;
	ime_param.maxTextLength = max_length;
	ime_param.initialText = ime_initial;
	ime_param.inputTextBuffer = ime_input;
	ime_param.enterLabel = (SceUChar8)enter_label;

	result = sceImeDialogInit(&ime_param);
	if (result < 0)
		return throw_sce_error(ctx, "sceImeDialogInit", result);

	current_dialog = VITAJS_DIALOG_IME;
	return JS_TRUE;
}

static JSValue vitajs_dialog_status(
	JSContext *ctx,
	JSValue this_val,
	int argc,
	JSValueConst *argv)
{
	(void)this_val;
	(void)argv;

	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "status() expects no arguments");

	return JS_NewString(ctx, status_name(get_native_status()));
}

static JSValue vitajs_dialog_update(
	JSContext *ctx,
	JSValue this_val,
	int argc,
	JSValueConst *argv)
{
	(void)this_val;
	(void)argv;

	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "update() expects no arguments");

	if (current_dialog != VITAJS_DIALOG_NONE)
	{
		int result = vita2d_common_dialog_update();
		if (result < 0)
			return throw_sce_error(ctx, "sceCommonDialogUpdate", result);
	}

	return JS_NewString(ctx, status_name(get_native_status()));
}

static const char *common_result_name(int result)
{
	switch (result)
	{
	case SCE_COMMON_DIALOG_RESULT_OK:
		return "ok";
	case SCE_COMMON_DIALOG_RESULT_USER_CANCELED:
		return "canceled";
	case SCE_COMMON_DIALOG_RESULT_ABORTED:
		return "aborted";
	default:
		return "unknown";
	}
}

static const char *message_button_name(int button_id)
{
	switch (message_button_type)
	{
	case SCE_MSG_DIALOG_BUTTON_TYPE_YESNO:
		return button_id == 1 ? "yes" : button_id == 2 ? "no"
													   : "invalid";
	case SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL:
		return button_id == 1 ? "ok" : button_id == 2 ? "cancel"
													  : "invalid";
	case SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL:
		return button_id == 1 || button_id == 2 ? "cancel" : "invalid";
	case SCE_MSG_DIALOG_BUTTON_TYPE_OK:
		return button_id == 1 ? "ok" : "invalid";
	case SCE_MSG_DIALOG_BUTTON_TYPE_NONE:
	default:
		return "invalid";
	}
}

static JSValue vitajs_dialog_result(
	JSContext *ctx,
	JSValue this_val,
	int argc,
	JSValueConst *argv)
{
	(void)this_val;
	(void)argv;

	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "result() expects no arguments");

	if (current_dialog == VITAJS_DIALOG_NONE)
		return JS_NULL;

	if (get_native_status() != SCE_COMMON_DIALOG_STATUS_FINISHED)
		return JS_NULL;

	JSValue object = JS_NewObject(ctx);
	if (JS_IsException(object))
		return object;

	if (current_dialog == VITAJS_DIALOG_MESSAGE)
	{
		SceMsgDialogResult result;
		memset(&result, 0, sizeof(result));

		int native_result = sceMsgDialogGetResult(&result);
		if (native_result < 0)
		{
			JS_FreeValue(ctx, object);
			return throw_sce_error(ctx, "sceMsgDialogGetResult", native_result);
		}

		if (set_property(ctx, object, "type", JS_NewString(ctx, "message")) < 0 ||
			set_property(ctx, object, "result", JS_NewString(ctx, common_result_name(result.result))) < 0 ||
			set_property(ctx, object, "button", JS_NewString(ctx, message_button_name(result.buttonId))) < 0 ||
			set_property(ctx, object, "buttonId", JS_NewInt32(ctx, result.buttonId)) < 0)
		{
			JS_FreeValue(ctx, object);
			return JS_EXCEPTION;
		}

		native_result = sceMsgDialogTerm();
		current_dialog = VITAJS_DIALOG_NONE;

		if (native_result < 0)
		{
			JS_FreeValue(ctx, object);
			return throw_sce_error(ctx, "sceMsgDialogTerm", native_result);
		}

		return object;
	}

	SceImeDialogResult result;
	memset(&result, 0, sizeof(result));

	int native_result = sceImeDialogGetResult(&result);
	if (native_result < 0)
	{
		JS_FreeValue(ctx, object);
		return throw_sce_error(ctx, "sceImeDialogGetResult", native_result);
	}

	char utf8[(SCE_IME_DIALOG_MAX_TEXT_LENGTH * 4) + 1];
	utf16_to_utf8(ime_input, utf8, sizeof(utf8));

	int confirmed =
		result.result == SCE_COMMON_DIALOG_RESULT_OK &&
		result.button == SCE_IME_DIALOG_BUTTON_ENTER;

	if (set_property(ctx, object, "type", JS_NewString(ctx, "keyboard")) < 0 ||
		set_property(ctx, object, "result", JS_NewString(ctx, common_result_name(result.result))) < 0 ||
		set_property(ctx, object, "confirmed", JS_NewBool(ctx, confirmed)) < 0 ||
		set_property(ctx, object, "text", JS_NewString(ctx, utf8)) < 0)
	{
		JS_FreeValue(ctx, object);
		return JS_EXCEPTION;
	}

	native_result = sceImeDialogTerm();
	current_dialog = VITAJS_DIALOG_NONE;

	if (native_result < 0)
	{
		JS_FreeValue(ctx, object);
		return throw_sce_error(ctx, "sceImeDialogTerm", native_result);
	}

	return object;
}

static JSValue vitajs_dialog_abort(
	JSContext *ctx,
	JSValue this_val,
	int argc,
	JSValueConst *argv)
{
	(void)this_val;
	(void)argv;

	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "abort() expects no arguments");

	if (current_dialog == VITAJS_DIALOG_NONE)
		return JS_FALSE;

	int result;
	if (current_dialog == VITAJS_DIALOG_MESSAGE)
		result = sceMsgDialogAbort();
	else
		result = sceImeDialogAbort();

	if (result < 0)
		return throw_sce_error(ctx, "dialog abort", result);

	return JS_TRUE;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("message", 1, vitajs_dialog_message),
	JS_CFUNC_DEF("keyboard", 1, vitajs_dialog_keyboard),
	JS_CFUNC_DEF("status", 0, vitajs_dialog_status),
	JS_CFUNC_DEF("update", 0, vitajs_dialog_update),
	JS_CFUNC_DEF("result", 0, vitajs_dialog_result),
	JS_CFUNC_DEF("abort", 0, vitajs_dialog_abort),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(
		ctx,
		m,
		module_funcs,
		countof(module_funcs));
}

JSModuleDef *vitajs_dialog_init(JSContext *ctx)
{
	return vitajs_push_module(
		ctx,
		module_init,
		module_funcs,
		countof(module_funcs),
		"Dialog");
}
