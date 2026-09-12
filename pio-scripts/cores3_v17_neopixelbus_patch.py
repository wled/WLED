# M5Stack CoreS3 / WLED V17 NeoPixelBus Production Integration
#
# PlatformIO PRE script for the m5stack_cores3 environment.
#
# This script makes the two hardware-validated NeoPixelBus fixes reproducible:
#
#   1) ESP32-S3 RMT output:
#      - DMA enabled
#      - 1024-symbol TX buffer
#      - non-blocking readiness check without ESP-IDF timeout=0 log noise
#      - guarded RMT handle lifecycle
#
#   2) ESP32-S3 LCD/GDMA output:
#      - fully stop/reset/disconnect/delete the GDMA channel when the last
#        LCD mux bus is destroyed
#      - prevents stale LCD peripheral ownership across runtime bus rebuilds
#
# The script patches the NeoPixelBus source downloaded by PlatformIO under
# .pio/libdeps. It does not require modified library files to be committed.
#
# NeoPixelBus itself is licensed under LGPL-3.0-or-later. This script keeps
# the upstream license header in the patched files untouched.
#
# The patch is idempotent and is safe to run on every PlatformIO build.

from pathlib import Path

Import("env")

RMT_MARKER = "CoreS3 NeoPixelBus RMT DMA1024 production patch"
LCD_MARKER = "CoreS3 NeoPixelBus LCD GDMA teardown production patch"

LCD_TEST_PRAGMAS = (
    '#pragma message("=== CORES3 TEST: ACTIVE NeoPixelBus NeoEsp32LcdXMethod.h IS COMPILED ===")',
    '#pragma message("=== CORES3 TEST: NeoEsp32LcdXMethod.h IS COMPILED ===")',
)


def _replace_function(source: str, signature: str, replacement: str) -> str:
    '''Replace one C++ function body by matching balanced braces.'''
    sig_pos = source.find(signature)
    if sig_pos < 0:
        raise RuntimeError(f"signature not found: {signature}")

    brace_start = source.find("{", sig_pos + len(signature))
    if brace_start < 0:
        raise RuntimeError(f"opening brace not found: {signature}")

    depth = 0
    in_string = False
    in_char = False
    escape = False
    line_comment = False
    block_comment = False
    i = brace_start

    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if line_comment:
            if ch == "\n":
                line_comment = False
            i += 1
            continue

        if block_comment:
            if ch == "*" and nxt == "/":
                block_comment = False
                i += 2
                continue
            i += 1
            continue

        if escape:
            escape = False
            i += 1
            continue

        if (in_string or in_char) and ch == "\\":
            escape = True
            i += 1
            continue

        if not in_string and not in_char:
            if ch == "/" and nxt == "/":
                line_comment = True
                i += 2
                continue
            if ch == "/" and nxt == "*":
                block_comment = True
                i += 2
                continue

        if not in_char and ch == '"':
            in_string = not in_string
            i += 1
            continue

        if not in_string and ch == "'":
            in_char = not in_char
            i += 1
            continue

        if not in_string and not in_char:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return source[:sig_pos] + replacement + source[i + 1:]

        i += 1

    raise RuntimeError(f"matching closing brace not found: {signature}")


def _find_rmt_target() -> Path:
    '''Locate the active NeoPixelBus RMT header used by this PIO environment.'''
    libdeps_root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")

    candidates = []
    for path in libdeps_root.rglob("NeoEsp32RmtXMethod.h"):
        try:
            text = path.read_text(encoding="utf-8")
        except Exception:
            continue

        if "NeoEsp32RmtMethodBase" in text and "rmt_new_tx_channel" in text:
            candidates.append(path)

    # WLED's source-pinned NeoPixelBus package is installed as NeoPixelBus@src-...
    # Prefer that copy when PlatformIO also leaves another NeoPixelBus directory.
    preferred = [p for p in candidates if "NeoPixelBus@src-" in str(p)]

    if len(preferred) == 1:
        return preferred[0]
    if len(candidates) == 1:
        return candidates[0]

    detail = "\n".join(f"  {p}" for p in candidates) or "  <none>"
    raise RuntimeError(
        "CoreS3 NeoPixelBus patch could not uniquely locate "
        "NeoEsp32RmtXMethod.h:\n" + detail
    )


