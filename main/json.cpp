#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <time.h>
#include <cmath>

#include "ArduinoJson.h"

#include "heater.h"
#include "json.h"
#include "config.h"

namespace MyJsonNamespace {
	MyJsonClass::MyJsonClass() {}

	char *MyJsonClass::update( void ) {
		time( &this->Jnow );
		this->doc["time"] = this->Jnow;
		this->doc["target"] = heater_status.target;
		this->doc["fnt"] = heater_status.fnt;
		this->doc["bck"] = heater_status.bck;
		this->doc["top"] = heater_status.top;
		this->doc["bot"] = heater_status.bot;
		this->doc["chip"] = heater_status.chip;
		this->doc["rem"] = heater_status.rem;
		if ( std::isfinite( heater_status.out ) ) {
			this->doc["out"] = heater_status.out;
		} else {
			this->doc["out"] = nullptr;
		}
		this->doc["voltage"] = heater_status.voltage;
		this->doc["current"] = heater_status.current;
		this->doc["power"] = heater_status.power;
		this->doc["energy"] = heater_status.energy;
		this->doc["one_pwr"] = heater_status.one_gpio;
		this->doc["two_pwr"] = heater_status.two_gpio;
		this->doc["safe"] = heater_status.safe;
		this->doc["blue"] = heater_status.blue;

		serializeJson( doc, this->json_string, sizeof( this->json_string ) );
		return ( this->json_string );
	}

	extern "C" char *json_update( void ) {
		return ( MyJsonObject.update() );
	}
}
