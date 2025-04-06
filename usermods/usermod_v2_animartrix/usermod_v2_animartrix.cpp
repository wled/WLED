#include "wled.h"
#include <ANIMartRIX.h>

#warning WLED usermod: CC BY-NC 3.0 licensed effects by Stefan Petrick, include this usermod only if you accept the terms!
//========================================================================================================================


static const char _data_FX_mode_Module_Experiment10[] PROGMEM = "Z💡Module_Experiment10@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment9[] PROGMEM = "Z💡Module_Experiment9@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment8[] PROGMEM = "Z💡Module_Experiment8@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment7[] PROGMEM = "Z💡Module_Experiment7@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment6[] PROGMEM = "Z💡Module_Experiment6@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment5[] PROGMEM = "Z💡Module_Experiment5@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment4[] PROGMEM = "Z💡Module_Experiment4@Speed;;1;2";
static const char _data_FX_mode_Zoom2[] PROGMEM = "Z💡Zoom2@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment3[] PROGMEM = "Z💡Module_Experiment3@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment2[] PROGMEM = "Z💡Module_Experiment2@Speed;;1;2";
static const char _data_FX_mode_Module_Experiment1[] PROGMEM = "Z💡Module_Experiment1@Speed;;1;2";
static const char _data_FX_mode_Parametric_Water[] PROGMEM = "Z💡Parametric_Water@Speed;;1;2";
static const char _data_FX_mode_Water[] PROGMEM = "Z💡Water@Speed;;1;2";
static const char _data_FX_mode_Complex_Kaleido_6[] PROGMEM = "Z💡Complex_Kaleido_6@Speed;;1;2";
static const char _data_FX_mode_Complex_Kaleido_5[] PROGMEM = "Z💡Complex_Kaleido_5@Speed;;1;2";
static const char _data_FX_mode_Complex_Kaleido_4[] PROGMEM = "Z💡Complex_Kaleido_4@Speed;;1;2";
static const char _data_FX_mode_Complex_Kaleido_3[] PROGMEM = "Z💡Complex_Kaleido_3@Speed;;1;2";
static const char _data_FX_mode_Complex_Kaleido_2[] PROGMEM = "Z💡Complex_Kaleido_2@Speed;;1;2";
static const char _data_FX_mode_Complex_Kaleido[] PROGMEM = "Z💡Complex_Kaleido@Speed;;1;2";
static const char _data_FX_mode_SM10[] PROGMEM = "Z💡SM10@Speed;;1;2";
static const char _data_FX_mode_SM9[] PROGMEM = "Z💡SM9@Speed;;1;2";
static const char _data_FX_mode_SM8[] PROGMEM = "Z💡SM8@Speed;;1;2";
static const char _data_FX_mode_SM7[] PROGMEM = "Z💡SM7@Speed;;1;2";
static const char _data_FX_mode_SM6[] PROGMEM = "Z💡SM6@Speed;;1;2";
static const char _data_FX_mode_SM5[] PROGMEM = "Z💡SM5@Speed;;1;2";
static const char _data_FX_mode_SM4[] PROGMEM = "Z💡SM4@Speed;;1;2";
static const char _data_FX_mode_SM3[] PROGMEM = "Z💡SM3@Speed;;1;2";
static const char _data_FX_mode_SM2[] PROGMEM = "Z💡SM2@Speed;;1;2";
static const char _data_FX_mode_SM1[] PROGMEM = "Z💡SM1@Speed;;1;2";
static const char _data_FX_mode_Big_Caleido[] PROGMEM = "Z💡Big_Caleido@Speed;;1;2";
static const char _data_FX_mode_RGB_Blobs5[] PROGMEM = "Z💡RGB_Blobs5@Speed;;1;2";
static const char _data_FX_mode_RGB_Blobs4[] PROGMEM = "Z💡RGB_Blobs4@Speed;;1;2";
static const char _data_FX_mode_RGB_Blobs3[] PROGMEM = "Z💡RGB_Blobs3@Speed;;1;2";
static const char _data_FX_mode_RGB_Blobs2[] PROGMEM = "Z💡RGB_Blobs2@Speed;;1;2";
static const char _data_FX_mode_RGB_Blobs[] PROGMEM = "Z💡RGB_Blobs@Speed;;1;2";
static const char _data_FX_mode_Polar_Waves[] PROGMEM = "Z💡Polar_Waves@Speed;;1;2";
static const char _data_FX_mode_Slow_Fade[] PROGMEM = "Z💡Slow_Fade@Speed;;1;2";
static const char _data_FX_mode_Zoom[] PROGMEM = "Z💡Zoom@Speed;;1;2";
static const char _data_FX_mode_Hot_Blob[] PROGMEM = "Z💡Hot_Blob@Speed;;1;2";
static const char _data_FX_mode_Spiralus2[] PROGMEM = "Z💡Spiralus2@Speed;;1;2";
static const char _data_FX_mode_Spiralus[] PROGMEM = "Z💡Spiralus@Speed;;1;2";
static const char _data_FX_mode_Yves[] PROGMEM = "Z💡Yves@Speed;;1;2";
static const char _data_FX_mode_Scaledemo1[] PROGMEM = "Z💡Scaledemo1@Speed;;1;2";
static const char _data_FX_mode_Lava1[] PROGMEM = "Z💡Lava1@Speed;;1;2";
static const char _data_FX_mode_Caleido3[] PROGMEM = "Z💡Caleido3@Speed;;1;2";
static const char _data_FX_mode_Caleido2[] PROGMEM = "Z💡Caleido2@Speed;;1;2";
static const char _data_FX_mode_Caleido1[] PROGMEM = "Z💡Caleido1@Speed;;1;2";
static const char _data_FX_mode_Distance_Experiment[] PROGMEM = "Z💡Distance_Experiment@Speed;;1;2";
static const char _data_FX_mode_Center_Field[] PROGMEM = "Z💡Center_Field@Speed;;1;2";
static const char _data_FX_mode_Waves[] PROGMEM = "Z💡Waves@Speed;;1;2";
static const char _data_FX_mode_Chasing_Spirals[] PROGMEM = "Z💡Chasing_Spirals@Speed;;1;2";
static const char _data_FX_mode_Rotating_Blob[] PROGMEM = "Z💡Rotating_Blob@Speed;;1;2";


