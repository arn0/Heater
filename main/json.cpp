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
		this->doc["pf"] = heater_status.pf;
		this->doc["one_pwr"] = heater_status.one_gpio;
		this->doc["two_pwr"] = heater_status.two_gpio;
		this->doc["safe"] = heater_status.safe;
		this->doc["blue"] = heater_status.blue;

		auto schedule = this->doc["schedule"].to<JsonObject>();
		schedule["target"] = heater_status.schedule_target;
		schedule["base"] = heater_status.scheduled_base_target;
		schedule["is_day"] = heater_status.schedule_is_day;
		schedule["preheat"] = heater_status.preheat_active;
		schedule["minutes_to_next"] = heater_status.minutes_to_next_transition;
		schedule["override"] = heater_status.override_active;
		schedule["override_target"] = heater_status.override_target;
		schedule["override_until"] = static_cast<long long>( heater_status.override_expires );

		const heater_config_t *cfg = heater_config_get();
		if ( cfg ) {
			auto config = this->doc["config"].to<JsonObject>();
			char buffer[6];
			heater_config_string_from_minutes( cfg->day_start_minutes, buffer, sizeof( buffer ) );
			config["day_start"] = buffer;
			heater_config_string_from_minutes( cfg->night_start_minutes, buffer, sizeof( buffer ) );
			config["night_start"] = buffer;
			config["day_temp"] = cfg->day_temperature;
			config["night_temp"] = cfg->night_temperature;
			config["floor_temp"] = cfg->floor_temperature;
			config["night_enabled"] = cfg->night_enabled;
			config["preheat_min"] = cfg->preheat_min_minutes;
			config["preheat_max"] = cfg->preheat_max_minutes;
			config["warmup_rate"] = cfg->warmup_rate_c_per_min;
			config["stage_full"] = cfg->stage_full_delta;
			config["stage_single"] = cfg->stage_single_delta;
			config["stage_hold"] = cfg->stage_hold_delta;
			config["override_minutes"] = cfg->override_duration_minutes;
		}
		serializeJson( doc, this->json_string, sizeof( this->json_string ) );
		return ( this->json_string );
	}

	extern "C" char *json_update( void ) {
		return ( MyJsonObject.update() );
	}
}