def _find_lcd_target(rmt_target: Path) -> Path:
    '''
    Locate NeoEsp32LcdXMethod.h from the same active NeoPixelBus package.

    The sibling lookup is intentionally preferred so RMT and LCD patches can
    never be applied to different NeoPixelBus copies in .pio/libdeps.
    '''
    sibling = rmt_target.with_name("NeoEsp32LcdXMethod.h")
    if sibling.is_file():
        try:
            text = sibling.read_text(encoding="utf-8")
        except Exception as exc:
            raise RuntimeError(
                f"CoreS3 LCD patch could not read {sibling}: {exc}"
            ) from exc

        if "NeoEspLcdMonoBuffContext" in text and "gdma_connect" in text:
            return sibling

    libdeps_root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")
    candidates = []

    for path in libdeps_root.rglob("NeoEsp32LcdXMethod.h"):
        try:
            text = path.read_text(encoding="utf-8")
        except Exception:
            continue

        if "NeoEspLcdMonoBuffContext" in text and "gdma_connect" in text:
            candidates.append(path)

    preferred = [p for p in candidates if "NeoPixelBus@src-" in str(p)]

    if len(preferred) == 1:
        return preferred[0]
    if len(candidates) == 1:
        return candidates[0]

    detail = "\n".join(f"  {p}" for p in candidates) or "  <none>"
    raise RuntimeError(
        "CoreS3 NeoPixelBus patch could not uniquely locate "
        "NeoEsp32LcdXMethod.h:\n" + detail
    )


