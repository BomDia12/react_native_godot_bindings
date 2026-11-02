extends Node

const WAIT_MSEC := 20000
const TEST_ID := "game-translations"

var failed := false

@onready var root: ReactNativeRootView = $Root

func fail(message: String) -> void:
	if not failed:
		failed = true
		printerr("RN_SMOKE_FAILED: %s: %s" % [TEST_ID, message])
		get_tree().quit(1)

func js(code: String) -> Variant:
	return await ReactNativeHost.evaluate(code)

func wait_until(condition: Callable, label: String) -> bool:
	var deadline := Time.get_ticks_msec() + WAIT_MSEC
	while Time.get_ticks_msec() < deadline:
		if await condition.call():
			return true
		await get_tree().process_frame
	fail("timed out waiting for %s" % label)
	return false

func done() -> void:
	print("RN_SMOKE_OK: %s" % TEST_ID)
	get_tree().quit(0)

func _ready() -> void:
	_run.call_deferred()

func add_catalog(locale: String, greeting: String) -> void:
	var catalog := Translation.new()
	catalog.locale = locale
	catalog.add_message("GREETING", greeting)
	catalog.add_plural_message("ITEM", PackedStringArray(["%s item" % locale, "%s items" % locale]))
	TranslationServer.add_translation(catalog)

func _run() -> void:
	add_catalog("en", "Hello")
	add_catalog("pt_BR", "Ola")
	TranslationServer.set_locale("en")
	if not await wait_until(func(): return await js("__godotI18n.greeting === 'Hello'"), "the English greeting"):
		return
	if (await js("__godotI18n.items")) != "en items":
		fail("the plural did not translate: %s" % (await js("__godotI18n.items")))
		return
	if not await js("__godotI18n.getLoadedLocales().includes('pt_BR')"):
		fail("getLoadedLocales does not list the catalog")
		return
	await js("__godotI18n.setLocale('pt_BR')")
	if TranslationServer.get_locale() != "pt_BR":
		fail("setLocale did not reach Godot")
		return
	if not await wait_until(func(): return await js("__godotI18n.greeting === 'Ola' && __godotI18n.locale === 'pt_BR'"), "text to rerender in the new locale"):
		return
	TranslationServer.set_locale("en")
	if not await wait_until(func(): return await js("__godotI18n.greeting === 'Hello'"), "a locale set from GDScript to rerender"):
		return
	# Nothing listens any more, and tr() must still not read old strings.
	root.queue_free()
	await get_tree().process_frame
	await get_tree().process_frame
	await js("__godotI18n.tr('GREETING')")
	TranslationServer.set_locale("pt_BR")
	if not await wait_until(func(): return (await js("__godotI18n.tr('GREETING')")) == "Ola", "tr() to follow a locale nothing is listening to"):
		return
	done()
