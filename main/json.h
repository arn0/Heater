#ifndef JSON_H
#define JSON_H

#include "heater.h"

#ifdef __cplusplus

namespace MyJsonNamespace {

	class MyJsonClass {
	public:
		JsonDocument doc;
		char json_string[1024];
		time_t Jnow;

		// Constructor
		MyJsonClass();

		// Fill string
		char *update( void );
	};
	static MyJsonClass MyJsonObject;
}

extern "C" {

#endif

char *json_update( void );

#ifdef __cplusplus
}
#endif

#endif // JSON_H
