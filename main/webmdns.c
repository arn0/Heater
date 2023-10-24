#include "esp_system.h"
#include "mdns.h"

#include "webmdns.h"

void start_mdns_service(){
	esp_err_t err = mdns_init();	            						// initialize mDNS service
	if (err) {
		printf("MDNS Init failed: %d\n", err);
		return;
	}
	mdns_hostname_set("heater");                  				//set hostname
	mdns_instance_name_set("ESP32 Heater control");   				//set default instance
	mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);			//add our services

	//NOTE: services must be added before their properties can be set
	//use custom instance for the web server
	
	mdns_service_instance_name_set("_http", "_tcp", "Radiator");
}