def _patch_rmt(target: Path) -> None:
    '''Apply the already validated ESP32-S3 RMT DMA1024 production patch.'''
    text = target.read_text(encoding="utf-8")

    if RMT_MARKER in text:
        required = [
            "config.mem_block_symbols = 1024;",
            "config.flags.with_dma = true;",
            "return (_channel != nullptr && _led_encoder != nullptr);",
        ]
        missing = [item for item in required if item not in text]
        if missing:
            raise RuntimeError(
                "CoreS3 RMT patch marker exists but verification failed; "
                f"missing: {missing}"
            )

        print(f"[CoreS3 RMT DMA1024] patch already present: {target.name}")
        return

    destructor = f'''    ~NeoEsp32RmtMethodBase()
    {{
        // {RMT_MARKER}
        if (_channel != nullptr)
        {{
            ESP_ERROR_CHECK_WITHOUT_ABORT(
                rmt_tx_wait_all_done(_channel, 10000 / portTICK_PERIOD_MS)
            );
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_disable(_channel));
        }}

        if (_led_encoder != nullptr)
        {{
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_del_encoder(_led_encoder));
            _led_encoder = nullptr;
        }}

        if (_channel != nullptr)
        {{
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_del_channel(_channel));
            _channel = nullptr;
        }}

        gpio_matrix_out(_pin, 0x100, false, false);
        pinMode(_pin, INPUT);

        free(_dataEditing);
        free(_dataSending);
    }}'''

    ready = f'''    bool IsReadyToUpdate() const
    {{
        // {RMT_MARKER}
        // ESP-IDF 5.5 emits an error log when timeout=0 is used as a busy poll.
        // Update() performs the actual completion wait before transmitting.
        return (_channel != nullptr && _led_encoder != nullptr);
    }}'''

    initialize = f'''    void Initialize()
    {{
        // {RMT_MARKER}
        rmt_tx_channel_config_t config = {{}};
        config.clk_src = RMT_CLK_SRC_DEFAULT;
        config.gpio_num = static_cast<gpio_num_t>(_pin);

#if defined(CONFIG_IDF_TARGET_ESP32S3)
        // CoreS3 / ESP32-S3 uses DMA with a 1024-symbol buffer.
        // A 32-pixel RGB frame is 768 RMT symbols, so the complete frame
        // fits in one DMA buffer and avoids the refill boundary implicated
        // in the previously observed tail-pixel anomaly.
        config.mem_block_symbols = 1024;
#else
        config.mem_block_symbols = 192;
#endif

        config.resolution_hz = T_SPEED::RmtTicksPerSecond;
        config.trans_queue_depth = 4;
        config.flags.invert_out = T_INVERTED::Inverted;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
        config.flags.with_dma = true;
#else
        config.flags.with_dma = false;
#endif

        esp_err_t ret = rmt_new_tx_channel(&config, &_channel);
        if (ret != ESP_OK || _channel == nullptr)
        {{
            _channel = nullptr;
            return;
        }}

        led_strip_encoder_config_t encoder_config = {{}};
        encoder_config.resolution = T_SPEED::RmtTicksPerSecond;
        _tx_config.loop_count = 0;

        ret = rmt_new_led_strip_encoder(
            &encoder_config,
            &_led_encoder,
            T_SPEED::RmtBit0,
            T_SPEED::RmtBit1
        );

        if (ret != ESP_OK || _led_encoder == nullptr)
        {{
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_del_channel(_channel));
            _channel = nullptr;
            _led_encoder = nullptr;
            return;
        }}

        ret = rmt_enable(_channel);
        if (ret != ESP_OK)
        {{
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_del_encoder(_led_encoder));
            _led_encoder = nullptr;
            ESP_ERROR_CHECK_WITHOUT_ABORT(rmt_del_channel(_channel));
            _channel = nullptr;
            return;
        }}
    }}'''

    update = f'''    void Update(bool maintainBufferConsistency)
    {{
        // {RMT_MARKER}
        if (_channel == nullptr || _led_encoder == nullptr)
        {{
            return;
        }}

        // Serialize writes: wait for the previous asynchronous RMT transfer
        // to finish before starting the next frame.
        if (ESP_OK == ESP_ERROR_CHECK_WITHOUT_ABORT(
                rmt_tx_wait_all_done(_channel, 10000 / portTICK_PERIOD_MS)))
        {{
            const esp_err_t ret = rmt_transmit(
                _channel,
                _led_encoder,
                _dataEditing,
                _sizeData,
                &_tx_config
            );

            if (ret != ESP_OK)
            {{
                return;
            }}

            if (maintainBufferConsistency)
            {{
                memcpy(_dataSending, _dataEditing, _sizeData);
            }}

            std::swap(_dataSending, _dataEditing);
        }}
    }}'''

    try:
        text = _replace_function(
            text, "~NeoEsp32RmtMethodBase()", destructor
        )
        text = _replace_function(
            text, "bool IsReadyToUpdate() const", ready
        )
        text = _replace_function(
            text, "void Initialize()", initialize
        )
        text = _replace_function(
            text, "void Update(bool maintainBufferConsistency)", update
        )
    except RuntimeError as exc:
        raise RuntimeError(
            f"CoreS3 RMT DMA1024 patch failed for {target}: {exc}"
        ) from exc

    required = [
        RMT_MARKER,
        "config.mem_block_symbols = 1024;",
        "config.flags.with_dma = true;",
        "return (_channel != nullptr && _led_encoder != nullptr);",
    ]
    for item in required:
        if item not in text:
            raise RuntimeError(
                f"CoreS3 RMT DMA1024 verification failed: missing {item}"
            )

    target.write_text(text, encoding="utf-8", newline="\n")
    print(
        "[CoreS3 RMT DMA1024] applied ESP32-S3 DMA / "
        f"1024-symbol patch: {target}"
    )