class ANIMartRIXMod:public ANIMartRIX {
	public:
	void initEffect() {
	  if (SEGENV.call == 0) {
		init(SEGMENT.virtualWidth(), SEGMENT.virtualHeight(), false);
	  }
	  float speedFactor = 1.0;
	  if (SEGMENT.speed < 128) {
		speedFactor = (float) map(SEGMENT.speed,   0, 127, 1, 10) / 10.0f;
	  }
	  else{
		speedFactor = map(SEGMENT.speed, 128, 255, 10, 100) / 10;
	  } 
	  setSpeedFactor(speedFactor);
	}
	void setPixelColor(int x, int y, rgb pixel) {
		SEGMENT.setPixelColorXY(x, y, CRGB(pixel.red, pixel.green, pixel.blue));
	}
	void setPixelColor(int index, rgb pixel) {
		SEGMENT.setPixelColor(index, CRGB(pixel.red, pixel.green, pixel.blue));
  	}

	// Add any extra custom effects not part of the ANIMartRIX libary here
};
ANIMartRIXMod anim;

uint16_t mode_Module_Experiment10() {
	anim.initEffect(); 
	anim.Module_Experiment10();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment9() { 
	anim.initEffect(); 
	anim.Module_Experiment9();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment8() { 
	anim.initEffect(); 
	anim.Module_Experiment8();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment7() { 
	anim.initEffect(); 
	anim.Module_Experiment7();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment6() { 
	anim.initEffect(); 
	anim.Module_Experiment6();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment5() { 
	anim.initEffect(); 
	anim.Module_Experiment5();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment4() { 
	anim.initEffect(); 
	anim.Module_Experiment4();
	return FRAMETIME;
}
uint16_t mode_Zoom2() { 
	anim.initEffect(); 
	anim.Zoom2();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment3() { 
	anim.initEffect(); 
	anim.Module_Experiment3();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment2() { 
	anim.initEffect(); 
	anim.Module_Experiment2();
	return FRAMETIME;
}
uint16_t mode_Module_Experiment1() { 
	anim.initEffect(); 
	anim.Module_Experiment1();
	return FRAMETIME;
}
uint16_t mode_Parametric_Water() { 
	anim.initEffect(); 
	anim.Parametric_Water();
	return FRAMETIME;
}
uint16_t mode_Water() { 
	anim.initEffect(); 
	anim.Water();
	return FRAMETIME;
}
uint16_t mode_Complex_Kaleido_6() { 
	anim.initEffect(); 
	anim.Complex_Kaleido_6();
	return FRAMETIME;
}
uint16_t mode_Complex_Kaleido_5() { 
	anim.initEffect(); 
	anim.Complex_Kaleido_5();
	return FRAMETIME;
}
uint16_t mode_Complex_Kaleido_4() { 
	anim.initEffect(); 
	anim.Complex_Kaleido_4();
	return FRAMETIME;
}
uint16_t mode_Complex_Kaleido_3() { 
	anim.initEffect(); 
	anim.Complex_Kaleido_3();
	return FRAMETIME;
}
uint16_t mode_Complex_Kaleido_2() { 
	anim.initEffect(); 
	anim.Complex_Kaleido_2();
	return FRAMETIME;
}
uint16_t mode_Complex_Kaleido() { 
	anim.initEffect(); 
	anim.Complex_Kaleido();
	return FRAMETIME;
}
uint16_t mode_SM10() { 
	anim.initEffect(); 
	anim.SM10();
	return FRAMETIME;
}
uint16_t mode_SM9() { 
	anim.initEffect(); 
	anim.SM9();
	return FRAMETIME;
}
uint16_t mode_SM8() { 
	anim.initEffect(); 
	anim.SM8();
	return FRAMETIME;
}
// uint16_t mode_SM7() { 
//	anim.initEffect(); 
// 	anim.SM7();
//
//	return FRAMETIME;
// }
uint16_t mode_SM6() { 
	anim.initEffect(); 
	anim.SM6();
	return FRAMETIME;
}
uint16_t mode_SM5() { 
	anim.initEffect(); 
	anim.SM5();
	return FRAMETIME;
}
uint16_t mode_SM4() { 
	anim.initEffect(); 
	anim.SM4();
	return FRAMETIME;
}
uint16_t mode_SM3() { 
	anim.initEffect(); 
	anim.SM3();
	return FRAMETIME;
}
uint16_t mode_SM2() { 
	anim.initEffect(); 
	anim.SM2();
	return FRAMETIME;
}
uint16_t mode_SM1() { 
	anim.initEffect(); 
	anim.SM1();
	return FRAMETIME;
}
uint16_t mode_Big_Caleido() { 
	anim.initEffect(); 	
	anim.Big_Caleido();
	return FRAMETIME;
}
uint16_t mode_RGB_Blobs5() { 
	anim.initEffect(); 	
	anim.RGB_Blobs5();
	return FRAMETIME;
}
uint16_t mode_RGB_Blobs4() { 
	anim.initEffect(); 
	anim.RGB_Blobs4();
	return FRAMETIME;
}
uint16_t mode_RGB_Blobs3() { 
	anim.initEffect(); 
	anim.RGB_Blobs3();
	return FRAMETIME;
}
uint16_t mode_RGB_Blobs2() { 
	anim.initEffect(); 
	anim.RGB_Blobs2();
	return FRAMETIME;
}
uint16_t mode_RGB_Blobs() { 
	anim.initEffect(); 
	anim.RGB_Blobs();
	return FRAMETIME;
}
uint16_t mode_Polar_Waves() { 
	anim.initEffect(); 
	anim.Polar_Waves();
	return FRAMETIME;
}
uint16_t mode_Slow_Fade() { 
	anim.initEffect(); 
	anim.Slow_Fade();
	return FRAMETIME;
}
uint16_t mode_Zoom() { 
	anim.initEffect(); 
	anim.Zoom();
	return FRAMETIME;
}
uint16_t mode_Hot_Blob() { 
	anim.initEffect(); 
	anim.Hot_Blob();
	return FRAMETIME;
}
uint16_t mode_Spiralus2() { 
	anim.initEffect(); 
	anim.Spiralus2();
	return FRAMETIME;
}
uint16_t mode_Spiralus() { 
	anim.initEffect(); 
	anim.Spiralus();
	return FRAMETIME;
}
uint16_t mode_Yves() { 
	anim.initEffect(); 
	anim.Yves();
	return FRAMETIME;
}
uint16_t mode_Scaledemo1() { 
	anim.initEffect(); 
	anim.Scaledemo1();
	return FRAMETIME;
}
uint16_t mode_Lava1() { 
	anim.initEffect(); 
	anim.Lava1();
	return FRAMETIME;
}
uint16_t mode_Caleido3() { 
	anim.initEffect(); 
	anim.Caleido3();
	return FRAMETIME;
}
uint16_t mode_Caleido2() { 
	anim.initEffect(); 
	anim.Caleido2();
	return FRAMETIME;
}
uint16_t mode_Caleido1() { 
	anim.initEffect(); 
	anim.Caleido1();
	return FRAMETIME;
}
uint16_t mode_Distance_Experiment() { 
	anim.initEffect(); 
	anim.Distance_Experiment();
	return FRAMETIME;
}
uint16_t mode_Center_Field() { 
	anim.initEffect(); 
	anim.Center_Field();
	return FRAMETIME;
}
uint16_t mode_Waves() { 
	anim.initEffect(); 
	anim.Waves();
	return FRAMETIME;
}
uint16_t mode_Chasing_Spirals() { 
	anim.initEffect(); 
	anim.Chasing_Spirals();
	return FRAMETIME;
}
uint16_t mode_Rotating_Blob() { 
	anim.initEffect(); 
	anim.Rotating_Blob();
	return FRAMETIME;
}


class AnimartrixUsermod : public Usermod {
  protected:
	bool enabled = false; //WLEDMM
	const char *_name; //WLEDMM
	bool initDone = false; //WLEDMM
	unsigned long lastTime = 0; //WLEDMM

  public:

    AnimartrixUsermod(const char *name, bool enabled) {
		this->_name = name;
		this->enabled = enabled;
	} //WLEDMM
	

    void setup() {

      strip.addEffect(255, &mode_Module_Experiment10, _data_FX_mode_Module_Experiment10);
      strip.addEffect(255, &mode_Module_Experiment9, _data_FX_mode_Module_Experiment9);
      strip.addEffect(255, &mode_Module_Experiment8, _data_FX_mode_Module_Experiment8);
      strip.addEffect(255, &mode_Module_Experiment7, _data_FX_mode_Module_Experiment7);
      strip.addEffect(255, &mode_Module_Experiment6, _data_FX_mode_Module_Experiment6);
      strip.addEffect(255, &mode_Module_Experiment5, _data_FX_mode_Module_Experiment5);
      strip.addEffect(255, &mode_Module_Experiment4, _data_FX_mode_Module_Experiment4);
      strip.addEffect(255, &mode_Zoom2, _data_FX_mode_Zoom2);
      strip.addEffect(255, &mode_Module_Experiment3, _data_FX_mode_Module_Experiment3);
      strip.addEffect(255, &mode_Module_Experiment2, _data_FX_mode_Module_Experiment2);
      strip.addEffect(255, &mode_Module_Experiment1, _data_FX_mode_Module_Experiment1);
      strip.addEffect(255, &mode_Parametric_Water, _data_FX_mode_Parametric_Water);
      strip.addEffect(255, &mode_Water, _data_FX_mode_Water);
      strip.addEffect(255, &mode_Complex_Kaleido_6, _data_FX_mode_Complex_Kaleido_6);
      strip.addEffect(255, &mode_Complex_Kaleido_5, _data_FX_mode_Complex_Kaleido_5);
      strip.addEffect(255, &mode_Complex_Kaleido_4, _data_FX_mode_Complex_Kaleido_4);
      strip.addEffect(255, &mode_Complex_Kaleido_3, _data_FX_mode_Complex_Kaleido_3);
      strip.addEffect(255, &mode_Complex_Kaleido_2, _data_FX_mode_Complex_Kaleido_2);
      strip.addEffect(255, &mode_Complex_Kaleido, _data_FX_mode_Complex_Kaleido);
      strip.addEffect(255, &mode_SM10, _data_FX_mode_SM10);
      strip.addEffect(255, &mode_SM9, _data_FX_mode_SM9);
      strip.addEffect(255, &mode_SM8, _data_FX_mode_SM8);
      // strip.addEffect(255, &mode_SM7, _data_FX_mode_SM7);
      strip.addEffect(255, &mode_SM6, _data_FX_mode_SM6);
      strip.addEffect(255, &mode_SM5, _data_FX_mode_SM5);
      strip.addEffect(255, &mode_SM4, _data_FX_mode_SM4);
      strip.addEffect(255, &mode_SM3, _data_FX_mode_SM3);
      strip.addEffect(255, &mode_SM2, _data_FX_mode_SM2);
      strip.addEffect(255, &mode_SM1, _data_FX_mode_SM1);
      strip.addEffect(255, &mode_Big_Caleido, _data_FX_mode_Big_Caleido);
      strip.addEffect(255, &mode_RGB_Blobs5, _data_FX_mode_RGB_Blobs5);
      strip.addEffect(255, &mode_RGB_Blobs4, _data_FX_mode_RGB_Blobs4);
      strip.addEffect(255, &mode_RGB_Blobs3, _data_FX_mode_RGB_Blobs3);
      strip.addEffect(255, &mode_RGB_Blobs2, _data_FX_mode_RGB_Blobs2);
      strip.addEffect(255, &mode_RGB_Blobs, _data_FX_mode_RGB_Blobs);
      strip.addEffect(255, &mode_Polar_Waves, _data_FX_mode_Polar_Waves);
      strip.addEffect(255, &mode_Slow_Fade, _data_FX_mode_Slow_Fade);
      strip.addEffect(255, &mode_Zoom, _data_FX_mode_Zoom);
      strip.addEffect(255, &mode_Hot_Blob, _data_FX_mode_Hot_Blob);
      strip.addEffect(255, &mode_Spiralus2, _data_FX_mode_Spiralus2);
      strip.addEffect(255, &mode_Spiralus, _data_FX_mode_Spiralus);
      strip.addEffect(255, &mode_Yves, _data_FX_mode_Yves);
      strip.addEffect(255, &mode_Scaledemo1, _data_FX_mode_Scaledemo1);
      strip.addEffect(255, &mode_Lava1, _data_FX_mode_Lava1);
      strip.addEffect(255, &mode_Caleido3, _data_FX_mode_Caleido3);
      strip.addEffect(255, &mode_Caleido2, _data_FX_mode_Caleido2);
      strip.addEffect(255, &mode_Caleido1, _data_FX_mode_Caleido1);
      strip.addEffect(255, &mode_Distance_Experiment, _data_FX_mode_Distance_Experiment);
      strip.addEffect(255, &mode_Center_Field, _data_FX_mode_Center_Field);
      strip.addEffect(255, &mode_Waves, _data_FX_mode_Waves);
      strip.addEffect(255, &mode_Chasing_Spirals, _data_FX_mode_Chasing_Spirals);
      strip.addEffect(255, &mode_Rotating_Blob, _data_FX_mode_Rotating_Blob);

      initDone = true;
    }

    void loop() {
      if (!enabled || strip.isUpdating()) return;

      // do your magic here
      if (millis() - lastTime > 1000) {
        //USER_PRINTLN("I'm alive!");
        lastTime = millis();
      }
    }

    void addToJsonInfo(JsonObject& root)
    {
      char myStringBuffer[16]; // buffer for snprintf()
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray infoArr = user.createNestedArray(FPSTR(_name));

      String uiDomString = F("Animartrix requires the Creative Commons Attribution License CC BY-NC 3.0");
      infoArr.add(uiDomString);
	}

    uint16_t getId()
    {
      return USERMOD_ID_ANIMARTRIX;
    }

};

static AnimartrixUsermod animartrix_module("Animartrix", false);
REGISTER_USERMOD(animartrix_module);

