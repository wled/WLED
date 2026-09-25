  // =========================================================
  // Shared paired-touch helpers
  // =========================================================


  bool isSelectedTouchPairInside( M5StackTouchTarget firstTarget, bool firstInside, M5StackTouchTarget secondTarget, bool secondInside ) {
    if ( touchState.touchTarget == firstTarget ) {
      return firstInside;
    }

    if ( touchState.touchTarget == secondTarget ) {
      return secondInside;
    }

    return false;
  }

  // =========================================================
  // Touch processing
  // =========================================================

  // =========================================================
  // Build one touch hit-test snapshot
  // =========================================================

  M5StackTouchHitState buildTouchHitState( int16_t touchX, int16_t touchY ) {
    M5StackTouchHitState hit;

    hit.insidePower = M5StackDisplayTouchHelpers::isPowerButtonTouched( touchX, touchY );

    if ( currentPage == SCREEN_MAIN ) {
      if ( getNetworkAccessMode() == NETWORK_ACCESS_NONE ) {
        hit.insideWiFiRecovery =
          M5StackDisplayTouchHelpers::pointInsideRect(
            touchX,
            touchY,
            M5STACK_TOUCH_WIFI_RECOVERY
          );
      }

      hit.insideBrightnessDown = M5StackDisplayTouchHelpers::isBrightnessDownTouched( touchX, touchY );

      hit.insideBrightnessUp = M5StackDisplayTouchHelpers::isBrightnessUpTouched( touchX, touchY );

      hit.insideEffectPrev = M5StackDisplayTouchHelpers::isEffectPrevTouched( touchX, touchY );

      hit.insideEffectDetail = M5StackDisplayTouchHelpers::isEffectDetailTouched( touchX, touchY );

      hit.insideEffectNext = M5StackDisplayTouchHelpers::isEffectNextTouched( touchX, touchY );

      hit.insideColor = M5StackDisplayTouchHelpers::isColorButtonTouched( touchX, touchY );

      hit.insidePresetOpen = M5StackDisplayTouchHelpers::isPresetOpenButtonTouched( touchX, touchY );
    }

    if ( currentPage == SCREEN_COLOR ) {
      hit.insideBack = M5StackDisplayTouchHelpers::isBackButtonTouched( touchX, touchY );

      const M5StackEffectColorCapabilities colorCapability =
        getEffectColorCapabilities( getCurrentEffectMode() );

      if ( colorCapability.color1 ) {
        hit.insideColorSlot1 =
          M5StackDisplayTouchHelpers::pointInsideRect(
            touchX,
            touchY,
            M5STACK_TOUCH_COLOR_SLOT_1
          );
      }

      if ( colorCapability.color2 ) {
        hit.insideColorSlot2 =
          M5StackDisplayTouchHelpers::pointInsideRect(
            touchX,
            touchY,
            M5STACK_TOUCH_COLOR_SLOT_2
          );
      }

      if ( colorCapability.color3 ) {
        hit.insideColorSlot3 =
          M5StackDisplayTouchHelpers::pointInsideRect(
            touchX,
            touchY,
            M5STACK_TOUCH_COLOR_SLOT_3
          );
      }

      if ( effectUsesAnyColor( colorCapability ) ) {
        hit.insideHueDown = M5StackDisplayTouchHelpers::isHueDownTouched( touchX, touchY );

        hit.insideHueUp = M5StackDisplayTouchHelpers::isHueUpTouched( touchX, touchY );

        hit.insideSaturationDown = M5StackDisplayTouchHelpers::isSaturationDownTouched( touchX, touchY );

        hit.insideSaturationUp = M5StackDisplayTouchHelpers::isSaturationUpTouched( touchX, touchY );
      }
    }

    if ( currentPage == SCREEN_EFFECT ) {
      hit.insideBack = M5StackDisplayTouchHelpers::isBackButtonTouched( touchX, touchY );

      const uint8_t effectMode = getCurrentEffectMode();

      if ( isEffectSpeedControlVisible( effectMode ) ) {
        hit.insideSpeedDown = M5StackDisplayTouchHelpers::isSpeedDownTouched( touchX, touchY );

        hit.insideSpeedUp = M5StackDisplayTouchHelpers::isSpeedUpTouched( touchX, touchY );
      }

      if ( isEffectIntensityControlVisible( effectMode ) ) {
        hit.insideIntensityDown = M5StackDisplayTouchHelpers::isIntensityDownTouched( touchX, touchY );

        hit.insideIntensityUp = M5StackDisplayTouchHelpers::isIntensityUpTouched( touchX, touchY );
      }

      if ( isEffectPaletteControlVisible( effectMode ) ) {
        hit.insidePalettePrev = M5StackDisplayTouchHelpers::isPalettePrevTouched( touchX, touchY );

        hit.insidePaletteNext = M5StackDisplayTouchHelpers::isPaletteNextTouched( touchX, touchY );
      }
    }

    if ( currentPage == SCREEN_PRESET ) {
      hit.insideBack = M5StackDisplayTouchHelpers::isBackButtonTouched( touchX, touchY );

      if ( presetSubPage == PRESET_SUBPAGE_NAV ) {
        hit.insidePresetPrev = M5StackDisplayTouchHelpers::isPresetPrevTouched( touchX, touchY );

        hit.insidePresetNext = M5StackDisplayTouchHelpers::isPresetNextTouched( touchX, touchY );

        hit.insidePresetManage = ( presetCacheReady && !presetCacheBuilding && pendingPresetId == 0 && M5StackDisplayTouchHelpers::isPresetManageTouched( touchX, touchY ) );
      }
      else if ( presetSubPage == PRESET_SUBPAGE_MANAGE ) {
        hit.insidePresetSaveNew = ( findFirstFreePresetId() > 0 && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetSaveNewTouched( touchX, touchY ) );

        hit.insidePresetOverwriteOpen = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetOverwriteOpenTouched( touchX, touchY ) );

        hit.insidePresetDeleteOpen = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetDeleteOpenTouched( touchX, touchY ) );

        hit.insidePresetBootOpen = ( presetCacheReady && !presetCacheBuilding && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetBootOpenTouched( touchX, touchY ) );
      }
      else if ( presetSubPage == PRESET_SUBPAGE_SAVE ) {
        hit.insidePresetSaveHold = ( presetSaveOperationState == PRESET_SAVE_OP_IDLE && presetCacheReady && !presetCacheBuilding && presetSaveCandidateId > 0 && findPresetCacheIndex( presetSaveCandidateId ) < 0 && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetSaveHoldTouched( touchX, touchY ) );
      }
      else if ( presetSubPage == PRESET_SUBPAGE_OVERWRITE ) {
        hit.insidePresetOverwritePrev = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && M5StackDisplayTouchHelpers::isPresetOverwritePrevTouched( touchX, touchY ) );

        hit.insidePresetOverwriteNext = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && M5StackDisplayTouchHelpers::isPresetOverwriteNextTouched( touchX, touchY ) );

        hit.insidePresetOverwriteHold = ( presetSaveOperationState == PRESET_SAVE_OP_IDLE && presetCacheReady && !presetCacheBuilding && presetOverwriteTargetId > 0 && findPresetCacheIndex( presetOverwriteTargetId ) >= 0 && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetOverwriteHoldTouched( touchX, touchY ) );
      }
      else if ( presetSubPage == PRESET_SUBPAGE_DELETE ) {
        hit.insidePresetDeletePrev = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && M5StackDisplayTouchHelpers::isPresetDeletePrevTouched( touchX, touchY ) );

        hit.insidePresetDeleteNext = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && M5StackDisplayTouchHelpers::isPresetDeleteNextTouched( touchX, touchY ) );

        hit.insidePresetDeleteHold = ( presetDeleteOperationState == PRESET_DELETE_OP_IDLE && presetCacheReady && !presetCacheBuilding && presetDeleteTargetId > 0 && findPresetCacheIndex( presetDeleteTargetId ) >= 0 && pendingPresetId == 0 && !presetNeedsSaving() && M5StackDisplayTouchHelpers::isPresetDeleteHoldTouched( touchX, touchY ) );
      }
      else if ( presetSubPage == PRESET_SUBPAGE_BOOT ) {
        hit.insidePresetBootPrev = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && M5StackDisplayTouchHelpers::isPresetBootPrevTouched( touchX, touchY ) );

        hit.insidePresetBootNext = ( presetCacheReady && !presetCacheBuilding && presetCacheCount > 0 && M5StackDisplayTouchHelpers::isPresetBootNextTouched( touchX, touchY ) );

        hit.insidePresetBootHold = ( presetBootOperationState == PRESET_BOOT_OP_IDLE && presetCacheReady && !presetCacheBuilding && pendingPresetId == 0 && !presetNeedsSaving() && isPresetBootTargetValid() && !isPresetBootTargetCurrent() && M5StackDisplayTouchHelpers::isPresetBootHoldTouched( touchX, touchY ) );
      }
    }


    return hit;
  }

  // =========================================================
  // Touch Press
  //
  // Starts a new gesture and acquires its M5StackTouchTarget.
  // =========================================================

  void handleTouchPress( const M5StackTouchFrameContext& context ) {
    M5StackTouchRuntimeState& touchState = context.state;
    const M5StackTouchHitState& hit = context.hit;
    const unsigned long now = context.now;

    if (!touchState.touchActive) {
      touchState.touchActive = true;

      touchState.touchTarget = M5STACK_TOUCH_TARGET_NONE;

      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.wifiRecoveryHoldState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.brightnessRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.effectRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.colorSlotHoldState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.hueRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.saturationRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.speedRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.intensityRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.paletteRepeatState );
      M5StackDisplayTouchHelpers::resetRepeatTouch( touchState.presetRepeatState );

      presetSaveHoldStartTime = 0;

      presetSaveHoldTriggered = false;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_NONE ) {
      if (hit.insidePower) {
        touchState.touchTarget = M5STACK_TOUCH_TARGET_POWER;

        touchState.lastTouchInsidePower = true;
      }

      else if ( currentPage == SCREEN_MAIN ) {
        if (hit.insideWiFiRecovery) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_WIFI_RECOVERY;

          touchState.lastTouchInsideWiFiRecovery = true;
          touchState.wifiRecoveryVisualPressed = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch(
            touchState.wifiRecoveryHoldState,
            now
          );

          drawMainNetworkStatusLine(
            "Hold... Recovery AP",
            TFT_YELLOW
          );
        }
        else if (hit.insideBrightnessDown) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN;

          touchState.lastTouchInsideBrightness = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.brightnessRepeatState, now );
        }
        else if (hit.insideBrightnessUp) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_BRIGHTNESS_UP;

          touchState.lastTouchInsideBrightness = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.brightnessRepeatState, now );
        }
        else if (hit.insideEffectPrev) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_EFFECT_PREV;

          touchState.lastTouchInsideEffect = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.effectRepeatState, now );
        }
        else if (hit.insideEffectDetail) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_EFFECT_DETAIL;

          touchState.lastTouchInsideEffectDetail = true;
        }
        else if (hit.insideEffectNext) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_EFFECT_NEXT;

          touchState.lastTouchInsideEffect = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.effectRepeatState, now );
        }
        else if (hit.insideColor) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_COLOR_OPEN;

          touchState.lastTouchInsideColor = true;
        }
        else if (hit.insidePresetOpen) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_OPEN;

          touchState.lastTouchInsidePresetOpen = true;
        }
      }

      else if ( currentPage == SCREEN_COLOR ) {
        if (hit.insideBack) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_BACK;

          touchState.lastTouchInsideBack = true;
        }
        else if (hit.insideColorSlot1) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_COLOR_SLOT_1;

          touchState.lastTouchInsideColorSlot = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.colorSlotHoldState, now );
        }
        else if (hit.insideColorSlot2) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_COLOR_SLOT_2;

          touchState.lastTouchInsideColorSlot = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.colorSlotHoldState, now );
        }
        else if (hit.insideColorSlot3) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_COLOR_SLOT_3;

          touchState.lastTouchInsideColorSlot = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.colorSlotHoldState, now );
        }
        else if (hit.insideHueDown) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_HUE_DOWN;

          touchState.lastTouchInsideHue = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.hueRepeatState, now );

          beginHueEdit();
        }
        else if (hit.insideHueUp) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_HUE_UP;

          touchState.lastTouchInsideHue = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.hueRepeatState, now );

          beginHueEdit();
        }
        else if (hit.insideSaturationDown) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_SATURATION_DOWN;

          touchState.lastTouchInsideSaturation = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.saturationRepeatState, now );

          beginSaturationEdit();
        }
        else if (hit.insideSaturationUp) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_SATURATION_UP;

          touchState.lastTouchInsideSaturation = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.saturationRepeatState, now );

          beginSaturationEdit();
        }
      }

      else if ( currentPage == SCREEN_EFFECT ) {
        if (hit.insideBack) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_BACK;

          touchState.lastTouchInsideBack = true;
        }
        else if (hit.insideSpeedDown) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_SPEED_DOWN;

          touchState.lastTouchInsideSpeed = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.speedRepeatState, now );
        }
        else if (hit.insideSpeedUp) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_SPEED_UP;

          touchState.lastTouchInsideSpeed = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.speedRepeatState, now );
        }
        else if (hit.insideIntensityDown) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_INTENSITY_DOWN;

          touchState.lastTouchInsideIntensity = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.intensityRepeatState, now );
        }
        else if (hit.insideIntensityUp) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_INTENSITY_UP;

          touchState.lastTouchInsideIntensity = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.intensityRepeatState, now );
        }
        else if (hit.insidePalettePrev) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PALETTE_PREV;

          touchState.lastTouchInsidePalette = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.paletteRepeatState, now );
        }
        else if (hit.insidePaletteNext) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PALETTE_NEXT;

          touchState.lastTouchInsidePalette = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.paletteRepeatState, now );
        }
      }

      else if ( currentPage == SCREEN_PRESET ) {
        if (hit.insideBack) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_BACK;

          touchState.lastTouchInsideBack = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_NAV && hit.insidePresetPrev ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_PREV;

          touchState.lastTouchInsidePresetNav = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.presetRepeatState, now );
        }
        else if ( presetSubPage == PRESET_SUBPAGE_NAV && hit.insidePresetNext ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_NEXT;

          touchState.lastTouchInsidePresetNav = true;

          M5StackDisplayTouchHelpers::beginRepeatTouch( touchState.presetRepeatState, now );
        }
        else if ( presetSubPage == PRESET_SUBPAGE_NAV && hit.insidePresetManage ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_MANAGE;

          touchState.lastTouchInsidePresetManage = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_MANAGE && hit.insidePresetSaveNew ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_SAVE_NEW;

          touchState.lastTouchInsidePresetSaveNew = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_MANAGE && hit.insidePresetOverwriteOpen ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_OPEN;

          touchState.lastTouchInsidePresetOverwriteOpen = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_MANAGE && hit.insidePresetDeleteOpen ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_DELETE_OPEN;

          touchState.lastTouchInsidePresetDeleteOpen = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_MANAGE && hit.insidePresetBootOpen ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_BOOT_OPEN;

          touchState.lastTouchInsidePresetBootOpen = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_SAVE && hit.insidePresetSaveHold ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_SAVE_HOLD;

          touchState.lastTouchInsidePresetSaveHold = true;

          presetSaveHoldStartTime = now;

          presetSaveHoldTriggered = false;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_OVERWRITE && hit.insidePresetOverwritePrev ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV;

          touchState.lastTouchInsidePresetOverwriteNav = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_OVERWRITE && hit.insidePresetOverwriteNext ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_NEXT;

          touchState.lastTouchInsidePresetOverwriteNav = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_OVERWRITE && hit.insidePresetOverwriteHold ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_HOLD;

          touchState.lastTouchInsidePresetOverwriteHold = true;

          presetSaveHoldStartTime = now;

          presetSaveHoldTriggered = false;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_DELETE && hit.insidePresetDeletePrev ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV;

          touchState.lastTouchInsidePresetDeleteNav = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_DELETE && hit.insidePresetDeleteNext ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_DELETE_NEXT;

          touchState.lastTouchInsidePresetDeleteNav = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_DELETE && hit.insidePresetDeleteHold ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_DELETE_HOLD;

          touchState.lastTouchInsidePresetDeleteHold = true;

          presetSaveHoldStartTime = now;

          presetSaveHoldTriggered = false;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_BOOT && hit.insidePresetBootPrev ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV;

          touchState.lastTouchInsidePresetBootNav = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_BOOT && hit.insidePresetBootNext ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_BOOT_NEXT;

          touchState.lastTouchInsidePresetBootNav = true;
        }
        else if ( presetSubPage == PRESET_SUBPAGE_BOOT && hit.insidePresetBootHold ) {
          touchState.touchTarget = M5STACK_TOUCH_TARGET_PRESET_BOOT_HOLD;

          touchState.lastTouchInsidePresetBootHold = true;

          presetSaveHoldStartTime = now;

          presetSaveHoldTriggered = false;
        }
      }
    }

  }

  // =========================================================
  // Touch Hold
  //
  // Preserves pressed visuals, long-press and repeat behavior.
  // =========================================================

  void handleTouchHold( const M5StackTouchFrameContext& context ) {
    M5StackTouchRuntimeState& touchState = context.state;
    const M5StackTouchHitState& hit = context.hit;
    const unsigned long now = context.now;

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_POWER ) {
      touchState.lastTouchInsidePower = hit.insidePower;

      if ( hit.insidePower != touchState.powerButtonVisualPressed ) {
        drawPowerButton( bri > 0, hit.insidePower );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_WIFI_RECOVERY ) {
      touchState.lastTouchInsideWiFiRecovery = hit.insideWiFiRecovery;

      if ( !hit.insideWiFiRecovery ) {
        // Leaving the header area cancels this hold completely. Re-entering
        // during the same gesture must not trigger Recovery AP immediately.
        touchState.wifiRecoveryHoldState.pressStart = 0;

        if ( touchState.wifiRecoveryVisualPressed ) {
          touchState.wifiRecoveryVisualPressed = false;

          drawMainNetworkStatusLine(
            getCurrentNetworkDisplayText()
          );
        }

        return;
      }

      if (
        !touchState.wifiRecoveryHoldState.longPressActive &&
        touchState.wifiRecoveryHoldState.pressStart > 0 &&
        now - touchState.wifiRecoveryHoldState.pressStart >= WIFI_RECOVERY_HOLD_MS
      ) {
        touchState.wifiRecoveryHoldState.longPressActive = true;

        drawMainNetworkStatusLine(
          "Starting Recovery AP...",
          TFT_YELLOW
        );

        if ( startWiFiRecoveryAP() ) {
          NetworkAccessMode recoveryMode = getNetworkAccessMode();
          String recoveryText = getNetworkDisplayText( recoveryMode );

          lastNetworkAccessMode = recoveryMode;
          lastNetworkDisplayText = recoveryText;

          touchState.wifiRecoveryVisualPressed = false;

          drawMainNetworkStatusLine(
            recoveryText,
            TFT_GREEN
          );
        }
        else {
          // Keep the visual marked active so release restores the normal
          // offline hint after a failed start attempt.
          drawMainNetworkStatusLine(
            "Recovery AP failed",
            TFT_RED
          );
        }
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN, M5STACK_TOUCH_TARGET_BRIGHTNESS_UP ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN, hit.insideBrightnessDown, M5STACK_TOUCH_TARGET_BRIGHTNESS_UP, hit.insideBrightnessUp );

      touchState.lastTouchInsideBrightness = insideSelectedButton;

      if ( insideSelectedButton != touchState.brightnessButtonVisualPressed ) {
        drawBrightness( bri, insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.brightnessButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.brightnessRepeatState, now, BRI_LONG_PRESS_MS, BRI_REPEAT_MS ) ) {
        brightnessLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_EFFECT_PREV, M5STACK_TOUCH_TARGET_EFFECT_NEXT ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_EFFECT_PREV, hit.insideEffectPrev, M5STACK_TOUCH_TARGET_EFFECT_NEXT, hit.insideEffectNext );

      touchState.lastTouchInsideEffect = insideSelectedButton;

      if ( insideSelectedButton != touchState.effectButtonVisualPressed ) {
        drawEffect( getCurrentEffectMode(), insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.effectButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.effectRepeatState, now, EFFECT_LONG_PRESS_MS, EFFECT_REPEAT_MS ) ) {
        effectLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_EFFECT_DETAIL ) {
      bool insideSelectedButton = hit.insideEffectDetail;

      touchState.lastTouchInsideEffectDetail = insideSelectedButton;

      if ( insideSelectedButton != touchState.effectDetailVisualPressed ) {
        drawEffectDetailButton( getCurrentEffectMode(), insideSelectedButton );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_OPEN ) {
      touchState.lastTouchInsideColor = hit.insideColor;

      if ( hit.insideColor != touchState.colorButtonVisualPressed ) {
        drawColorButton( getPrimaryColor(), hit.insideColor );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_OPEN ) {
      touchState.lastTouchInsidePresetOpen = hit.insidePresetOpen;

      if ( hit.insidePresetOpen != touchState.presetOpenButtonVisualPressed ) {
        drawPresetOpenButton( hit.insidePresetOpen );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_BACK ) {
      touchState.lastTouchInsideBack = hit.insideBack;

      if ( hit.insideBack != touchState.backButtonVisualPressed ) {
        drawBackButton( hit.insideBack );
      }

      return;
    }

    if (
      touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_1 ||
      touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_2 ||
      touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_3
    ) {
      bool insideSelectedSlot = false;

      if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_1 ) {
        insideSelectedSlot = hit.insideColorSlot1;
      }
      else if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_2 ) {
        insideSelectedSlot = hit.insideColorSlot2;
      }
      else {
        insideSelectedSlot = hit.insideColorSlot3;
      }

      touchState.lastTouchInsideColorSlot = insideSelectedSlot;

      if (!insideSelectedSlot) {
        return;
      }

      if (
        !touchState.colorSlotHoldState.longPressActive &&
        touchState.colorSlotHoldState.pressStart > 0 &&
        now - touchState.colorSlotHoldState.pressStart >= COLOR_SLOT_LONG_PRESS_MS
      ) {
        touchState.colorSlotHoldState.longPressActive = true;

        uint8_t colorSlot = 0;

        if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_2 ) {
          colorSlot = 1;
        }
        else if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_3 ) {
          colorSlot = 2;
        }

        toggleColorSlotBlack( colorSlot );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_HUE_DOWN, M5STACK_TOUCH_TARGET_HUE_UP ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_HUE_DOWN, hit.insideHueDown, M5STACK_TOUCH_TARGET_HUE_UP, hit.insideHueUp );

      touchState.lastTouchInsideHue = insideSelectedButton;

      if ( insideSelectedButton != touchState.hueButtonVisualPressed ) {
        drawHue( logicalHueValue, insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.hueButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.hueRepeatState, now, HUE_LONG_PRESS_MS, HUE_REPEAT_MS ) ) {
        hueLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_SATURATION_DOWN, M5STACK_TOUCH_TARGET_SATURATION_UP ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_SATURATION_DOWN, hit.insideSaturationDown, M5STACK_TOUCH_TARGET_SATURATION_UP, hit.insideSaturationUp );

      touchState.lastTouchInsideSaturation = insideSelectedButton;

      if ( insideSelectedButton != touchState.saturationButtonVisualPressed ) {
        drawSaturation( logicalSaturationValue, insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.saturationButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.saturationRepeatState, now, SATURATION_LONG_PRESS_MS, SATURATION_REPEAT_MS ) ) {
        saturationLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_SPEED_DOWN, M5STACK_TOUCH_TARGET_SPEED_UP ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_SPEED_DOWN, hit.insideSpeedDown, M5STACK_TOUCH_TARGET_SPEED_UP, hit.insideSpeedUp );

      touchState.lastTouchInsideSpeed = insideSelectedButton;

      if ( insideSelectedButton != touchState.speedButtonVisualPressed ) {
        drawSpeed( getCurrentSpeed(), insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.speedButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.speedRepeatState, now, SPEED_LONG_PRESS_MS, SPEED_REPEAT_MS ) ) {
        speedLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_INTENSITY_DOWN, M5STACK_TOUCH_TARGET_INTENSITY_UP ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_INTENSITY_DOWN, hit.insideIntensityDown, M5STACK_TOUCH_TARGET_INTENSITY_UP, hit.insideIntensityUp );

      touchState.lastTouchInsideIntensity = insideSelectedButton;

      if ( insideSelectedButton != touchState.intensityButtonVisualPressed ) {
        drawIntensity( getCurrentIntensity(), insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.intensityButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.intensityRepeatState, now, INTENSITY_LONG_PRESS_MS, INTENSITY_REPEAT_MS ) ) {
        intensityLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_PALETTE_PREV, M5STACK_TOUCH_TARGET_PALETTE_NEXT ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_PALETTE_PREV, hit.insidePalettePrev, M5STACK_TOUCH_TARGET_PALETTE_NEXT, hit.insidePaletteNext );

      touchState.lastTouchInsidePalette = insideSelectedButton;

      if ( insideSelectedButton != touchState.paletteButtonVisualPressed ) {
        drawPalette( getCurrentPalette(), insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.paletteButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.paletteRepeatState, now, PALETTE_LONG_PRESS_MS, PALETTE_REPEAT_MS ) ) {
        paletteLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_PRESET_PREV, M5STACK_TOUCH_TARGET_PRESET_NEXT ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_PRESET_PREV, hit.insidePresetPrev, M5STACK_TOUCH_TARGET_PRESET_NEXT, hit.insidePresetNext );

      touchState.lastTouchInsidePresetNav = insideSelectedButton;

      if ( insideSelectedButton != touchState.presetNavButtonVisualPressed ) {
        drawPresetNavigation( insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.presetNavButtonVisualPressed = insideSelectedButton;
      }

      if (!insideSelectedButton) {
        return;
      }

      if ( M5StackDisplayTouchHelpers::serviceRepeatTouch( touchState.presetRepeatState, now, PRESET_LONG_PRESS_MS, PRESET_REPEAT_MS ) ) {
        presetLongPressStep( touchState.touchTarget );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_MANAGE ) {
      touchState.lastTouchInsidePresetManage = hit.insidePresetManage;

      if ( hit.insidePresetManage != touchState.presetManageButtonVisualPressed ) {
        drawPresetManageButton( hit.insidePresetManage );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_SAVE_NEW ) {
      touchState.lastTouchInsidePresetSaveNew = hit.insidePresetSaveNew;

      if ( hit.insidePresetSaveNew != touchState.presetSaveNewButtonVisualPressed ) {
        drawPresetSaveNewButton( hit.insidePresetSaveNew );
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_SAVE_HOLD ) {
      touchState.lastTouchInsidePresetSaveHold = hit.insidePresetSaveHold;

      if ( hit.insidePresetSaveHold != touchState.presetSaveHoldButtonVisualPressed ) {
        drawPresetSaveHoldButton( hit.insidePresetSaveHold );
      }

      if (!hit.insidePresetSaveHold) {
        presetSaveHoldStartTime = 0;

        return;
      }

      if ( presetSaveHoldStartTime == 0 ) {
        presetSaveHoldStartTime = now;
      }

      if ( !presetSaveHoldTriggered && now - presetSaveHoldStartTime >= PRESET_SAVE_HOLD_MS ) {
        presetSaveHoldTriggered = true;

        requestNewPresetSave();

        return;
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_OPEN ) {
      touchState.lastTouchInsidePresetOverwriteOpen = hit.insidePresetOverwriteOpen;

      if ( hit.insidePresetOverwriteOpen != touchState.presetOverwriteOpenButtonVisualPressed ) {
        drawPresetOverwriteOpenButton( hit.insidePresetOverwriteOpen );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV, M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_NEXT ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV, hit.insidePresetOverwritePrev, M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_NEXT, hit.insidePresetOverwriteNext );

      touchState.lastTouchInsidePresetOverwriteNav = insideSelectedButton;

      if ( insideSelectedButton != touchState.presetOverwriteNavButtonVisualPressed ) {
        drawPresetOverwriteNavigation( insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.presetOverwriteNavButtonVisualPressed = insideSelectedButton;
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_HOLD ) {
      touchState.lastTouchInsidePresetOverwriteHold = hit.insidePresetOverwriteHold;

      if ( hit.insidePresetOverwriteHold != touchState.presetOverwriteHoldButtonVisualPressed ) {
        drawPresetOverwriteHoldButton( hit.insidePresetOverwriteHold );
      }

      if (!hit.insidePresetOverwriteHold) {
        presetSaveHoldStartTime = 0;

        return;
      }

      if ( presetSaveHoldStartTime == 0 ) {
        presetSaveHoldStartTime = now;
      }

      if ( !presetSaveHoldTriggered && now - presetSaveHoldStartTime >= PRESET_SAVE_HOLD_MS ) {
        presetSaveHoldTriggered = true;

        requestPresetOverwrite();

        return;
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_OPEN ) {
      touchState.lastTouchInsidePresetDeleteOpen = hit.insidePresetDeleteOpen;

      if ( hit.insidePresetDeleteOpen != touchState.presetDeleteOpenButtonVisualPressed ) {
        drawPresetDeleteOpenButton( hit.insidePresetDeleteOpen );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV, M5STACK_TOUCH_TARGET_PRESET_DELETE_NEXT ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV, hit.insidePresetDeletePrev, M5STACK_TOUCH_TARGET_PRESET_DELETE_NEXT, hit.insidePresetDeleteNext );

      touchState.lastTouchInsidePresetDeleteNav = insideSelectedButton;

      if ( insideSelectedButton != touchState.presetDeleteNavButtonVisualPressed ) {
        drawPresetDeleteNavigation( insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.presetDeleteNavButtonVisualPressed = insideSelectedButton;
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_HOLD ) {
      touchState.lastTouchInsidePresetDeleteHold = hit.insidePresetDeleteHold;

      if ( hit.insidePresetDeleteHold != touchState.presetDeleteHoldButtonVisualPressed ) {
        drawPresetDeleteHoldButton( hit.insidePresetDeleteHold );
      }

      if (!hit.insidePresetDeleteHold) {
        presetSaveHoldStartTime = 0;

        return;
      }

      if ( presetSaveHoldStartTime == 0 ) {
        presetSaveHoldStartTime = now;
      }

      if ( !presetSaveHoldTriggered && now - presetSaveHoldStartTime >= PRESET_SAVE_HOLD_MS ) {
        presetSaveHoldTriggered = true;

        requestPresetDelete();

        return;
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_OPEN ) {
      touchState.lastTouchInsidePresetBootOpen = hit.insidePresetBootOpen;

      if ( hit.insidePresetBootOpen != touchState.presetBootOpenButtonVisualPressed ) {
        drawPresetBootOpenButton( hit.insidePresetBootOpen );
      }

      return;
    }

    if ( M5StackDisplayTouchHelpers::isTouchTargetPair( touchState.touchTarget, M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV, M5STACK_TOUCH_TARGET_PRESET_BOOT_NEXT ) ) {
      bool insideSelectedButton = isSelectedTouchPairInside( M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV, hit.insidePresetBootPrev, M5STACK_TOUCH_TARGET_PRESET_BOOT_NEXT, hit.insidePresetBootNext );

      touchState.lastTouchInsidePresetBootNav = insideSelectedButton;

      if ( insideSelectedButton != touchState.presetBootNavButtonVisualPressed ) {
        drawPresetBootNavigation( insideSelectedButton ? touchState.touchTarget : M5STACK_TOUCH_TARGET_NONE );

        touchState.presetBootNavButtonVisualPressed = insideSelectedButton;
      }

      return;
    }

    if ( touchState.touchTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_HOLD ) {
      touchState.lastTouchInsidePresetBootHold = hit.insidePresetBootHold;

      if ( hit.insidePresetBootHold != touchState.presetBootHoldButtonVisualPressed ) {
        drawPresetBootHoldButton( hit.insidePresetBootHold );
      }

      if (!hit.insidePresetBootHold) {
        presetSaveHoldStartTime = 0;

        return;
      }

      if ( presetSaveHoldStartTime == 0 ) {
        presetSaveHoldStartTime = now;
      }

      if ( !presetSaveHoldTriggered && now - presetSaveHoldStartTime >= PRESET_SAVE_HOLD_MS ) {
        presetSaveHoldTriggered = true;

        requestPresetBootSetting();

        return;
      }

      return;
    }

    return;
    }

  // =========================================================
  // Touch Release action state
  //
  // Only one M5StackTouchTarget can own a gesture, so release-time
  // execution is represented by one action instead of parallel
  // boolean flags.
  // =========================================================

  enum TouchReleaseAction : uint8_t {
    TOUCH_RELEASE_ACTION_NONE = 0,
    TOUCH_RELEASE_ACTION_POWER,
    TOUCH_RELEASE_ACTION_BRIGHTNESS_SHORT,
    TOUCH_RELEASE_ACTION_EFFECT_STEP,
    TOUCH_RELEASE_ACTION_EFFECT_DETAIL,
    TOUCH_RELEASE_ACTION_COLOR_OPEN,
    TOUCH_RELEASE_ACTION_COLOR_SLOT_SELECT,
    TOUCH_RELEASE_ACTION_PRESET_OPEN,
    TOUCH_RELEASE_ACTION_BACK,
    TOUCH_RELEASE_ACTION_HUE_SHORT,
    TOUCH_RELEASE_ACTION_SATURATION_SHORT,
    TOUCH_RELEASE_ACTION_SPEED_SHORT,
    TOUCH_RELEASE_ACTION_INTENSITY_SHORT,
    TOUCH_RELEASE_ACTION_PALETTE_SHORT,
    TOUCH_RELEASE_ACTION_PRESET_SHORT,
    TOUCH_RELEASE_ACTION_PRESET_MANAGE_OPEN,
    TOUCH_RELEASE_ACTION_PRESET_SAVE_NEW,
    TOUCH_RELEASE_ACTION_PRESET_OVERWRITE_OPEN,
    TOUCH_RELEASE_ACTION_PRESET_OVERWRITE_STEP,
    TOUCH_RELEASE_ACTION_PRESET_DELETE_OPEN,
    TOUCH_RELEASE_ACTION_PRESET_DELETE_STEP,
    TOUCH_RELEASE_ACTION_PRESET_BOOT_OPEN,
    TOUCH_RELEASE_ACTION_PRESET_BOOT_STEP
  };

  struct ColorEditSnapshot {
    bool hueValid;
    CHSV32 hueHsv;
    uint8_t hueValue;
    uint8_t hueWhite;

    bool saturationValid;
    CHSV32 saturationHsv;
    uint8_t saturationValue;
    uint8_t saturationWhite;
  };

  TouchReleaseAction determineTouchReleaseAction( M5StackTouchTarget releasedTarget, unsigned long now ) {
    switch (releasedTarget) {
      case M5STACK_TOUCH_TARGET_POWER:
        return ( touchState.lastTouchInsidePower && now - touchState.lastTouchAction >= TOUCH_ACTION_COOLDOWN_MS ) ? TOUCH_RELEASE_ACTION_POWER : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_WIFI_RECOVERY:
        // Recovery AP is a hold-only gesture. Short release intentionally
        // performs no action.
        return TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN:
      case M5STACK_TOUCH_TARGET_BRIGHTNESS_UP:
        return ( touchState.lastTouchInsideBrightness && !touchState.brightnessRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_BRIGHTNESS_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_EFFECT_PREV:
      case M5STACK_TOUCH_TARGET_EFFECT_NEXT:
        return ( touchState.lastTouchInsideEffect && !touchState.effectRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_EFFECT_STEP : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_EFFECT_DETAIL:
        return touchState.lastTouchInsideEffectDetail ? TOUCH_RELEASE_ACTION_EFFECT_DETAIL : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_COLOR_OPEN:
        return touchState.lastTouchInsideColor ? TOUCH_RELEASE_ACTION_COLOR_OPEN : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_COLOR_SLOT_1:
      case M5STACK_TOUCH_TARGET_COLOR_SLOT_2:
      case M5STACK_TOUCH_TARGET_COLOR_SLOT_3:
        return (
          touchState.lastTouchInsideColorSlot &&
          !touchState.colorSlotHoldState.longPressActive
        )
          ? TOUCH_RELEASE_ACTION_COLOR_SLOT_SELECT
          : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_OPEN:
        return touchState.lastTouchInsidePresetOpen ? TOUCH_RELEASE_ACTION_PRESET_OPEN : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_BACK:
        return touchState.lastTouchInsideBack ? TOUCH_RELEASE_ACTION_BACK : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_HUE_DOWN:
      case M5STACK_TOUCH_TARGET_HUE_UP:
        return ( touchState.lastTouchInsideHue && !touchState.hueRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_HUE_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_SATURATION_DOWN:
      case M5STACK_TOUCH_TARGET_SATURATION_UP:
        return ( touchState.lastTouchInsideSaturation && !touchState.saturationRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_SATURATION_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_SPEED_DOWN:
      case M5STACK_TOUCH_TARGET_SPEED_UP:
        return ( touchState.lastTouchInsideSpeed && !touchState.speedRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_SPEED_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_INTENSITY_DOWN:
      case M5STACK_TOUCH_TARGET_INTENSITY_UP:
        return ( touchState.lastTouchInsideIntensity && !touchState.intensityRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_INTENSITY_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PALETTE_PREV:
      case M5STACK_TOUCH_TARGET_PALETTE_NEXT:
        return ( touchState.lastTouchInsidePalette && !touchState.paletteRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_PALETTE_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_PREV:
      case M5STACK_TOUCH_TARGET_PRESET_NEXT:
        return ( touchState.lastTouchInsidePresetNav && !touchState.presetRepeatState.longPressActive ) ? TOUCH_RELEASE_ACTION_PRESET_SHORT : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_MANAGE:
        return touchState.lastTouchInsidePresetManage ? TOUCH_RELEASE_ACTION_PRESET_MANAGE_OPEN : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_SAVE_NEW:
        return touchState.lastTouchInsidePresetSaveNew ? TOUCH_RELEASE_ACTION_PRESET_SAVE_NEW : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_OPEN:
        return touchState.lastTouchInsidePresetOverwriteOpen ? TOUCH_RELEASE_ACTION_PRESET_OVERWRITE_OPEN : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV:
      case M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_NEXT:
        return touchState.lastTouchInsidePresetOverwriteNav ? TOUCH_RELEASE_ACTION_PRESET_OVERWRITE_STEP : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_DELETE_OPEN:
        return touchState.lastTouchInsidePresetDeleteOpen ? TOUCH_RELEASE_ACTION_PRESET_DELETE_OPEN : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV:
      case M5STACK_TOUCH_TARGET_PRESET_DELETE_NEXT:
        return touchState.lastTouchInsidePresetDeleteNav ? TOUCH_RELEASE_ACTION_PRESET_DELETE_STEP : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_BOOT_OPEN:
        return touchState.lastTouchInsidePresetBootOpen ? TOUCH_RELEASE_ACTION_PRESET_BOOT_OPEN : TOUCH_RELEASE_ACTION_NONE;

      case M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV:
      case M5STACK_TOUCH_TARGET_PRESET_BOOT_NEXT:
        return touchState.lastTouchInsidePresetBootNav ? TOUCH_RELEASE_ACTION_PRESET_BOOT_STEP : TOUCH_RELEASE_ACTION_NONE;

      default:
        return TOUCH_RELEASE_ACTION_NONE;
    }
  }

  void releaseTouchVisualState( M5StackTouchTarget releasedTarget ) {
    if (
      releasedTarget == M5STACK_TOUCH_TARGET_WIFI_RECOVERY &&
      touchState.wifiRecoveryVisualPressed
    ) {
      drawMainNetworkStatusLine(
        getCurrentNetworkDisplayText()
      );

      touchState.wifiRecoveryVisualPressed = false;
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_POWER && touchState.powerButtonVisualPressed ) {
      drawPowerButton( bri > 0, false );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_BRIGHTNESS_DOWN || releasedTarget == M5STACK_TOUCH_TARGET_BRIGHTNESS_UP ) && touchState.brightnessButtonVisualPressed ) {
      drawBrightness( bri, M5STACK_TOUCH_TARGET_NONE );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_EFFECT_PREV || releasedTarget == M5STACK_TOUCH_TARGET_EFFECT_NEXT ) && touchState.effectButtonVisualPressed ) {
      drawEffect( getCurrentEffectMode(), M5STACK_TOUCH_TARGET_NONE );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_EFFECT_DETAIL && touchState.effectDetailVisualPressed ) {
      drawEffectDetailButton( getCurrentEffectMode(), false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_COLOR_OPEN && touchState.colorButtonVisualPressed ) {
      drawColorButton( getPrimaryColor(), false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_OPEN && touchState.presetOpenButtonVisualPressed ) {
      drawPresetOpenButton( false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_BACK && touchState.backButtonVisualPressed ) {
      drawBackButton( false );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_HUE_DOWN || releasedTarget == M5STACK_TOUCH_TARGET_HUE_UP ) && touchState.hueButtonVisualPressed ) {
      drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_SATURATION_DOWN || releasedTarget == M5STACK_TOUCH_TARGET_SATURATION_UP ) && touchState.saturationButtonVisualPressed ) {
      drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_SPEED_DOWN || releasedTarget == M5STACK_TOUCH_TARGET_SPEED_UP ) && touchState.speedButtonVisualPressed ) {
      drawSpeed( getCurrentSpeed(), M5STACK_TOUCH_TARGET_NONE );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_INTENSITY_DOWN || releasedTarget == M5STACK_TOUCH_TARGET_INTENSITY_UP ) && touchState.intensityButtonVisualPressed ) {
      drawIntensity( getCurrentIntensity(), M5STACK_TOUCH_TARGET_NONE );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_PALETTE_PREV || releasedTarget == M5STACK_TOUCH_TARGET_PALETTE_NEXT ) && touchState.paletteButtonVisualPressed ) {
      drawPalette( getCurrentPalette(), M5STACK_TOUCH_TARGET_NONE );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_PREV || releasedTarget == M5STACK_TOUCH_TARGET_PRESET_NEXT ) && touchState.presetNavButtonVisualPressed ) {
      drawPresetNavigation( M5STACK_TOUCH_TARGET_NONE );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_MANAGE && touchState.presetManageButtonVisualPressed ) {
      drawPresetManageButton( false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_SAVE_NEW && touchState.presetSaveNewButtonVisualPressed ) {
      drawPresetSaveNewButton( false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_SAVE_HOLD && touchState.presetSaveHoldButtonVisualPressed ) {
      drawPresetSaveHoldButton( false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_OPEN && touchState.presetOverwriteOpenButtonVisualPressed ) {
      drawPresetOverwriteOpenButton( false );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV || releasedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_NEXT ) && touchState.presetOverwriteNavButtonVisualPressed ) {
      drawPresetOverwriteNavigation( M5STACK_TOUCH_TARGET_NONE );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_HOLD && touchState.presetOverwriteHoldButtonVisualPressed ) {
      drawPresetOverwriteHoldButton( false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_OPEN && touchState.presetDeleteOpenButtonVisualPressed ) {
      drawPresetDeleteOpenButton( false );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV || releasedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_NEXT ) && touchState.presetDeleteNavButtonVisualPressed ) {
      drawPresetDeleteNavigation( M5STACK_TOUCH_TARGET_NONE );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_HOLD && touchState.presetDeleteHoldButtonVisualPressed ) {
      drawPresetDeleteHoldButton( false );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_OPEN && touchState.presetBootOpenButtonVisualPressed ) {
      drawPresetBootOpenButton( false );
    }

    if ( ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV || releasedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_NEXT ) && touchState.presetBootNavButtonVisualPressed ) {
      drawPresetBootNavigation( M5STACK_TOUCH_TARGET_NONE );
    }

    if ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_HOLD && touchState.presetBootHoldButtonVisualPressed ) {
      drawPresetBootHoldButton( false );
    }
  }

  void clearColorEditState() {
    hueEditValid = false;
    saturationEditValid = false;
  }

  void executeBackReleaseAction() {
    if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_BOOT ) {
      presetBootOperationState = PRESET_BOOT_OP_IDLE;
      presetBootTargetId = 0;
      presetBootTargetName = "NONE";
      presetBootResultStartMs = 0;

      drawPresetManageScreen();

      return;
    }

    if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_DELETE ) {
      presetDeleteOperationState = PRESET_DELETE_OP_IDLE;

      presetDeleteTargetId = 0;

      presetDeleteTargetName = "";

      presetDeleteWasCurrentPreset = false;
      presetDeleteWasBootPreset = false;

      presetDeleteResultStartMs = 0;

      drawPresetManageScreen();

      return;
    }

    if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_OVERWRITE ) {
      presetSaveOperationState = PRESET_SAVE_OP_IDLE;

      presetSaveOperationIsOverwrite = false;

      presetSaveCandidateId = 0;

      presetSaveCandidateName = "";

      presetOverwriteTargetId = 0;

      presetOverwriteTargetName = "";

      drawPresetManageScreen();

      return;
    }

    if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_SAVE ) {
      presetSaveOperationState = PRESET_SAVE_OP_IDLE;

      presetSaveCandidateId = 0;

      presetSaveCandidateName = "";

      drawPresetManageScreen();

      return;
    }

    if ( currentPage == SCREEN_PRESET && presetSubPage == PRESET_SUBPAGE_MANAGE ) {
      drawPresetScreen();

      return;
    }

    drawMainScreen( getCurrentNetworkDisplayText() );
  }

  void executeTouchReleaseAction( TouchReleaseAction action, M5StackTouchTarget releasedTarget, unsigned long now ) {
    switch (action) {
      case TOUCH_RELEASE_ACTION_POWER:
        touchState.lastTouchAction = now;

        toggleLedPowerFromTouch();

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_BRIGHTNESS_SHORT:
        brightnessShortPress( releasedTarget );

        drawBrightness( bri, M5STACK_TOUCH_TARGET_NONE );

        lastBrightnessValue = bri;

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_EFFECT_STEP:
        if ( releasedTarget == M5STACK_TOUCH_TARGET_EFFECT_PREV ) {
          applyEffectStep( -1 );
        }
        else {
          applyEffectStep( 1 );
        }

        drawEffect( getCurrentEffectMode(), M5STACK_TOUCH_TARGET_NONE );

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_EFFECT_DETAIL:
        clearColorEditState();

        drawEffectDetailScreen();
        return;

      case TOUCH_RELEASE_ACTION_COLOR_OPEN:
        clearColorEditState();

        drawColorScreen();
        return;

      case TOUCH_RELEASE_ACTION_COLOR_SLOT_SELECT:
        clearColorEditState();

        if ( releasedTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_1 ) {
          selectColorSlot( 0 );
        }
        else if ( releasedTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_2 ) {
          selectColorSlot( 1 );
        }
        else if ( releasedTarget == M5STACK_TOUCH_TARGET_COLOR_SLOT_3 ) {
          selectColorSlot( 2 );
        }

        return;

      case TOUCH_RELEASE_ACTION_PRESET_OPEN:
        clearColorEditState();

        presetNoEntries = false;

        drawPresetScreen();
        return;

      case TOUCH_RELEASE_ACTION_PRESET_MANAGE_OPEN:
        clearColorEditState();

        drawPresetManageScreen();
        return;

      case TOUCH_RELEASE_ACTION_PRESET_SAVE_NEW:
        clearColorEditState();

        if ( preparePresetSaveCandidate() ) {
          drawPresetSaveScreen();
        }
        else {
          drawPresetManageScreen();
        }
        return;

      case TOUCH_RELEASE_ACTION_PRESET_OVERWRITE_OPEN:
        clearColorEditState();

        if ( preparePresetOverwriteTarget() ) {
          drawPresetOverwriteScreen();
        }
        else {
          drawPresetManageScreen();
        }
        return;

      case TOUCH_RELEASE_ACTION_PRESET_OVERWRITE_STEP: {
        int direction = ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_OVERWRITE_PREV ) ? -1 : 1;

        if ( stepPresetOverwriteTarget( direction ) ) {
          drawPresetOverwriteScreen();
        }

        clearColorEditState();
        return;
      }

      case TOUCH_RELEASE_ACTION_PRESET_DELETE_OPEN:
        clearColorEditState();

        if ( preparePresetDeleteTarget() ) {
          drawPresetDeleteScreen();
        }
        else {
          drawPresetManageScreen();
        }
        return;

      case TOUCH_RELEASE_ACTION_PRESET_DELETE_STEP: {
        int direction = ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_DELETE_PREV ) ? -1 : 1;

        if ( stepPresetDeleteTarget( direction ) ) {
          drawPresetDeleteScreen();
        }

        clearColorEditState();
        return;
      }

      case TOUCH_RELEASE_ACTION_PRESET_BOOT_OPEN:
        clearColorEditState();

        if ( preparePresetBootTarget() ) {
          drawPresetBootScreen();
        }
        else {
          drawPresetManageScreen();
        }
        return;

      case TOUCH_RELEASE_ACTION_PRESET_BOOT_STEP: {
        int direction = ( releasedTarget == M5STACK_TOUCH_TARGET_PRESET_BOOT_PREV ) ? -1 : 1;

        if ( stepPresetBootTarget( direction ) ) {
          drawPresetBootScreen();
        }

        clearColorEditState();
        return;
      }

      case TOUCH_RELEASE_ACTION_BACK:
        clearColorEditState();

        executeBackReleaseAction();
        return;

      case TOUCH_RELEASE_ACTION_HUE_SHORT:
        hueShortPress( releasedTarget );

        drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

        drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_SATURATION_SHORT:
        saturationShortPress( releasedTarget );

        drawHue( logicalHueValue, M5STACK_TOUCH_TARGET_NONE );

        drawSaturation( logicalSaturationValue, M5STACK_TOUCH_TARGET_NONE );

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_SPEED_SHORT:
        speedShortPress( releasedTarget );

        drawSpeed( getCurrentSpeed(), M5STACK_TOUCH_TARGET_NONE );

        lastSpeedValue = getCurrentSpeed();

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_INTENSITY_SHORT:
        intensityShortPress( releasedTarget );

        drawIntensity( getCurrentIntensity(), M5STACK_TOUCH_TARGET_NONE );

        lastIntensityValue = getCurrentIntensity();

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_PALETTE_SHORT:
        paletteShortPress( releasedTarget );

        drawPalette( getCurrentPalette(), M5STACK_TOUCH_TARGET_NONE );

        lastPaletteValue = getCurrentPalette();

        clearColorEditState();
        return;

      case TOUCH_RELEASE_ACTION_PRESET_SHORT:
        presetShortPress( releasedTarget );

        drawPresetNavigation( M5STACK_TOUCH_TARGET_NONE );

        clearColorEditState();
        return;

      default:
        clearColorEditState();
        return;
    }
  }

  // =========================================================
  // Touch Release
  //
  // Release confirmation is unchanged. Action selection,
  // pressed-visual release, gesture reset, and action execution
  // are now separated into focused helpers.
  // =========================================================

  void handleTouchRelease( const M5StackTouchReleaseContext& context ) {
    M5StackTouchRuntimeState& touchState = context.state;
    const unsigned long now = context.now;

    if (!touchState.touchActive) {
      return;
    }

    if ( touchState.touchReleaseCandidate == 0 ) {
      touchState.touchReleaseCandidate = now;

      return;
    }

    if ( now - touchState.touchReleaseCandidate < TOUCH_RELEASE_CONFIRM_MS ) {
      return;
    }

    M5StackTouchTarget releasedTarget = touchState.touchTarget;

    TouchReleaseAction releaseAction = determineTouchReleaseAction( releasedTarget, now );

    releaseTouchVisualState( releasedTarget );

    ColorEditSnapshot editSnapshot = {
      hueEditValid, hueEditHsv, hueEditValue, hueEditWhite,
      saturationEditValid, saturationEditHsv, saturationEditValue, saturationEditWhite
    };

    resetTouchGesture();

    hueEditValid = editSnapshot.hueValid;
    hueEditHsv = editSnapshot.hueHsv;
    hueEditValue = editSnapshot.hueValue;
    hueEditWhite = editSnapshot.hueWhite;

    saturationEditValid = editSnapshot.saturationValid;
    saturationEditHsv = editSnapshot.saturationHsv;
    saturationEditValue = editSnapshot.saturationValue;
    saturationEditWhite = editSnapshot.saturationWhite;

    executeTouchReleaseAction( releaseAction, releasedTarget, now );
  }

  // =========================================================
  // Touch dispatcher
  //
  // Poll touch once, then route the same state through
  // Press / Hold / Release helpers.
  // =========================================================

  void handleTouch() {
    if (!touchReady) {
      return;
    }

    if (!readyScreenShown) {
      return;
    }

    if ( isPresetSaveBusy() || isPresetDeleteBusy() || isPresetBootBusy() ) {
      return;
    }

    unsigned long now = millis();

    if ( now - touchState.lastTouchPoll < TOUCH_POLL_MS ) {
      return;
    }

    touchState.lastTouchPoll = now;

    int16_t touchX = -1;

    int16_t touchY = -1;

    bool touching = readDisplayTouch( touchX, touchY );

    if (touching) {

      lastUserActivityMs = now;

      touchState.touchReleaseCandidate = 0;

      touchState.lastTouchX = touchX;

      touchState.lastTouchY = touchY;

      M5StackTouchHitState hit = buildTouchHitState( touchX, touchY );

      M5StackTouchFrameContext touchContext = {
        touchState,
        hit,
        now
      };

      handleTouchPress( touchContext );

      handleTouchHold( touchContext );

      return;
    }

    M5StackTouchReleaseContext releaseContext = {
      touchState,
      now
    };

    handleTouchRelease( releaseContext );
  }