def _patch_lcd_gdma(target: Path) -> None:
    '''Apply the validated LCD/GDMA full teardown and remove TEST-only markers.'''
    text = target.read_text(encoding="utf-8")
    original = text

    # Remove the compile-time TEST banner used during hardware diagnosis.
    for pragma in LCD_TEST_PRAGMAS:
        text = text.replace(pragma + "\n\n", "")
        text = text.replace(pragma + "\r\n\r\n", "")
        text = text.replace(pragma + "\n", "")
        text = text.replace(pragma + "\r\n", "")
        text = text.replace(pragma, "")

    if LCD_MARKER not in text:
        destruct = f'''    void Destruct()
    {{
        if (_dmaItems == nullptr)
        {{
            return;
        }}

        // {LCD_MARKER}
        //
        // gdma_reset() alone resets the channel state but does not release
        // the LCD peripheral ownership held by the GDMA driver. A runtime
        // WLED bus rebuild can therefore fail on the next gdma_connect().
        //
        // NeoEsp32LcdXMethodBase waits for the active LCD transfer to finish
        // before DeregisterMuxBus() reaches this teardown.
        if (_dmaChannel != nullptr)
        {{
            esp_err_t dmaResult;

            dmaResult = gdma_stop(_dmaChannel);
            if (dmaResult != ESP_OK)
            {{
                log_w(
                    "LCD GDMA stop during teardown failed: %d",
                    (int)dmaResult
                );
            }}

            dmaResult = gdma_reset(_dmaChannel);
            if (dmaResult != ESP_OK)
            {{
                log_w(
                    "LCD GDMA reset during teardown failed: %d",
                    (int)dmaResult
                );
            }}

            dmaResult = gdma_disconnect(_dmaChannel);
            if (dmaResult != ESP_OK)
            {{
                log_w(
                    "LCD GDMA disconnect during teardown failed: %d",
                    (int)dmaResult
                );
            }}

            dmaResult = gdma_del_channel(_dmaChannel);
            if (dmaResult != ESP_OK)
            {{
                log_w(
                    "LCD GDMA delete during teardown failed: %d",
                    (int)dmaResult
                );
            }}

            _dmaChannel = nullptr;
        }}

        periph_module_disable(PERIPH_LCD_CAM_MODULE);
        periph_module_reset(PERIPH_LCD_CAM_MODULE);

        heap_caps_free(LcdBuffer);
        heap_caps_free(_dmaItems);

        LcdBufferSize = 0;
        _dmaItems = nullptr;
        LcdBuffer = nullptr;

        MuxMap.Reset();
    }}'''

        try:
            text = _replace_function(text, "void Destruct()", destruct)
        except RuntimeError as exc:
            raise RuntimeError(
                f"CoreS3 LCD GDMA teardown patch failed for {target}: {exc}"
            ) from exc

    required = [
        LCD_MARKER,
        "gdma_stop(_dmaChannel);",
        "gdma_reset(_dmaChannel);",
        "gdma_disconnect(_dmaChannel);",
        "gdma_del_channel(_dmaChannel);",
        "_dmaChannel = nullptr;",
    ]
    for item in required:
        if item not in text:
            raise RuntimeError(
                f"CoreS3 LCD GDMA verification failed: missing {item}"
            )

    for pragma in LCD_TEST_PRAGMAS:
        if pragma in text:
            raise RuntimeError(
                "CoreS3 LCD GDMA production cleanup failed: "
                f"TEST pragma still present: {pragma}"
            )

    if text == original:
        print(f"[CoreS3 LCD GDMA] patch already present: {target.name}")
        return

    target.write_text(text, encoding="utf-8", newline="\n")
    print(
        "[CoreS3 LCD GDMA] applied full GDMA teardown production patch: "
        f"{target}"
    )


def _apply_patches() -> None:
    rmt_target = _find_rmt_target()
    lcd_target = _find_lcd_target(rmt_target)

    _patch_rmt(rmt_target)
    _patch_lcd_gdma(lcd_target)


if not env.IsIntegrationDump():
    _apply_patches()
