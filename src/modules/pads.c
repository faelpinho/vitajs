#include <psp2/ctrl.h>
#include <psp2common/ctrl.h>

#include "../env.h"

static JSValue vitajs_getanalog(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int port = 0;

	if (argc == 1)
	{
		JS_ToInt32(ctx, &port, argv[0]);
		if (port > 1)
			return JS_ThrowSyntaxError(ctx, "wrong port number.");
	}

	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "btns", JS_NewUint32(ctx, ctrl.buttons), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "lx", JS_NewUint32(ctx, ctrl.lx), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "ly", JS_NewUint32(ctx, ctrl.ly), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "rx", JS_NewUint32(ctx, ctrl.rx), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "ry", JS_NewUint32(ctx, ctrl.ry), JS_PROP_C_W_E);

	return obj;
}

static JSValue vitajs_rumble(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2 && argc != 3)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected at least two (small: 0-255, large: 0-255, port? = 0)");

	static char actAlign[2];
	int port = 0;

	if (argc == 3)
	{
		JS_ToInt32(ctx, actAlign[0], argv[0]);
		JS_ToInt32(ctx, actAlign[1], argv[1]);
		JS_ToInt32(ctx, port, argv[2]);
	}
	else
	{
		JS_ToInt32(ctx, actAlign[0], argv[0]);
		JS_ToInt32(ctx, actAlign[1], argv[1]);
	}

	SceCtrlActuator *actuator;
	actuator->small = 255;
	actuator->large = 128;

	sceCtrlSetActuator(port, actuator);

	return JS_UNDEFINED;
}

static JSValue vitajs_check(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int pad, button;
	JSValue val;

	val = JS_GetPropertyStr(ctx, argv[0], "btns");
	JS_ToUint32(ctx, &pad, val);

	JS_ToInt32(ctx, &button, argv[1]);

	JS_FreeValue(ctx, val);

	return JS_NewBool(ctx, (pad & button));
}

static JSValue vitajs_get_battery_info(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc > 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments. Expected one (port = 1).");

	int port = 1;
	uint8_t *batteryLevel;

	JS_ToInt32(ctx, &port, argv[0]);

	if (sceCtrlGetBatteryInfo(port, batteryLevel) < 0)
		return JS_NewUint32(ctx, 0xFF); // unknown

	return JS_NewUint32(ctx, batteryLevel);
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("analog", 1, vitajs_getanalog),
	JS_CFUNC_DEF("check", 2, vitajs_check),
	JS_CFUNC_DEF("rumble", 3, vitajs_rumble),
	JS_CFUNC_DEF("battery_info", 0, vitajs_get_battery_info),
	JS_PROP_INT32_DEF("SELECT", SCE_CTRL_SELECT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("START", SCE_CTRL_START, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("UP", SCE_CTRL_UP, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("RIGHT", SCE_CTRL_RIGHT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DOWN", SCE_CTRL_DOWN, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("LEFT", SCE_CTRL_LEFT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TRIANGLE", SCE_CTRL_TRIANGLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CIRCLE", SCE_CTRL_CIRCLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CROSS", SCE_CTRL_CROSS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SQUARE", SCE_CTRL_SQUARE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("L1", SCE_CTRL_L1, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("L2", SCE_CTRL_L2, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("L3", SCE_CTRL_L3, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("R1", SCE_CTRL_R1, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("R2", SCE_CTRL_R2, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("R3", SCE_CTRL_R3, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("POWER", SCE_CTRL_POWER, JS_PROP_CONFIGURABLE), // ! needed to call sceCtrlPeekBufferPositiveExt2
	JS_PROP_INT32_DEF("MODE_DIGITAL", SCE_CTRL_MODE_DIGITAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("MODE_ANALOG", SCE_CTRL_MODE_ANALOG, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("MODE_ANALOG_WIDE", SCE_CTRL_MODE_ANALOG_WIDE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_UNPAIRED", SCE_CTRL_TYPE_UNPAIRED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_PSVITA", SCE_CTRL_TYPE_PHY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_PSTV", SCE_CTRL_TYPE_VIRT, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_DUALSHOCK3", SCE_CTRL_TYPE_DS3, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("TYPE_DUALSHOCK4", SCE_CTRL_TYPE_DS4, JS_PROP_CONFIGURABLE),

};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *vitajs_pads_init(JSContext *ctx)
{
	return vitajs_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Pads");
}
